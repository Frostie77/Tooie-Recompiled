#!/usr/bin/env python3
"""Check actual compile commands and packaged bytes before exporting an APK."""
import hashlib
import io
import json
import os
import re
import shlex
import shutil
import struct
import subprocess
import sys
import zipfile
from pathlib import Path
from PIL import Image


def require(condition, message):
    if not condition:
        raise ValueError(message)


def driver_test_mode():
    value = os.environ.get("TOOIE_ANDROID_DRIVER_TEST", "0")
    require(value in ("0", "1"), "invalid TOOIE_ANDROID_DRIVER_TEST")
    return value == "1"


def apk_profile(revision, driver_test):
    require(re.fullmatch(r"[0-9a-f]{40}", revision), "invalid source commit")
    if driver_test:
        return {"package": "io.github.banjotooierecompiled.gputest.g" + revision[:12],
                "application_label": "Tooie GPU Test " + revision[:12], "min_api": 28,
                "variant": "android-gpu-driver-test", "suffix": "GPU-Test"}
    return {"package": "io.github.banjotooierecompiled", "application_label": "Tooie Recompiled Debug",
            "min_api": 24, "variant": "debug", "suffix": "Debug"}


def check_native_entries(names, originals):
    require({name for name in names if name.startswith("lib/") and not name.endswith("/")} ==
            {"lib/arm64-v8a/" + name for name in originals},
            "unexpected or missing packaged native library/ABI")


def ndk_notices(ndk):
    paths = sorted(p for p in ndk.glob("NOTICE*") if p.is_file())
    require(paths, "NDK runtime/toolchain notices are missing")
    return b"\n".join(p.name.encode() + b"\n" + p.read_bytes() for p in paths)


def write_ndk_notices(source, work, ndk):
    path = work / "package/app/src/main/assets/program/assets/licenses/Android-NDK-NOTICE.txt"
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(ndk_notices(ndk))


def check_update_java(package):
    """Check the update entry points in the APK's executable DEX files."""
    names = package.namelist()
    require("classes.dex" in names, "APK primary classes.dex is missing")
    # D8 can split a debug APK into several DEX files even below the method
    # limit. Only root-level classes.dex/classesN.dex are app bytecode; an
    # asset, backup or nested file must not satisfy a missing-code check.
    dex_files = {name: package.read(name) for name in names
                 if re.fullmatch(r"classes(?:[2-9]|[1-9][0-9]+)?\.dex", name)}
    locations = {}
    for marker in (b"UpdateClient", b"UpdateManifest", b"AndroidUpdates", b"getUpdateState", b"requestUpdateCheck", b"openUpdateRelease"):
        matches = [name for name, dex in dex_files.items() if marker in dex]
        require(matches, "Android update Java class/entry point is missing: " + marker.decode())
        locations[marker.decode()] = matches
    print("PASS: Android update Java markers by DEX: " + json.dumps(locations, sort_keys=True))


def check_configuration(source, work, ndk):
    driver_test = driver_test_mode()
    cache = (work / "native/CMakeCache.txt").read_text()
    require("TOOIE_ANDROID_RUNTIME:BOOL=ON" in cache, "Android library target is disabled")
    require("TOOIE_VULKAN_DESCRIPTOR_ALLOCATION_PROBE:BOOL=OFF" in cache, "allocation probe must be OFF")
    require("TOOIE_ANDROID_DRIVER_TEST:BOOL=" + ("ON" if driver_test else "OFF") in cache,
            "driver-test build mode differs from requested variant")
    if driver_test:
        require("ANDROID_STL:STRING=c++_shared" in cache, "driver test requires one shared C++ runtime")
    require(re.search(r"^ANDROID_ABI:(?:STRING|UNINITIALIZED)=arm64-v8a$", cache, re.M), "wrong target ABI")
    commands = json.loads((work / "native/compile_commands.json").read_text())
    targets = [source / p for p in (
        "src/main.cpp", "src/recomp_api.cpp", "src/aiming_controls.cpp",
        "lib/rt64/src/contrib/plume/plume_vulkan.cpp",
        "lib/rt64/src/hle/rt64_present_queue.cpp",
        "lib/rt64/src/render/rt64_transform_processor.cpp")]
    if driver_test:
        targets.append(source / "src/android_vulkan_driver.cpp")
    for target in targets:
        matches = [entry for entry in commands if Path(entry["file"]).resolve() == target.resolve()]
        require(matches, f"missing actual compiler command: {target}")
        for entry in matches:
            args = entry.get("arguments") or shlex.split(entry["command"])
            joined = " ".join(args)
            driver_values = [arg for arg in args if arg.startswith("-DTOOIE_ANDROID_DRIVER_TEST")]
            require(driver_values == (["-DTOOIE_ANDROID_DRIVER_TEST=1"] if driver_test else []),
                    f"wrong driver loader compilation mode: {target}")
            require(str(ndk) in args[0] and "aarch64" in joined and "android24" in joined,
                    f"not an NDK ARM64/API24 command: {target}")
            for name in ("TOOIE_ANDROID_TIMING_TRACE", "TOOIE_ANDROID_GEOMETRY_TRACE",
                         "TOOIE_ANDROID_CORRECTION_TRACE", "TOOIE_VULKAN_QUEUE_ISOLATION_TEST"):
                values = [arg for arg in args if arg.startswith("-D" + name + "=")]
                require(values == ["-D" + name + "=0"], f"unexpected diagnostic define: {name}")
            for name in ("TOOIE_VULKAN_DESCRIPTOR_ALLOCATION_PROBE", "TOOIE_VULKAN_TRANSFER_CLEAR_TEST",
                         "TOOIE_VULKAN_ALLOW_ATTACHMENT_CLEAR_FALLBACK", "TOOIE_VULKAN_CLEAR_BARRIER_TEST"):
                require(not any(arg.startswith("-D" + name) for arg in args), f"unexpected control: {name}")
    print("PASS: actual ARM64/API24 compile commands; allocation probe OFF and diagnostic controls disabled")


