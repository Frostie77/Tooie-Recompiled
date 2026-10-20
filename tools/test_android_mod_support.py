#!/usr/bin/env python3
"""Host checks of production Android import/JNI/installer code; no ROM or device.

Uses the real runtime manifest, config and miniz consumers. It does not execute
guest mods, render replacement textures, or run an Android document provider.
"""
import argparse
import io
import json
import os
from pathlib import Path
import platform
import shutil
import struct
import subprocess
import tempfile
import warnings
import zipfile


def zip_bytes(entries):
    buffer = io.BytesIO()
    with zipfile.ZipFile(buffer, "w", zipfile.ZIP_DEFLATED) as archive:
        for name, data in entries:
            with warnings.catch_warnings():
                warnings.simplefilter("ignore", UserWarning) # Deliberate duplicate-entry negative fixture.
                archive.writestr(name, data)
    return buffer.getvalue()


def fixtures(directory):
    directory.mkdir(parents=True)
    manifest = {"game_id": "bt", "id": "normal_mod", "display_name": "Host fixture", "version": "1.0.0",
                "authors": ["Fixture"], "minimum_recomp_version": "1.0.0",
                "config_schema": {"options": [{"id": "enabled", "name": "Enabled", "type": "Bool", "default": True}]}}

    def mod(**changes):
        return zip_bytes([("mod.json", json.dumps(manifest | changes)), ("fixture.txt", "not executable guest code")])

    good = mod()
    (directory / "normal.nrm").write_bytes(good)
    (directory / "updates").mkdir()
    (directory / "updates/normal.nrm").write_bytes(mod(version="2.0.0"))
    (directory / "protected.nrm").write_bytes(mod(id="protected_mod"))
    (directory / "invalid.nrm").write_bytes(b"not a ZIP")
    for filename, changes in {
        "wrong_game.nrm": {"game_id": "bk"},
        "native.nrm": {"native_libraries": {"unverified": ["entry"]}},
        "future.nrm": {"minimum_recomp_version": "9.0.0"},
        "deprecated.nrm": {"id": "deprecated_mod"},
        "bad_id.nrm": {"id": "../bad"},
        "bad_config.nrm": {"config_schema": {"options": [{"id": "x", "name": "X", "type": "Bool", "default": "wrong type"}]}},
    }.items():
        (directory / filename).write_bytes(mod(**changes))
    for filename, entries in {
        "oversized_manifest.nrm": [("mod.json", " " * (1024 * 1024 + 1))],
        "oversized_database.rtz": [("rt64.json", " " * (16 * 1024 * 1024 + 1))],
        "deep.nrm": [("mod.json", '{"nested":' * 80 + "0" + "}" * 80)],
        "bad_manifest.nrm": [("mod.json", '{"id":123}')],
        "bad_json.nrm": [("mod.json", '{"game_id":')],
        "missing.nrm": [("readme.txt", "no manifest")],
        "invalid_texture.rtz": [("rt64.json", "invalid JSON")],
        "traversal.nrm": [("mod.json", json.dumps(manifest)), ("../outside.nrm", "leave private staging")],
        "duplicate.nrm": [("mod.json", json.dumps(manifest)), ("mod.json", json.dumps(manifest))],
        "textures.rtz": [("rt64.json", '{"textures":[]}')],
        "bundle.zip": [("nested/bundle_one.nrm", mod(id="bundle_one")), ("bundle_two.rtz", zip_bytes([("rt64.json", "{}")])), ("README.md", "fixture")],
        "traversal_bundle.zip": [("../outside.nrm", good)],
        "collision_bundle.zip": [("one/same.nrm", good), ("two/same.nrm", mod(id="other"))],
        "duplicate_id_bundle.zip": [("one.nrm", good), ("two.nrm", good)],
        "native_bundle.zip": [("normal.nrm", good), ("desktop.so", "not an Android library")],
        "empty_bundle.zip": [("README.md", "no mods")],
        "corrupt_bundle.zip": [("inner.nrm", b"broken nested archive")],
    }.items():
        (directory / filename).write_bytes(zip_bytes(entries))
    # A real POSIX symlink entry, not a filename-only approximation.
    output = io.BytesIO()
    with zipfile.ZipFile(output, "w") as archive:
        archive.writestr("mod.json", json.dumps(manifest))
        link = zipfile.ZipInfo("link"); link.create_system = 3; link.external_attr = 0o120777 << 16
        archive.writestr(link, "../outside")
    (directory / "symlink.nrm").write_bytes(output.getvalue())
    # Declared 256 MiB entries cross the total budget without allocating/inflating payloads.
    expanded = bytearray(zip_bytes([("mod.json", json.dumps(manifest)), *[(f"large-{index}", "") for index in range(17)]]))
    cursor = 0
    while True:
        cursor = expanded.find(b"PK\x01\x02", cursor)
        if cursor < 0:
            break
        name_length = struct.unpack_from("<H", expanded, cursor + 28)[0]
        name = expanded[cursor + 46:cursor + 46 + name_length]
        if name.startswith(b"large-"):
            struct.pack_into("<I", expanded, cursor + 24, 256 * 1024 * 1024)
        cursor += 46 + name_length
    (directory / "expanded_limit.nrm").write_bytes(expanded)
    one_asset = bytearray(zip_bytes([("mod.json", json.dumps(manifest)), ("oversized-asset", "")]))
    cursor = one_asset.find(b"PK\x01\x02", one_asset.find(b"PK\x01\x02") + 1)
    struct.pack_into("<I", one_asset, cursor + 24, 256 * 1024 * 1024 + 1)
    (directory / "oversized_entry.nrm").write_bytes(one_asset)
    # Empty entries keep this bounded (~6 MiB) while crossing the real entry-count limit.
    with zipfile.ZipFile(directory / "entry_limit.nrm", "w") as archive:
        for number in range(65537):
            archive.writestr(f"entry-{number}", b"")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--build-dir", type=Path, help="Retain a uniquely named test run here")
    parser.add_argument("--android-jar", type=Path, help="Also compile the actual Activity and packaged SDL Java classes")
    parser.add_argument("--jni-include", type=Path, help="Directory containing jni.h and its platform include")
    parser.add_argument("--tested-base-root", type=Path, help="Pinned 4a0a309/5db2d7f source for exact previous-picker/installer failure controls")
    parser.add_argument("--cxx", default=os.environ.get("CXX", "g++"))
    parser.add_argument("--cc", default=os.environ.get("CC", "gcc"))
    parser.add_argument("--no-sanitizers", action="store_true", help="Explicitly disable host ASan/UBSan")
    args = parser.parse_args()
    root = args.root.resolve()
    temporary = None
    if args.build_dir:
        args.build_dir.mkdir(parents=True, exist_ok=True)
        build = Path(tempfile.mkdtemp(prefix="mod-support-", dir=args.build_dir.resolve()))
    else:
        temporary = tempfile.TemporaryDirectory(prefix="tooie-mod-support-")
        build = Path(temporary.name)
    sequence = 0

    def run(command, label, env=None, expected=0):
        nonlocal sequence
        sequence += 1
        result = subprocess.run([str(value) for value in command], cwd=root, env=env,
                                capture_output=True, text=True, timeout=180)
        output = result.stdout + result.stderr
        (build / f"{sequence:02d}-{label}.log").write_text(output)
        if result.returncode != expected:
            raise RuntimeError(f"{label} exited {result.returncode}\n{output[-12000:]}")
        if "WARNING in native method" in output or "runtime error:" in output:
            raise RuntimeError(f"{label} emitted a JNI/sanitizer warning\n{output[-12000:]}")
        for line in output.splitlines():
            if line.startswith("PASS:"):
                print(line, flush=True)
        return result

    java = Path(shutil.which("java") or "java").resolve()
    javac = shutil.which("javac")
    if javac:
        compile_java = [javac]
    else:
        compiler_helper = build / "CompileJava.java"
        compiler_helper.write_text("import javax.tools.ToolProvider; public class CompileJava { public static void main(String[] a) { var c=ToolProvider.getSystemJavaCompiler(); if(c==null) throw new IllegalStateException(\"JDK compiler unavailable\"); System.exit(c.run(null,System.out,System.err,a)); } }")
        compile_java = [java, "--source", "17", compiler_helper]
    java_source = root / "android/app/src/main/java"
    helpers = java_source / "io/github/banjotooierecompiled"
    tests = root / "tests/java/io/github/banjotooierecompiled"
    run([*compile_java, "--release", "17", "-d", build / "classes", helpers / "RomImporter.java", helpers / "ModImporter.java",
         *[tests / name for name in ("RomImporterTest.java", "ModImporterTest.java", "BanjoTooieSDLActivity.java", "RomPickerJniTest.java")]], "java-host-compile")
    for name in ("RomImporterTest", "ModImporterTest"):
        run([java, "-ea", "-cp", build / "classes", "io.github.banjotooierecompiled." + name], name)
    jni = args.jni_include or java.parent.parent / "include"
    if not (jni / "jni.h").is_file():
        raise RuntimeError("JNI development headers missing; set --jni-include or use a complete JDK.")
    frontend = root / "lib/RecompFrontend/recompui"
    run([args.cxx, "-std=c++20", "-D__ANDROID__", "-fPIC", "-shared", "-pthread",
         "-I" + str(root / "tests/android_picker_headers"), "-I" + str(jni), "-I" + str(jni / "linux"),
         "-I" + str(root / "tests/desktop_picker_headers"), frontend / "src/util/android_file_dialog.cpp",
         frontend / "src/util/file.cpp", root / "tests/android_rom_picker_jni.cpp", "-o", build / "libtooie_picker_test.so"], "jni-build")
    run([java, "-ea", "-Xcheck:jni", "-Djava.library.path=" + str(build), "-cp", build / "classes",
         "io.github.banjotooierecompiled.RomPickerJniTest"], "jni-execution")
    if args.android_jar:
        run([*compile_java, "--release", "17", "-cp", args.android_jar.resolve(), "-d", build / "android-classes",
             *sorted(java_source.rglob("*.java"))], "android-sdk-java")
        print("PASS: actual Activity, import helpers and packaged SDL Java compile against Android SDK", flush=True)
    else:
        print("SKIP: actual Activity SDK compilation (provide --android-jar)", flush=True)

    includes = build / "include"; includes.mkdir()
    (includes / "miniz_export.h").write_text("#pragma once\n#define MINIZ_EXPORT\n")
    runtime = root / "lib/N64ModernRuntime"
    include_dirs = [root / "tests/android_mod_headers", includes, frontend / "src/composites",
                    runtime / "librecomp/include", runtime / "N64Recomp/include", runtime / "ultramodern/include",
                    runtime / "thirdparty", runtime / "thirdparty/concurrentqueue", runtime / "thirdparty/miniz"]
    include_flags = ["-I" + str(path) for path in include_dirs]
    common = ["-O0", "-g1", "-fno-omit-frame-pointer", "-ffunction-sections", "-fdata-sections"]
    sanitizer = [] if args.no_sanitizers else ["-fsanitize=address,undefined"]
    machine_flags = ["-msse4.1"] if platform.machine().lower() in ("x86_64", "amd64") else []
    environment = os.environ.copy()
    # Leak scanning needs /proc access unavailable in some hosts; these are ASan/UBSan checks.
    environment.setdefault("ASAN_OPTIONS", "detect_leaks=0")
    environment.setdefault("UBSAN_OPTIONS", "halt_on_error=1:print_stacktrace=1")
    run([args.cxx, "-std=c++20", *common, *sanitizer, root / "tests/android_mod_transaction.cpp", "-o", build / "transactions"], "transaction-build")
    run([build / "transactions", build / "transaction-fixture"], "transaction-execution", environment)

    # Compile the exact production ZIP/manifest consumers, excluding unrelated runtime startup.
    manifest_source = (runtime / "librecomp/src/mod_manifest.cpp").read_text()
    manifest_prefix = build / "manifest-consumer.cpp"
    manifest_prefix.write_text(manifest_source[:manifest_source.index("\nbool parse_mod_config_storage(")])
    recomp_source = (runtime / "librecomp/src/recomp.cpp").read_text()
    version = build / "version-consumer.cpp"
    version.write_text('#include <array>\n#include <charconv>\n#include "librecomp/game.hpp"\n' +
                       recomp_source[recomp_source.index("bool recomp::Version::from_string("):recomp_source.index("\nconst std::array<uint8_t, 4> first_rom_bytes")])
    sources = [manifest_prefix, version, runtime / "librecomp/src/config.cpp", runtime / "librecomp/src/config_option.cpp",
               frontend / "src/composites/android_mod_installer.cpp", root / "tests/android_mod_installer.cpp"]
    objects = []
    print("Compiling production archive/manifest/config consumers with host memory checks...", flush=True)
    for number, source in enumerate(sources):
        target = build / f"consumer-{number}.o"
        run([args.cxx, "-std=c++20", "-D__ANDROID__", *machine_flags, *common, *sanitizer, *include_flags, "-c", source, "-o", target], f"consumer-{number}")
        objects.append(target)
    # Strict C99 hides POSIX stdio unless the feature-test macros are set before
    # system headers. Keep implicit function declarations fatal on every GCC.
    # These flags belong to the four host miniz C objects, not the Android app.
    posix_c_flags = ["-std=c99", "-D_POSIX_C_SOURCE=200809L", "-D_FILE_OFFSET_BITS=64",
                     "-Werror=implicit-function-declaration"]
    contract = build / "miniz-posix-contract.c"
    contract.write_text("#include <stdio.h>\n#include <sys/types.h>\n"
                        "typedef char require_large_off_t[(sizeof(off_t) >= 8) ? 1 : -1];\n"
                        "off_t (*checked_tell)(FILE*) = &ftello;\n"
                        "int (*checked_seek)(FILE*, off_t, int) = &fseeko;\n")
    run([args.cc, *posix_c_flags, "-c", contract, "-o", build / "miniz-posix-contract.o"], "miniz-posix-declarations")
    print("PASS: strict C99 POSIX ftello/fseeko prototypes and 64-bit file offsets; implicit function declarations remain fatal", flush=True)
    for name in ("miniz.c", "miniz_zip.c", "miniz_tinfl.c", "miniz_tdef.c"):
        target = build / (name + ".o")
        run([args.cc, *posix_c_flags, *common, *sanitizer, *include_flags, "-c", runtime / "thirdparty/miniz" / name, "-o", target], name)
        objects.append(target)
    run([args.cxx, *sanitizer, "-Wl,--gc-sections", "-pthread", *objects, "-o", build / "installer"], "installer-link")
    archive_fixtures = build / "archives"
    fixtures(archive_fixtures)
    run([build / "installer", archive_fixtures, build / "installed/mods"], "installer-execution", environment)
    run([args.cxx, "-std=c++20", "-D__ANDROID__", *machine_flags, *common, *sanitizer, *include_flags,
         root / "tests/mod_texture_registration.cpp", "-o", build / "texture-registration"], "texture-registration-build")
    run([build / "texture-registration"], "texture-registration-execution", environment)

    # The desktop adapter remains the actual existing file.cpp/NFD response fixture.
    desktop_headers = build / "desktop-headers"; desktop_headers.mkdir()
    (desktop_headers / "SDL.h").write_text((root / "tests/android_picker_headers/SDL.h").read_text() + "\nusing Uint32 = uint32_t;\n")
    run([args.cxx, "-std=c++20", "-I" + str(desktop_headers), "-I" + str(root / "tests/desktop_picker_headers"),
         "-I" + str(root / "lib/rt64/src/contrib/nativefiledialog-extended/src/include"),
         frontend / "src/util/file.cpp", root / "tests/desktop_file_dialog.cpp", "-o", build / "desktop"], "desktop-adapter-build")
    run([build / "desktop"], "desktop-adapter-execution")
    if args.tested_base_root:
        base = args.tested_base_root.resolve()
        base_frontend = base / "lib/RecompFrontend/recompui"
        expected_blobs = {
            "src/util/file.cpp": "388b7d5c2a91679b8c1faa969b33cc7f27517c67",
            "src/composites/ui_mod_installer.cpp": "ee1d89978b2a7ae96e6253d5d045984e36f96d35",
            "src/composites/ui_mod_installer.h": "bf317133566929dc7d72b54a84acbdd235fbe15c",
        }
        import hashlib
        for path, expected_blob in expected_blobs.items():
            data = (base_frontend / path).read_bytes()
            actual_blob = hashlib.sha1(b"blob " + str(len(data)).encode() + b"\0" + data).hexdigest()
            if actual_blob != expected_blob:
                raise RuntimeError("Tested-base source identity differs: " + path)
        # file.cpp's old RmlUi include is unused by the dialog implementation.
        (includes / "RmlUi").mkdir(); (includes / "RmlUi/Core.h").write_text("#pragma once\n")
        base_library = build / "base-jni"; base_library.mkdir()
        run([args.cxx, "-std=c++20", "-D__ANDROID__", "-fPIC", "-shared", "-pthread",
             "-I" + str(root / "tests/android_picker_headers"), "-I" + str(jni), "-I" + str(jni / "linux"),
             "-I" + str(root / "tests/desktop_picker_headers"), "-I" + str(includes), "-I" + str(frontend / "src/util"),
             frontend / "src/util/android_file_dialog.cpp", base_frontend / "src/util/file.cpp",
             root / "tests/android_rom_picker_jni.cpp", "-o", base_library / "libtooie_picker_test.so"], "base-picker-build")
        route_java = build / "ModRouteControl.java"
        route_java.write_text('''package io.github.banjotooierecompiled;
public class ModRouteControl {
 public static void main(String[] a) throws Exception {
  java.nio.file.Files.createDirectories(java.nio.file.Path.of(a[0]));
  BanjoTooieSDLActivity activity = new BanjoTooieSDLActivity();
  RomPickerJniTest.init(activity, a[0]); RomPickerJniTest.beginMod();
  int calls=RomPickerJniTest.modCalls();
  System.out.println("Picker control: activity_opens="+activity.modOpens+" immediate_callbacks="+calls);
  RomPickerJniTest.stop(); System.exit(activity.modOpens==1 && calls==0 ? 0 : 2);
 }
}''')
        run([*compile_java, "--release", "17", "-cp", build / "classes", "-d", build / "classes", route_java], "route-control-compile")
        current_route = run([java, "-Djava.library.path=" + str(build), "-cp", build / "classes",
                             "io.github.banjotooierecompiled.ModRouteControl", build / "route-current"], "current-route-control")
        previous_route = run([java, "-Djava.library.path=" + str(base_library), "-cp", build / "classes",
                              "io.github.banjotooierecompiled.ModRouteControl", build / "route-base"], "base-route-control", expected=2)
        if "activity_opens=1 immediate_callbacks=0" not in current_route.stdout or "activity_opens=0 immediate_callbacks=1" not in previous_route.stdout:
            raise RuntimeError("Unexpected picker control observations")
        runtime_objects = objects[:4] + objects[6:]
        for old in (False, True):
            label = "base" if old else "current"
            extra = ["-DMOD_TEST_BASE", "-I" + str(base_frontend / "src/composites")] if old else []
            control = build / (label + "-installer-control.o")
            run([args.cxx, "-std=c++20", "-D__ANDROID__", *machine_flags, *common, *sanitizer, *extra, *include_flags,
                 "-c", root / "tests/android_mod_baseline.cpp", "-o", control], label + "-installer-control-build")
            installer_object = objects[4]
            if old:
                installer_object = build / "base-installer.o"
                run([args.cxx, "-std=c++20", "-D__ANDROID__", *machine_flags, *common, *sanitizer,
                     "-I" + str(base_frontend / "src/composites"), *include_flags, "-c", base_frontend / "src/composites/ui_mod_installer.cpp",
                     "-o", installer_object], "base-installer-build")
            executable = build / (label + "-installer-control")
            run([args.cxx, *sanitizer, "-Wl,--gc-sections", "-pthread", *runtime_objects, installer_object, control, "-o", executable], label + "-control-link")
            observed = run([executable, archive_fixtures, build / (label + "-control/mods")], label + "-installer-control", environment, expected=2 if old else 0)
            expected_observation = "accepted_wrong_game=1 wrote_outside_mod_directory=1" if old else "accepted_wrong_game=0 wrote_outside_mod_directory=0"
            if expected_observation not in observed.stdout:
                raise RuntimeError("Unexpected installer control observations")
        print("PASS: exact-base picker fails the selected-file requirement; old installer accepts a wrong-game mod and writes outside its mod directory in sacrificial fixtures; all three controls pass with the implementation", flush=True)
    else:
        print("SKIP: exact previous-source failure controls (provide --tested-base-root)", flush=True)
    print("PASS: Android mod support host suite. Device import, executable mods and actual texture output remain unverified.", flush=True)
    if args.build_dir:
        print("Logs: " + str(build), flush=True)
    if temporary:
        temporary.cleanup()


if __name__ == "__main__":
    main()
