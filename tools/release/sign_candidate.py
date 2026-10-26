#!/usr/bin/env python3
"""Sign a prepared candidate locally, then verify it. Never installs/publishes."""
import argparse
import json
import os
import shutil
import subprocess
import tempfile
import zipfile
from pathlib import Path

import verify_apk as release


def sign(candidate_dir, tools, keystore, identities, output, previous=None):
    require = release.require
    require(not output.exists(), "Signed output directory already exists; choose a new directory")
    report_file = candidate_dir / "build-report.json"
    report = json.loads(report_file.read_text(encoding="utf-8"))
    require(report.get("artifact_kind") == "unsigned-release-candidate", "Expected the unsigned candidate report")
    require(report.get("package") == release.PACKAGE and
            report.get("installation_strategy") == "separate-apps",
            "Expected the separate release app; rebuild older same-package candidates")
    name = report["apk"]
    require(Path(name).name == name and name.endswith(".apk"), "Invalid candidate APK filename")
    require(report["version_name"] == report["native_version"], "Native and Android versions differ")
    public = json.loads(identities.read_text(encoding="utf-8"))
    require(public.get("schema") == 1, "Unknown public signing-identity schema")
    certificate = release.fingerprint(public["release"]["certificate_sha256"])
    alias = public["release"]["alias"]
    require(isinstance(alias, str) and alias, "Missing release key alias")
    require(keystore.is_file(), "Local release keystore not found")
    if previous is not None:
        old = release.apk_identity(previous, tools)
        require(old["package"] == release.PACKAGE, "Previous APK package differs")
        require(old["certificate_sha256"] == certificate,
                "Previous APK uses a different key; a direct update is not verified. "
                "Keep the development app and its data; do not uninstall or reset it")
        require(report["version_code"] > old["version_code"], "Version code must increase for the update")
    with tempfile.TemporaryDirectory(prefix="tooie-local-sign-") as temp:
        root = Path(temp)
        unsigned, signed = root / "unsigned.apk", root / "signed.apk"
        shutil.copyfile(candidate_dir / name, unsigned)
        require(release.digest(unsigned.read_bytes()) == report["sha256"], "Unsigned candidate hash differs")
        release.check_release_manifest(release.manifest_identity(unsigned, tools), unsigned, tools,
                                       report["version_name"], report["version_code"], report.get("driver_picker") is True)
        stable_report = root / "build-report.json"
        stable_report.write_text(json.dumps(report), encoding="utf-8")
        # Inherit the local terminal for apksigner's password prompt. No password
        # is read by Python, put in arguments/environment, or captured in a log.
        try:
            subprocess.run([str(tools / "apksigner"), "sign", "--ks", str(keystore),
                            "--ks-key-alias", alias, "--v4-signing-enabled", "false",
                            "--out", str(signed), str(unsigned)], check=True, timeout=300)
        except (subprocess.CalledProcessError, subprocess.TimeoutExpired) as error:
            raise ValueError("Local signing failed or timed out; no retry") from error
        result = release.verify(signed, unsigned, stable_report, tools, report["version_name"],
                                report["version_code"], certificate, report["source_commit"], previous)
        require(result["all_unsigned_payload_matches"], "Complete unsigned payload was not verified")
        result["publication_approved"] = False
        output.mkdir(parents=True, exist_ok=False, mode=0o700)
        filename = name.removesuffix("-Unsigned.apk") + "-Signed-Candidate.apk"
        with (output / filename).open("xb") as target:
            target.write(signed.read_bytes())
        (output / "verification.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
        (output / "SHA256SUMS").write_text(result["apk_sha256"] + "  " + filename + "\n", encoding="utf-8")
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    here = Path(__file__).resolve().parent
    parser.add_argument("--candidate-dir", type=Path, default=here)
    parser.add_argument("--build-tools", type=Path, default=Path.home() / "Android/Sdk/build-tools/36.0.0")
    parser.add_argument("--keystore", type=Path,
                        default=Path.home() / ".local/share/tooie-recompiled/signing/tooie-release.p12")
    parser.add_argument("--identities", type=Path, default=here / "signing-identities.json")
    parser.add_argument("--output", type=Path)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--previous-apk", type=Path)
    mode.add_argument("--new-install", action="store_true",
                      help="Prepare the separate release app; existing development app data is not transferred")
    args = parser.parse_args()
    output = args.output or args.candidate_dir / "signed-candidate"
    os.umask(0o077)
    try:
        sign(args.candidate_dir, args.build_tools, args.keystore, args.identities, output, args.previous_apk)
    except (ValueError, OSError, KeyError, TypeError, zipfile.BadZipFile) as error:
        parser.exit(1, f"STOP: {error}\nNo installation or publishing was attempted.\n")
    print("PASS: locally signed candidate, expected certificate and complete unsigned payload verified")
    print("Separate release app verified. Installation and publication approval remain pending.")
    print(output)


if __name__ == "__main__":
    main()
