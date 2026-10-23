#!/usr/bin/env python3
"""Export an unsigned Android candidate; never signs, installs or publishes."""
import argparse
import json
import re
import shutil
import subprocess
import tempfile
import zipfile
from pathlib import Path

import verify_apk as release


def require_unsigned(apk, tools):
    try:
        result = subprocess.run([str(tools / "apksigner"), "verify", "--verbose", str(apk)],
                                stdin=subprocess.DEVNULL, capture_output=True, text=True, timeout=45)
    except subprocess.TimeoutExpired as error:
        raise ValueError("Unsigned signature check timed out; no retry") from error
    release.require(result.returncode == 1 and "DOES NOT VERIFY" in result.stdout + result.stderr,
                    "Expected an unsigned Gradle release APK, not a signed package or a tool failure")


def prepare(unsigned, built, build_report, tools, source_commit, version_code, output):
    require = release.require
    require(not output.exists(), "Candidate output already exists; choose a new directory")
    require(re.fullmatch(r"[0-9a-f]{40}", source_commit), "Expected the complete source commit")
    require(isinstance(version_code, int) and not isinstance(version_code, bool) and
            1 <= version_code <= 2_100_000_000, "Version code is outside the Android range")
    report = json.loads(build_report.read_text(encoding="utf-8"))
    require(report["source_commit"] == source_commit, "Verified build belongs to a different source commit")
    version = report["native_version"]
    require(re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+(?:-[0-9A-Za-z.-]+)?(?:\+[0-9A-Za-z.-]+)?", version),
            "Invalid native version")
    require(set(report["libraries"]) == release.LIBRARIES, "Expected both native libraries in the build report")
    with tempfile.TemporaryDirectory(prefix="tooie-unsigned-check-") as temp:
        root = Path(temp)
        unsigned_copy, built_copy = root / "unsigned.apk", root / "built.apk"
        shutil.copyfile(unsigned, unsigned_copy)
        shutil.copyfile(built, built_copy)
        require(release.digest(built_copy.read_bytes()) == report["sha256"], "Verified build APK hash differs")
        require_unsigned(unsigned_copy, tools)
        identity = release.manifest_identity(unsigned_copy, tools)
        release.check_release_manifest(identity, unsigned_copy, tools, version, version_code)
        release.check_payload(unsigned_copy, built_copy, report, version)
        name = "Tooie-Recompiled-Android-arm64-" + source_commit[:12] + "-Unsigned.apk"
        result = {**report, "artifact_kind": "unsigned-release-candidate", "apk": name,
                  "package": release.PACKAGE, "application_label": "Tooie Recompiled",
                  "development_package": release.DEVELOPMENT_PACKAGE, "activity": release.ACTIVITY,
                  "installation_strategy": "separate-apps", "shared_app_data": False,
                  "sha256": release.digest(unsigned_copy.read_bytes()), "version_name": version,
                  "version_code": version_code, "verified_debug_apk_sha256": report["sha256"],
                  "signing": "UNSIGNED; local protected release signing is still required",
                  "device_tested": False, "publication_approved": False}
        source = Path(__file__).resolve().parents[2]
        helpers = {name: source / "tools/release" / name for name in
                   ("sign_candidate.py", "verify_apk.py", "signing-identities.json")}
        helpers["COPYING"] = source / "COPYING"
        require(all(path.is_file() for path in helpers.values()), "Candidate helper or licence is missing")
        # All verification precedes output creation. An existing directory is never reused.
        output.mkdir(parents=True, exist_ok=False)
        shutil.copyfile(unsigned_copy, output / name)
        for helper, path in helpers.items():
            shutil.copyfile(path, output / helper)
        (output / "build-report.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
        (output / "README.txt").write_text(
            "Tooie Recompiled: unsigned Android ARM64 candidate\n\n"
            "This APK cannot be installed until signed. No release has been published.\n"
            "Review build-report.json and the source commit before local signing.\n"
            "This is the separate release app: " + release.PACKAGE + "\n"
            "The existing debug app remains " + release.DEVELOPMENT_PACKAGE + ".\n"
            "Both can stay installed. Saves, settings, mods and imported ROMs are\n"
            "private to each app; the release starts with its own data. No transfer\n"
            "or replacement of the debug installation is performed.\n\n"
            "For a fresh-install candidate, in Ubuntu run:\n"
            "  python3 sign_candidate.py --new-install\n\n"
            "The default key stays at ~/.local/share/tooie-recompiled/signing/tooie-release.p12.\n"
            "Enter the key password only in the local signing prompt. Never upload the key.\n"
            "Signing performs no installation or publishing. Back up your key separately.\n",
            encoding="utf-8")
        sums = "".join(release.digest(path.read_bytes()) + "  " + path.name + "\n"
                       for path in sorted(output.iterdir()) if path.is_file())
        (output / "SHA256SUMS").write_text(sums, encoding="utf-8")
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("unsigned-apk", "built-apk", "build-report", "build-tools", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--source-commit", required=True)
    parser.add_argument("--version-code", type=int, required=True)
    args = parser.parse_args()
    try:
        result = prepare(args.unsigned_apk, args.built_apk, args.build_report, args.build_tools,
                         args.source_commit, args.version_code, args.output)
    except (ValueError, OSError, KeyError, TypeError, zipfile.BadZipFile) as error:
        parser.exit(1, f"STOP: {error}\nNo signing, installation or publishing was attempted.\n")
    print("PASS: unsigned release manifest, native/asset provenance and candidate export")
    print(f"Candidate version: {result['version_name']} ({result['version_code']})")
    print(args.output)


if __name__ == "__main__":
    main()
