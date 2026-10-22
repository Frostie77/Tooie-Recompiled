#!/usr/bin/env python3
"""Host checks of production Android import/JNI/installer code; no ROM or device.

Uses the real runtime manifest, config and miniz consumers. It does not execute
guest mods, render replacement textures, or run an Android document provider.
"""
import argparse
import ctypes
import ctypes.util
import hashlib
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


def zstd_bytes(data):
    library = ctypes.CDLL(ctypes.util.find_library("zstd") or "libzstd.so")
    library.ZSTD_compressBound.argtypes = [ctypes.c_size_t]
    library.ZSTD_compressBound.restype = ctypes.c_size_t
    library.ZSTD_compress.argtypes = [ctypes.c_void_p, ctypes.c_size_t, ctypes.c_void_p, ctypes.c_size_t, ctypes.c_int]
    library.ZSTD_compress.restype = ctypes.c_size_t
    library.ZSTD_isError.argtypes = [ctypes.c_size_t]
    bound = library.ZSTD_compressBound(len(data))
    output = ctypes.create_string_buffer(bound)
    size = library.ZSTD_compress(output, bound, data, len(data), 3)
    if library.ZSTD_isError(size):
        raise RuntimeError("Could not compress the native Zstandard fixture")
    return output.raw[:size]


def zstd_zip(entries, payloads=None):
    """Small real mixed-method ZIPs; stdlib before Python 3.14 cannot write 93."""
    import zlib
    payloads = payloads or {}
    body = bytearray(); central = bytearray()
    for name, data, method in entries:
        data = data.encode() if isinstance(data, str) else data
        name_bytes = name.encode()
        compressed = (zstd_bytes(data) if method == 93 else
                      zlib.compress(data)[2:-4] if method == 8 else data)
        compressed = payloads.get(name, compressed)
        crc = zlib.crc32(data)
        needed = 63 if method == 93 else 20
        central += struct.pack("<IHHHHHHIIIHHHHHII", 0x02014b50, 20, needed, 0, method, 0, 0,
                               crc, len(compressed), len(data), len(name_bytes), 0, 0, 0, 0, 0, len(body)) + name_bytes
        body += struct.pack("<IHHHHHIIIHH", 0x04034b50, needed, 0, method, 0, 0,
                            crc, len(compressed), len(data), len(name_bytes), 0) + name_bytes + compressed
    return bytes(body + central + struct.pack("<IHHHHIIH", 0x06054b50, 0, 0, len(entries), len(entries), len(central), len(body), 0))


