#!/usr/bin/env python3
"""Prepare Android stable-feed metadata from a verified, protected-key-signed APK.

This writes a new local JSON file. Commit it to updates/android-stable.json only
after publishing the matching APK. It never installs an APK or publishes a feed.
"""
import argparse
import json
import re
import zipfile
from pathlib import Path

import verify_apk as release


def metadata(apk, verification, tools):
    report = json.loads(verification.read_text(encoding="utf-8"))
    require = release.require
    data = apk.read_bytes()
    digest = release.digest(data)
    require(report.get("apk_sha256") == digest, "APK hash differs from the verification report")
    require(report.get("all_unsigned_payload_matches") is True and
            report.get("native_libraries_and_assets_match_verified_build") is True,
            "The complete signed payload must have passed candidate verification")
    identity = release.apk_identity(apk, tools)
    identities = json.loads((Path(__file__).with_name("signing-identities.json")).read_text(encoding="utf-8"))
    certificate = release.fingerprint(identities["release"]["certificate_sha256"])
    require(identity["certificate_sha256"] == certificate == report.get("certificate_sha256"), "Release signer differs")
    require(identity["package"] == release.PACKAGE == report.get("package"), "Release package differs")
    version, code = identity["version_name"], identity["version_code"]
    require(isinstance(version, str) and len(version) <= 64 and
            re.fullmatch(r"(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)", version), "Expected a stable version")
    require(type(code) is int and 0 < code <= 2100000000 and code == report.get("version_code") and
            version == report.get("version_name"), "Android version differs from verification")
    release.check_release_manifest(identity, apk, tools, version, code)
    source = report.get("source_commit")
    require(isinstance(source, str) and re.fullmatch(r"[0-9a-f]{40}", source), "Invalid verified source commit")
    with zipfile.ZipFile(apk) as bundle:
        native = [n for n in bundle.namelist() if n.startswith("lib/") and n.endswith(".so")]
        require(native and all(n.startswith("lib/arm64-v8a/") for n in native), "APK is not ARM64 only")
    return {
        "schema_version": 1, "channel": "stable", "package": release.PACKAGE,
        "version_name": version, "version_code": code, "release_tag": "v" + version,
        "source_commit": source, "abis": ["arm64-v8a"], "min_sdk": 24,
        "apk_asset": f"Tooie-Recompiled-v{version}-Android-arm64.apk",
        "apk_sha256": digest, "apk_size": len(data), "certificate_sha256": certificate,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--apk", required=True, type=Path)
    parser.add_argument("--verification", required=True, type=Path)
    parser.add_argument("--build-tools", type=Path, default=Path.home() / "Android/Sdk/build-tools/36.0.0")
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    try:
        value = metadata(args.apk, args.verification, args.build_tools)
        with args.output.open("x", encoding="utf-8") as target:
            target.write(json.dumps(value, indent=2) + "\n")
    except (ValueError, OSError, KeyError, TypeError, zipfile.BadZipFile) as error:
        parser.exit(1, f"STOP: {error}\nNo feed or release was published.\n")
    print(f"PASS: verified Android stable metadata prepared: {args.output}")
    print("After publishing the exact APK, commit this file to updates/android-stable.json.")


if __name__ == "__main__":
    main()
