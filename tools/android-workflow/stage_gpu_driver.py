#!/usr/bin/env python3
"""Stage an explicitly hashed Turnip ZIP only in a verified experimental app.

No installation, root, system-driver replacement, public/retained Debug writes,
or game-data clearing. Staging/system selection stops only the exact test app.
"""
import argparse
import datetime
import hashlib
import io
import json
from pathlib import Path
import re
import stat
import struct
import subprocess
import uuid
import zipfile

ROOT = "files/vulkan-driver"
PACKAGE = re.compile(r"io\.github\.banjotooierecompiled\.gputest\.g[0-9a-f]{12}")
LEAF = re.compile(r"[A-Za-z0-9_+.-]{1,128}")


def require(ok, message):
    if not ok:
        raise ValueError(message)


def digest(data):
    return hashlib.sha256(data).hexdigest()


def object_without_duplicates(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, "Duplicate JSON field: " + key)
        result[key] = value
    return result


def read_report(path):
    report = json.loads(path.read_text(), object_pairs_hook=object_without_duplicates)
    require(isinstance(report, dict), "Invalid build report")
    source = report.get("source_commit", "")
    require(isinstance(source, str) and re.fullmatch(r"[0-9a-f]{40}", source), "Invalid report source identity")
    package = report.get("package", "")
    require(isinstance(package, str) and PACKAGE.fullmatch(package) and package.endswith(".g" + source[:12]), "Not the exact isolated GPU test package")
    require(report.get("variant") == "android-gpu-driver-test" and report.get("min_api") == 28,
            "Build report is not a verified GPU-driver test")
    require(re.fullmatch(r"[0-9a-f]{64}", report.get("sha256", "")), "Invalid APK digest")
    apk_name = report.get("apk", "")
    require(apk_name == "Tooie-Recompiled-Android-arm64-" + source[:12] + "-GPU-Test.apk", "Wrong test APK filename")
    require(digest((path.parent / apk_name).read_bytes()) == report["sha256"], "Local APK does not match the verified report")
    return report


def read_driver(path, expected_sha, api):
    require(re.fullmatch(r"[0-9a-f]{64}", expected_sha or ""), "Supply the expected driver ZIP SHA-256")
    require(path.stat().st_size <= 64 * 1024 * 1024, "Driver ZIP exceeds size limit")
    data = path.read_bytes()
    require(digest(data) == expected_sha, "Driver ZIP SHA-256 mismatch")
    libraries = {}
    with zipfile.ZipFile(io.BytesIO(data)) as bundle:
        infos = bundle.infolist(); names = [i.filename for i in infos]
        require(infos and len(names) == len(set(names)) and len(names) <= 32, "Empty, duplicate or excessive ZIP entries")
        require(sum(i.file_size for i in infos) <= 160 * 1024 * 1024, "Unpacked driver exceeds size limit")
        for info in infos:
            name = info.filename
            require(LEAF.fullmatch(name) and not name.startswith(".") and ".." not in name and
                    stat.S_IFMT(info.external_attr >> 16) in (0, stat.S_IFREG), "Unsafe archive entry: " + name)
            require(not info.flag_bits & 1, "Encrypted driver ZIP is unsupported")
        require("meta.json" in names and bundle.getinfo("meta.json").file_size <= 16384, "Missing or oversized driver metadata")
        metadata = json.loads(bundle.read("meta.json"), object_pairs_hook=object_without_duplicates)
        require(isinstance(metadata, dict), "Invalid driver metadata object")
        minimum = metadata.get("minApi")
        require(metadata.get("schemaVersion") == 1 and type(minimum) is int and 1 <= minimum <= api,
                "Driver metadata or minimum Android API is incompatible")
        require(str(metadata.get("vendor", "")).lower() == "mesa" and "turnip" in str(metadata.get("name", "")).lower(),
                "This experiment accepts Mesa Turnip packages only")
        library = metadata.get("libraryName", "")
        require(isinstance(library, str) and library in names and library.endswith(".so"), "Driver libraryName is missing")
        for info in infos:
            if info.filename.endswith(".so"):
                require(64 <= info.file_size <= 128 * 1024 * 1024, "Driver library size is invalid")
                content = bundle.read(info)
                require(content[:6] == b"\x7fELF\x02\x01" and struct.unpack_from("<HH", content, 16) == (3, 183),
                        "Driver library is not an ARM64 ELF shared library: " + info.filename)
                libraries[info.filename] = content
        require(library in libraries, "Main driver library is not valid")
    return metadata, libraries, max(28, minimum)