def mutate_entry(data, name, mutate_central=None, mutate_local=None):
    data = bytearray(data)
    cursor = struct.unpack_from("<I", data, len(data) - 6)[0]
    while data[cursor:cursor + 4] == b"PK\x01\x02":
        name_length, extra, comment = struct.unpack_from("<HHH", data, cursor + 28)
        if data[cursor + 46:cursor + 46 + name_length] == name.encode():
            local = struct.unpack_from("<I", data, cursor + 42)[0]
            if mutate_central: mutate_central(data, cursor)
            if mutate_local: mutate_local(data, local)
            return bytes(data)
        cursor += 46 + name_length + extra + comment
    raise AssertionError("Fixture entry not found: " + name)


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
        "hyphen_code.nrm": {"id": "Example-Code"},
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

    zstd_manifest = json.dumps(manifest | {"id": "zstd_fixture"})
    entries = [("mod.json", zstd_manifest, 8), ("rt64.json", '{"textures":[]}', 93), ("asset.dds", b"DDS fixture bytes", 93)]
    mixed = zstd_zip(entries)
    (directory / "zstd_mixed.rtz").write_bytes(mixed)
    (directory / "zstd_manifest.rtz").write_bytes(zstd_zip([(name, data, 93) for name, data, _ in entries]))
    (directory / "zstd_no_manifest.rtz").write_bytes(zstd_zip(entries[1:]))
    (directory / "zstd_empty_asset.rtz").write_bytes(zstd_zip([*entries, ("empty.txt", b"", 93)]))
    (directory / "zstd_bad_crc.rtz").write_bytes(mutate_entry(mixed, "asset.dds",
        lambda data, offset: struct.pack_into("<I", data, offset + 16, 0)))
    (directory / "zstd_truncated.rtz").write_bytes(zstd_zip(entries, {"asset.dds": zstd_bytes(b"DDS fixture bytes")[:-1]}))
    (directory / "zstd_trailing.rtz").write_bytes(zstd_zip(entries, {"asset.dds": zstd_bytes(b"DDS fixture bytes") + b"trailing invalid data"}))
    (directory / "zstd_multiframe.rtz").write_bytes(zstd_zip(entries, {"asset.dds": zstd_bytes(b"DDS fixture") + zstd_bytes(b" bytes")}))
    (directory / "zstd_wrong_size.rtz").write_bytes(mutate_entry(mixed, "asset.dds",
        lambda data, offset: struct.pack_into("<I", data, offset + 24, 18)))
    (directory / "zstd_oversized.rtz").write_bytes(mutate_entry(mixed, "asset.dds",
        lambda data, offset: struct.pack_into("<I", data, offset + 24, 256 * 1024 * 1024 + 1)))
    (directory / "zstd_local_method.rtz").write_bytes(mutate_entry(mixed, "asset.dds", mutate_local=
        lambda data, offset: struct.pack_into("<H", data, offset + 8, 8)))
    (directory / "zstd_local_flags.rtz").write_bytes(mutate_entry(mixed, "asset.dds", mutate_local=
        lambda data, offset: struct.pack_into("<H", data, offset + 6, 8)))
    for name, flag in (("encrypted", 1), ("patch", 32), ("strong", 64)):
        (directory / f"zstd_{name}.rtz").write_bytes(mutate_entry(mixed, "asset.dds",
            lambda data, offset, flag=flag: struct.pack_into("<H", data, offset + 8, flag),
            lambda data, offset, flag=flag: struct.pack_into("<H", data, offset + 6, flag)))
    (directory / "zstd_unsupported.rtz").write_bytes(mutate_entry(mixed, "asset.dds",
        lambda data, offset: struct.pack_into("<H", data, offset + 10, 99),
        lambda data, offset: struct.pack_into("<H", data, offset + 8, 99)))
    (directory / "zstd_bad_database.rtz").write_bytes(zstd_zip([entries[0], ("rt64.json", "invalid JSON", 93)]))
    (directory / "zstd_missing_database.rtz").write_bytes(zstd_zip([entries[0], entries[2]]))
    (directory / "zstd_bad_manifest.rtz").write_bytes(zstd_zip([("mod.json", zstd_manifest, 93), *entries[1:]], {"mod.json": b"not a Zstandard frame"}))
    (directory / "zstd_hyphen.rtz").write_bytes(zstd_zip([("mod.json", json.dumps(manifest | {"id": "Example-Textures"}), 93), *entries[1:]]))
    (directory / "hyphen_textures.rtz").write_bytes(zip_bytes([("mod.json", json.dumps(manifest | {"id": "Example-Textures"})), ("rt64.json", '{}')]))
    for index, bad_id in enumerate(("../bad", "bad\\name", "C:bad", "", "..", "nul\0id")):
        (directory / f"zstd_bad_id_{index}.rtz").write_bytes(zstd_zip([("mod.json", json.dumps(manifest | {"id": bad_id}), 93), *entries[1:]]))
    (directory / "truncated.nrm").write_bytes(good[:-8])
    (directory / "empty.nrm").write_bytes(zip_bytes([]))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--build-dir", type=Path, help="Retain a uniquely named test run here")
    parser.add_argument("--android-jar", type=Path, help="Also compile the actual Activity and packaged SDL Java classes")
    parser.add_argument("--jni-include", type=Path, help="Directory containing jni.h and its platform include")
    parser.add_argument("--tested-base-root", type=Path, help="Pinned 4a0a309/5db2d7f source for exact previous-picker/installer failure controls")
    parser.add_argument("--compression-base-root", type=Path, help="Exact a44bd5c source for independent pre-fix Zstandard failures")
    parser.add_argument("--rt64-pack", type=Path, help="Optional external pinned BT Reloaded v0.1.1 RTZ; never downloaded or redistributed by this test")
    parser.add_argument("--zstd-include", type=Path, help="Optional header directory when the host has only the Zstandard runtime")
    parser.add_argument("--zstd-link", default="-lzstd", help="Host Zstandard linker argument (default: -lzstd)")
    parser.add_argument("--cxx", default=os.environ.get("CXX", "g++"))
    parser.add_argument("--cc", default=os.environ.get("CC", "gcc"))
    parser.add_argument("--no-sanitizers", action="store_true", help="Explicitly disable host ASan/UBSan")
    args = parser.parse_args()
    root = args.root.resolve()
    pack = args.rt64_pack.resolve() if args.rt64_pack else None
    if pack and (pack.stat().st_size != 37474752 or hashlib.sha256(pack.read_bytes()).hexdigest() !=
                 "0f896281240ac22c4b1fa10e83757eb6d393fbbbe2121c81a2c38fbc960cdde9"):
        raise RuntimeError("External texture-pack identity differs from the pinned author release")
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
         *[tests / name for name in ("RomImporterTest.java", "ModImporterTest.java", "ModArchiveCopyTest.java", "BanjoTooieSDLActivity.java", "RomPickerJniTest.java")]], "java-host-compile")
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
    if args.zstd_include:
        include_flags.append("-I" + str(args.zstd_include.resolve()))
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
    run([args.cxx, *sanitizer, "-Wl,--gc-sections", "-pthread", *objects, args.zstd_link, "-o", build / "installer"], "installer-link")
    archive_fixtures = build / "archives"
    fixtures(archive_fixtures)
    run([java, "-ea", "-cp", build / "classes", "io.github.banjotooierecompiled.ModArchiveCopyTest",
         pack or archive_fixtures / "zstd_mixed.rtz", build / "zstd-java-copy"], "zstd-java-copy")
    run([build / "installer", archive_fixtures, build / "installed/mods", *([pack] if pack else [])], "installer-execution", environment)
    run([build / "installer", archive_fixtures, build / "zstd-control/mods", "--zstd-control"], "zstd-control", environment)
    run([build / "installer", archive_fixtures, build / "texture-id-control/mods", "--texture-id-control"], "texture-id-control", environment)

    # The real RT64 ZIP filesystem must consume the same bytes as the mod reader.
    texture_objects = []
    texture_sources = [root / "tests/mod_texture_archive.cpp", root / "lib/rt64/src/common/rt64_filesystem_zip.cpp",
                       root / "lib/rt64/src/common/rt64_mapped_file.cpp"]
    for number, source in enumerate(texture_sources):
        target = build / f"texture-archive-{number}.o"
        # RT64 itself is C++17; the runtime consumer requires C++20 char8_t.
        run([args.cxx, "-std=c++20" if number == 0 else "-std=c++17", "-DNDEBUG", *machine_flags, *common, *sanitizer,
             *include_flags, "-I" + str(root / "lib/rt64/src/common"),
             "-I" + str(root / "lib/rt64/src/contrib/hlslpp/include"), "-c", source, "-o", target], f"texture-archive-{number}")
        texture_objects.append(target)
    archive_objects = objects[:4] + objects[6:]
    run([args.cxx, *sanitizer, "-Wl,--gc-sections", "-pthread", *archive_objects, *texture_objects,
         args.zstd_link, "-o", build / "texture-archive"], "texture-archive-link")
    for name in ("zstd_mixed.rtz", "zstd_manifest.rtz", "zstd_no_manifest.rtz", "zstd_empty_asset.rtz", "zstd_multiframe.rtz"):
        run([build / "texture-archive", archive_fixtures / name], "texture-archive-" + name, environment)
    if pack:
        run([build / "texture-archive", pack], "texture-archive-external", environment)

    if args.compression_base_root:
        base = args.compression_base_root.resolve()
        pinned = {
            "android/app/src/main/java/io/github/banjotooierecompiled/ModImporter.java": "15eb5ac04c85bb3a09a851a832e156b005454aa5",
            "lib/RecompFrontend/recompui/src/composites/android_mod_installer.cpp": "cbfff2cb470a543979c6d996a0a0ca396d27a06d",
            "lib/N64ModernRuntime/librecomp/src/mod_manifest.cpp": "42a3c5c99d01ee9911d21ec310d314a9643dea68",
        }
        for path, expected in pinned.items():
            data = (base / path).read_bytes()
            if hashlib.sha1(b"blob " + str(len(data)).encode() + b"\0" + data).hexdigest() != expected:
                raise RuntimeError("Compression control source identity differs: " + path)
        run([*compile_java, "--release", "17", "-d", build / "base-classes",
             base / "android/app/src/main/java/io/github/banjotooierecompiled/ModImporter.java",
             tests / "ModArchiveCopyTest.java"], "base-compression-java-compile")
        failure = run([java, "-ea", "-cp", build / "base-classes", "io.github.banjotooierecompiled.ModArchiveCopyTest",
                       pack or archive_fixtures / "zstd_mixed.rtz", build / "base-java-copy"], "base-compression-java", expected=1)
        if "invalid CEN header (bad compression method" not in failure.stderr:
            raise RuntimeError("The exact parent did not reproduce the reported CEN error")
        old_installer = build / "base-compression-installer.o"
        run([args.cxx, "-std=c++20", "-D__ANDROID__", *machine_flags, *common, *sanitizer, *include_flags,
             "-c", base / "lib/RecompFrontend/recompui/src/composites/android_mod_installer.cpp",
             "-o", old_installer], "base-compression-installer-build")
        run([args.cxx, *sanitizer, "-Wl,--gc-sections", "-pthread", *archive_objects, old_installer, objects[5],
             args.zstd_link, "-o", build / "base-compression-installer"], "base-compression-installer-link")
        run([build / "base-compression-installer", archive_fixtures, build / "base-compression/mods", "--zstd-control"],
            "base-compression-installer", environment, expected=2)
        run([build / "base-compression-installer", archive_fixtures, build / "base-texture-id/mods", "--texture-id-control"],
            "base-texture-id", environment, expected=2)
        old_source = (base / "lib/N64ModernRuntime/librecomp/src/mod_manifest.cpp").read_text()
        old_prefix = build / "base-compression-manifest.cpp"
        old_prefix.write_text(old_source[:old_source.index("\nbool parse_mod_config_storage(")])
        old_manifest = build / "base-compression-manifest.o"
        run([args.cxx, "-std=c++20", "-D__ANDROID__", *machine_flags, *common, *sanitizer, *include_flags,
             "-c", old_prefix, "-o", old_manifest], "base-compression-reader-build")
        run([args.cxx, *sanitizer, "-Wl,--gc-sections", "-pthread", old_manifest, *archive_objects[1:], *texture_objects,
             args.zstd_link, "-o", build / "base-compression-reader"], "base-compression-reader-link")
        run([build / "base-compression-reader", pack or archive_fixtures / "zstd_mixed.rtz"],
            "base-compression-reader", environment, expected=2)
        print("PASS: four exact-parent controls reproduce Java CEN rejection, native compression and texture-ID rejection, and missing Zstandard reads independently", flush=True)
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
            run([args.cxx, *sanitizer, "-Wl,--gc-sections", "-pthread", *runtime_objects, installer_object, control, args.zstd_link, "-o", executable], label + "-control-link")
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