def elf_check(data, name):
    require(data[:6] == b"\x7fELF\x02\x01", f"{name}: not little-endian ELF64")
    require(struct.unpack_from("<H", data, 18)[0] == 183, f"{name}: not AArch64")
    require(struct.unpack_from("<H", data, 16)[0] == 3, f"{name}: not a shared library")


def build_id(readelf, path):
    notes = subprocess.check_output([str(readelf), "-n", str(path)], text=True)
    matches = re.findall(r"Build ID: ([0-9a-f]+)", notes)
    require(len(matches) == 1, f"missing/ambiguous ELF Build ID: {path}")
    return matches[0]


def check_apk(source, work, ndk):
    check_configuration(source, work, ndk)
    driver_test = driver_test_mode()
    revision = subprocess.check_output(["git", "-C", str(source), "rev-parse", "HEAD"], text=True).strip()
    profile = apk_profile(revision, driver_test)
    apk = work / "package/app/build/outputs/apk/debug/app-debug.apk"
    toolbin = ndk / "toolchains/llvm/prebuilt/linux-x86_64/bin"
    verify = work / "verify"
    verify.mkdir(exist_ok=True)
    version = (work / "logs/version.txt").read_text().strip()
    originals = {"libmain.so": work / "native/libmain.so",
                 "libSDL2.so": work / "prefixes/sdl/lib/libSDL2.so"}
    if driver_test:
        originals.update({name: work / "native" / name for name in
                          ("libmain_hook.so", "libhook_impl.so", "libfile_redirect_hook.so", "libgsl_alloc_hook.so")})
        originals["libc++_shared.so"] = toolbin.parent / "sysroot/usr/lib/aarch64-linux-android/libc++_shared.so"
    libraries = {}
    with zipfile.ZipFile(apk) as package:
        require(package.testzip() is None, "APK ZIP CRC failure")
        names = package.namelist()
        require(len(names) == len(set(names)), "duplicate ZIP entries")
        check_native_entries(names, originals)
        if driver_test:
            require(package.read("assets/program/assets/licenses/Android-NDK-NOTICE.txt") == ndk_notices(ndk),
                    "shared C++ runtime SDK notice differs")
        require({"AndroidManifest.xml", "resources.arsc", "classes.dex"}.issubset(names), "incomplete APK")
        require(not any(name.lower().endswith((".z64", ".v64", ".n64", ".exe")) for name in names),
                "unexpected ROM or desktop binary in APK")
        for name, original in originals.items():
            data = package.read("lib/arm64-v8a/" + name)
            elf_check(data, name)
            packed = verify / name
            packed.write_bytes(data)
            stripped = verify / (name + ".stripped")
            subprocess.run([str(toolbin / "llvm-strip"), "--strip-unneeded", "-o", str(stripped), str(original)], check=True)
            require(data in (original.read_bytes(), stripped.read_bytes()), f"{name}: packaged library differs from built/stripped output")
            require(build_id(toolbin / "llvm-readelf", packed) == build_id(toolbin / "llvm-readelf", original),
                    f"{name}: Build ID differs")
            libraries[name] = {"sha256": hashlib.sha256(data).hexdigest(),
                               "build_id": build_id(toolbin / "llvm-readelf", packed)}
        main = (verify / "libmain.so").read_bytes()
        driver_markers = (b"custom-turnip-test", b"driver-sha256-mismatch", b"event=active mode=")
        require(all(marker in main for marker in driver_markers) if driver_test else
                all(marker not in main for marker in driver_markers), "driver loader code/variant mismatch")
        require(b"recomp_on_init" in main, "code-mod startup event registration is missing")
        require(b"Aiming Invert" in main and b"aiming_invert" in main, "saved aiming preference is missing")
        require(b"tooie_aiming_invert" in main, "guest aiming input bridge is missing")
        for marker in (b"automatic_update_checks", b"getUpdateState", b"requestUpdateCheck", b"canShowUpdateNotice", b"openUpdateRelease"):
            require(marker in main, "Android update UI/JNI bridge is missing: " + marker.decode())
        check_update_java(package)
        require(version.encode() in main, "native CI version stamp is missing")
        symbols = subprocess.check_output([str(toolbin / "llvm-nm"), "-D", "--defined-only", str(verify / "libmain.so")], text=True)
        for suffix in ("nativeFileDialogResult", "nativeModFileDialogResult", "nativePresentationLifecycle"):
            require("Java_io_github_banjotooierecompiled_BanjoTooieSDLActivity_" + suffix in symbols,
                    f"JNI export missing: {suffix}")
        # Verify every checked-in asset and launcher icon against the APK.
        for path in (source / "android/app/src/main/assets").rglob("*"):
            if path.is_file():
                entry = "assets/" + path.relative_to(source / "android/app/src/main/assets").as_posix()
                require(package.read(entry) == path.read_bytes(), f"packaged asset differs: {entry}")
        for path in (source / "android/app/src/main/res").glob("mipmap-*/*.png"):
            matches = [name for name in names if re.fullmatch(
                "res/" + re.escape(path.parent.name) + r"(?:-v\d+)?/" + re.escape(path.name), name)]
            require(len(matches) == 1, f"missing/ambiguous launcher icon: {path}")
            with Image.open(path) as expected, Image.open(io.BytesIO(package.read(matches[0]))) as packed:
                require(expected.size == packed.size and expected.convert("RGBA").tobytes() == packed.convert("RGBA").tobytes(),
                        f"launcher icon pixels differ: {path}")
    sdk = ndk.parent.parent
    tools = sdk / "build-tools/36.0.0"
    subprocess.run([str(tools / "apksigner"), "verify", "--verbose", str(apk)], check=True)
    badging = subprocess.check_output([str(tools / "aapt"), "dump", "badging", str(apk)], text=True)
    require("package: name='" + profile["package"] + "'" in badging, "Android package identity differs")
    require("application-label:'" + profile["application_label"] + "'" in badging, "Android application label differs")
    require("launchable-activity: name='io.github.banjotooierecompiled.BanjoTooieSDLActivity'" in badging,
            "launch activity differs")
    require("sdkVersion:'" + str(profile["min_api"]) + "'" in badging and "targetSdkVersion:'35'" in badging, "Android API profile differs")
    require("application-debuggable" in badging, "test APK must support private run-as evidence collection")
    require("uses-permission: name='android.permission.INTERNET'" in badging, "Android update network permission is missing")
    require("android.permission.REQUEST_INSTALL_PACKAGES" not in badging, "Release-page checks must not request package-install permission")
    output = work / "artifacts"
    output.mkdir(exist_ok=True)
    filename = "Tooie-Recompiled-Android-arm64-" + revision[:12] + "-" + profile["suffix"] + ".apk"
    shutil.copyfile(apk, output / filename)
    report = {"source_commit": revision, "native_version": version,
              "package": profile["package"], "application_label": profile["application_label"],
              "variant": profile["variant"], "min_api": profile["min_api"],
              "installation_strategy": "separate-apps",
              "apk": filename, "sha256": hashlib.sha256(apk.read_bytes()).hexdigest(),
              "ndk": ndk.name, "libraries": libraries, "signing": "CI debug key; not a release/update signing identity",
              "device_tested": False}
    (output / "build-report.json").write_text(json.dumps(report, indent=2) + "\n")
    (output / "SHA256SUMS").write_text(report["sha256"] + "  " + filename + "\n")
    if driver_test:
        shutil.copyfile(source / "tools/android-workflow/stage_gpu_driver.py", output / "stage_gpu_driver.py")
        shutil.copyfile(source / "docs/development/poco-gpu-driver-test.md", output / "GPU-TEST-INSTRUCTIONS.md")
    print("PASS: signed Android ARM64 APK, exact native libraries/Build IDs, JNI, assets, icon, name and API profile")


if __name__ == "__main__":
    command, source, work, ndk = sys.argv[1:]
    action = {"configuration": check_configuration, "apk": check_apk, "notices": write_ndk_notices}[command]
    action(Path(source).resolve(), Path(work).resolve(), Path(ndk).resolve())
