#!/usr/bin/env python3
"""Read-only checks for a signed Android release candidate. Never installs/publishes."""
import argparse
import hashlib
import json
import re
import shutil
import struct
import subprocess
import tempfile
import zipfile
from pathlib import Path

PACKAGE = "io.github.banjotooierecompiled"
ACTIVITY = PACKAGE + ".BanjoTooieSDLActivity"
LIBRARIES = {"libmain.so", "libSDL2.so"}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def digest(data):
    return hashlib.sha256(data).hexdigest()


def fingerprint(value):
    value = value.replace(":", "").lower()
    require(re.fullmatch(r"[0-9a-f]{64}", value), "Expected a SHA-256 certificate fingerprint")
    return value


def run_tool(tool, *args):
    try:
        result = subprocess.run([str(tool), *map(str, args)], stdin=subprocess.DEVNULL,
                                capture_output=True, text=True, timeout=45)
    except subprocess.TimeoutExpired as error:
        raise ValueError(f"{tool.name} timed out; no retry or installation attempted") from error
    require(result.returncode == 0, f"{tool.name} rejected the APK (exit {result.returncode})")
    require(len(result.stdout) < 2_000_000, f"Unexpectedly large {tool.name} output")
    return result.stdout


def one(pattern, text, label, flags=0):
    matches = re.findall(pattern, text, flags)
    require(len(matches) == 1, f"Missing or ambiguous {label}")
    return matches[0]


def manifest_identity(apk, tools):
    badging = run_tool(tools / "aapt", "dump", "badging", apk)
    package_line = one(r"^package: (.+)$", badging, "package identity", re.M)
    package = one(r"(?:^|\s)name='([^']+)'", package_line, "package name")
    code = one(r"(?:^|\s)versionCode='([0-9]+)'", package_line, "version code")
    name = one(r"(?:^|\s)versionName='([^']+)'", package_line, "version name")
    return {"package": package, "version_code": int(code), "version_name": name, "badging": badging}


def apk_identity(apk, tools):
    signature = run_tool(tools / "apksigner", "verify", "--verbose", "--print-certs", apk)
    signer_count = one(r"^Number of signers: (\d+)\s*$", signature, "signer count", re.M)
    require(signer_count == "1", "Exactly one current APK signer is required")
    certificate = fingerprint(one(r"^Signer #1 certificate SHA-256 digest: ([0-9a-fA-F:]+)\s*$",
                                  signature, "signing certificate", re.M))
    signer_dn = one(r"^Signer #1 certificate DN: (.+)$", signature, "certificate name", re.M)
    return {**manifest_identity(apk, tools), "certificate_sha256": certificate,
            "debug_certificate": bool(re.search(r"(?:^|,)\s*CN\s*=\s*Android Debug\s*(?:,|$)",
                                                  signer_dn, re.I))}


def check_release_manifest(identity, apk, tools, expected_version, expected_code):
    require(identity["package"] == PACKAGE, "Android package identity differs")
    require(identity["version_name"] == expected_version and identity["version_code"] == expected_code,
            "Candidate Android version differs")
    badging = identity["badging"]
    require(one(r"^application-label:'([^']*)'", badging, "application label", re.M) == "Tooie Recompiled",
            "Android application label differs")
    require(one(r"^launchable-activity: name='([^']+)'", badging, "launch activity", re.M) == ACTIVITY,
            "Android launch activity differs")
    require("sdkVersion:'24'" in badging and "targetSdkVersion:'35'" in badging, "Android API profile differs")
    require(not re.search(r"^application-debuggable\b", badging, re.M), "Candidate is debuggable")
    xml = run_tool(tools / "aapt", "dump", "xmltree", apk, "AndroidManifest.xml")
    for attribute in ("debuggable", "testOnly"):
        values = re.findall(r"A: android:" + attribute + r"(?:\([^)]*\))?=\(type 0x12\)(0x[0-9a-fA-F]+)", xml)
        require(len(values) <= 1 and all(int(value, 16) == 0 for value in values),
                f"Candidate has {attribute} enabled")
        mentions = len(re.findall(r"A: android:" + attribute + r"(?:\(|=)", xml))
        require(mentions == len(values), f"Unrecognised {attribute} manifest encoding")


def is_signature_entry(name):
    return bool(re.fullmatch(r"META-INF/(?:MANIFEST\.MF|[^/]+\.(?:SF|RSA|DSA|EC))", name, re.I))


def archive(apk):
    package = zipfile.ZipFile(apk)
    try:
        entries = package.infolist()
        require(len(entries) <= 10000, "Too many APK entries")
        names = [entry.filename for entry in entries]
        require(len(names) == len(set(names)), "Duplicate APK entries")
        require(sum(entry.file_size for entry in entries) <= 1_073_741_824,
                "APK expands beyond the inspection limit")
        require(not any(name.lower().endswith((".z64", ".n64", ".v64", ".exe", ".jks", ".p12", ".pfx",
                                               ".pk8", ".keystore", ".key", ".pem")) for name in names),
                "APK contains an unexpected ROM, desktop binary or signing material")
        require({"classes.dex", "AndroidManifest.xml", "resources.arsc"} <= set(names),
                "Incomplete APK")
        libs = {name for name in names if name.startswith("lib/") and not name.endswith("/")}
        require(libs == {"lib/arm64-v8a/" + name for name in LIBRARIES},
                "Expected only the two Android ARM64 native libraries")
        require(package.testzip() is None, "APK CRC failure")
        return package
    except Exception:
        package.close()
        raise