class Device:
    def __init__(self, adb, serial):
        require(serial and not serial.startswith("-") and re.fullmatch(r"[A-Za-z0-9_.:-]+", serial), "Invalid device serial")
        self.prefix = [adb, "-s", serial]

    def call(self, *arguments, data=None):
        result = subprocess.run(self.prefix + list(arguments), input=data, stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE, timeout=120)
        require(result.returncode == 0, "ADB command failed: " + " ".join(arguments[:4]) + "\n" + result.stderr.decode(errors="replace")[:1000])
        return result.stdout

    def verify(self, report):
        require(PACKAGE.fullmatch(report["package"]), "Refusing access to a non-test app")
        require(self.call("get-state").strip() == b"device", "Device is not ready")
        api_text = self.call("shell", "getprop", "ro.build.version.sdk").decode().strip()
        require(re.fullmatch(r"[0-9]{2,3}", api_text), "Cannot read Android API level")
        api = int(api_text); require(api >= 28, "Test loader requires Android 9 or later")
        package = report["package"]
        paths = self.call("shell", "pm", "path", package).decode().splitlines()
        require(len(paths) == 1 and paths[0].startswith("package:"), "Install the exact single-APK GPU test first")
        path = paths[0][8:]
        require(re.fullmatch(r"/data/app/[A-Za-z0-9_./~+=-]+/base\.apk", path) and "/../" not in path, "Unexpected installed APK path")
        actual = self.call("shell", "sha256sum", path).decode().split()
        require(actual and actual[0] == report["sha256"], "Installed test APK differs from the verified build")
        private = self.call("exec-out", "run-as", package, "pwd").decode().strip()
        require(re.fullmatch(r"/data/(?:user/[0-9]+|data)/" + re.escape(package), private), "Private test-app access failed")
        self.package = package
        return api

    def private(self, *args, data=None, binary=False):
        require(PACKAGE.fullmatch(self.package), "Refusing access to a non-test app")
        return self.call("exec-in" if binary else "exec-out", "run-as", self.package, *args, data=data)

    def stop(self):
        require(PACKAGE.fullmatch(self.package), "Refusing to stop a non-test app")
        self.call("shell", "am", "force-stop", self.package)

    def put(self, path, data):
        require(re.fullmatch(re.escape(ROOT) + r"/(?:[0-9a-f]{32}/[A-Za-z0-9_+.-]{1,128}|selection-[0-9a-f]{32}\.tmp)", path)
                and ".." not in path.split("/")[-1], "Invalid private output path")
        self.private("dd", "of=" + path, "bs=65536", data=data, binary=True)
        actual = self.private("sha256sum", path).decode().split()
        require(actual and actual[0] == digest(data), "Remote staged file SHA-256 mismatch")
        self.private("chmod", "400", path)


def stage(device, report, driver_path, driver_sha, api):
    metadata, libraries, minimum = read_driver(driver_path, driver_sha, api)
    identity = uuid.uuid4().hex
    directory = ROOT + "/" + identity
    device.stop()
    device.private("mkdir", "-p", directory)
    device.private("chmod", "700", ROOT, directory)
    for name, data in libraries.items():
        device.put(directory + "/" + name, data)
    main = metadata["libraryName"]
    config = ("schema=1\ndirectory=" + identity + "\nlibrary=" + main + "\nmin_api=" + str(minimum) +
              "\npackage_sha256=" + driver_sha + "\nlibrary_sha256=" + digest(libraries[main]) + "\n").encode()
    pending = ROOT + "/selection-" + identity + ".tmp"
    device.put(pending, config)
    device.private("mv", pending, ROOT + "/selection.txt")
    require(device.private("cat", ROOT + "/selection.txt") == config, "Published selection does not match staged bytes")
    return {"source_commit": report["source_commit"], "package": report["package"], "apk_sha256": report["sha256"],
            "device_api": api, "driver_zip_sha256": driver_sha, "driver_metadata": metadata,
            "staged_libraries": {name: digest(data) for name, data in libraries.items()}, "directory": identity,
            "active_driver_verified": False, "device_gameplay_tested": False,
            "scope": "Staging only; inspect event=active and descriptor discovery in the new app log."}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--adb", default="adb")
    parser.add_argument("--serial", required=True)
    parser.add_argument("--build-report", type=Path, required=True)
    parser.add_argument("--driver-sha256")
    action = parser.add_mutually_exclusive_group(required=True)
    action.add_argument("--driver-zip", type=Path)
    action.add_argument("--system", action="store_true")
    action.add_argument("--collect", type=Path, metavar="NEW_LOG_FILE")
    options = parser.parse_args()
    report = read_report(options.build_report)
    device = Device(options.adb, options.serial)
    api = device.verify(report)
    if options.collect:
        require(not options.collect.exists(), "Choose a new log filename; existing evidence is preserved")
        log = device.private("cat", "files/data/banjotooi-debug.log")
        require(log and not log.startswith((b"cat:", b"run-as:")), "No app log yet; open the test app first")
        with options.collect.open("xb") as output:
            output.write(log)
        print("Saved test-app log:", options.collect, "SHA256:", digest(log))
        return
    if options.system:
        device.stop()
        # Preserve selection history. A missing selection is already system mode.
        result = subprocess.run(device.prefix + ["shell", "run-as", report["package"], "test", "-e", ROOT + "/selection.txt"],
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=30)
        require(result.returncode in (0, 1), "Cannot inspect current driver selection")
        if result.returncode == 0:
            device.private("mv", ROOT + "/selection.txt", ROOT + "/selection-disabled-" + uuid.uuid4().hex + ".txt")
        print("System driver selected for the next manual launch of", report["package"])
        return
    receipt = stage(device, report, options.driver_zip, options.driver_sha256, api)
    receipt["staged_at_utc"] = datetime.datetime.now(datetime.timezone.utc).isoformat()
    output = options.build_report.parent / ("GPU-driver-staging-" + receipt["directory"] + ".json")
    with output.open("x") as stream:
        json.dump(receipt, stream, indent=2); stream.write("\n")
    print("PASS: hashed driver staged in the isolated test app. Open it manually; device compatibility remains untested.")
    print("Receipt:", output)


if __name__ == "__main__":
    try:
        main()
    except (ValueError, OSError, subprocess.SubprocessError, zipfile.BadZipFile) as error:
        raise SystemExit("STOP: " + str(error))