def check_payload(candidate, built, report, native_version):
    with archive(candidate) as candidate_zip, archive(built) as built_zip:
        for name in LIBRARIES:
            entry = "lib/arm64-v8a/" + name
            data = candidate_zip.read(entry)
            require(len(data) >= 20 and data[:6] == b"\x7fELF\x02\x01" and
                    struct.unpack_from("<HH", data, 16) == (3, 183),
                    f"{name} is not an AArch64 ELF64 shared library")
            require(data == built_zip.read(entry), f"{name} differs from the verified build")
            require(digest(data) == report["libraries"][name]["sha256"],
                    f"{name} does not match the build report")
            if name == "libmain.so":
                require(native_version.encode() in data, "Native release version stamp is missing")
        expected_assets = {name for name in built_zip.namelist() if name.startswith("assets/")}
        require(expected_assets == {name for name in candidate_zip.namelist() if name.startswith("assets/")},
                "Candidate asset inventory differs from the verified build")
        for name in expected_assets:
            require(candidate_zip.read(name) == built_zip.read(name), f"Candidate asset differs: {name}")
        if report.get("artifact_kind") == "unsigned-release-candidate":
            def payload(package):
                return {item.filename for item in package.infolist()
                        if not item.is_dir() and not is_signature_entry(item.filename)}
            expected = payload(built_zip)
            require(payload(candidate_zip) == expected, "Candidate unsigned payload inventory differs")
            for name in expected:
                require(candidate_zip.read(name) == built_zip.read(name),
                        f"Candidate unsigned payload differs: {name}")


def verify(candidate, built, build_report, tools, expected_version, expected_code,
           expected_certificate, source_commit, previous=None):
    require(re.fullmatch(r"[0-9a-f]{40}", source_commit), "Expected the complete source commit")
    require(1 <= expected_code <= 2_100_000_000, "Version code is outside the Android range")
    expected_certificate = fingerprint(expected_certificate)
    report = json.loads(build_report.read_text(encoding="utf-8"))
    require(isinstance(report, dict), "Build report must be a JSON object")
    require(report["source_commit"] == source_commit, "Build report belongs to a different source commit")
    require(report["native_version"] == expected_version, "Native and Android release versions must agree")
    require(isinstance(report.get("libraries"), dict) and set(report["libraries"]) == LIBRARIES,
            "Build report must identify both native libraries")
    require(all(isinstance(item, dict) and isinstance(item.get("sha256"), str)
                for item in report["libraries"].values()), "Malformed native library build report")
    # Inspect stable private copies, so all SDK/ZIP checks use the same bytes.
    with tempfile.TemporaryDirectory(prefix="tooie-release-check-") as temp:
        root = Path(temp)
        copies = {}
        for name, source in (("candidate.apk", candidate), ("built.apk", built), ("previous.apk", previous)):
            if source is not None:
                target = root / name
                shutil.copyfile(source, target)
                copies[name] = target
        candidate_copy, built_copy = copies["candidate.apk"], copies["built.apk"]
        require(digest(built_copy.read_bytes()) == report["sha256"], "Verified build APK hash differs")
        identity = apk_identity(candidate_copy, tools)
        require(identity["certificate_sha256"] == expected_certificate, "Unexpected release signing certificate")
        require(not identity["debug_certificate"], "A debug certificate is not a protected release signing identity")
        check_release_manifest(identity, candidate_copy, tools, expected_version, expected_code)
        check_payload(candidate_copy, built_copy, report, expected_version)
        previous_result = None
        if previous is not None:
            previous_identity = apk_identity(copies["previous.apk"], tools)
            require(previous_identity["package"] == PACKAGE, "Previous APK uses a different package")
            require(expected_code > previous_identity["version_code"], "Version code must increase for the update")
            require(previous_identity["certificate_sha256"] == expected_certificate,
                    "Previous APK has a different signing certificate; direct replacement is not verified. "
                    "Do not uninstall or reset app data")
            previous_result = {"sha256": digest(copies["previous.apk"].read_bytes()),
                               "version_code": previous_identity["version_code"],
                               "direct_update_metadata_compatible": True}
        return {"source_commit": source_commit, "apk_sha256": digest(candidate_copy.read_bytes()),
                "version_name": expected_version, "version_code": expected_code,
                "certificate_sha256": expected_certificate, "package": PACKAGE,
                "native_libraries_and_assets_match_verified_build": True,
                "all_unsigned_payload_matches": report.get("artifact_kind") == "unsigned-release-candidate",
                "previous_apk": previous_result, "install_performed": False,
                "device_data_preservation_tested": False, "distribution_terms_reviewed": False,
                "scope": "package checks only; not publication approval or a gameplay/upgrade test"}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("candidate", "built-apk", "build-report", "build-tools", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--source-commit", required=True)
    parser.add_argument("--version-name", required=True)
    parser.add_argument("--version-code", type=int, required=True)
    parser.add_argument("--certificate-sha256", required=True)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--previous-apk", type=Path)
    mode.add_argument("--new-install", action="store_true",
                      help="Inspect a fresh-install candidate; does not verify replacement of an installed development build")
    args = parser.parse_args()
    try:
        require(not args.output.exists(), "Output already exists; choose a new report path")
        report = verify(args.candidate, args.built_apk, args.build_report, args.build_tools,
                        args.version_name, args.version_code, args.certificate_sha256,
                        args.source_commit, args.previous_apk)
        with args.output.open("x", encoding="utf-8") as output:
            json.dump(report, output, indent=2)
            output.write("\n")
    except (ValueError, OSError, KeyError, zipfile.BadZipFile) as error:
        parser.exit(1, f"STOP: {error}\nNo installation or publishing action was attempted.\n")
    print("PASS: release-candidate package checks; no installation or publication performed")
    print(args.output)


if __name__ == "__main__":
    main()
