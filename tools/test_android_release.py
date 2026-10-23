#!/usr/bin/env python3
"""Release package rejection controls and optional real SDK/Gradle fixture."""
import argparse
import importlib.util
import json
import os
import struct
import subprocess
import tempfile
import unittest
import warnings
import zipfile
from pathlib import Path
from unittest.mock import patch

spec = importlib.util.spec_from_file_location("release_apk", Path(__file__).parent / "release/verify_apk.py")
release = importlib.util.module_from_spec(spec)
spec.loader.exec_module(release)
VERSION = "0.1.0-preview.1"
CERT = "12" * 32
COMMIT = "ab" * 20


def elf(extra=b""):
    header = bytearray(64)
    header[:6] = b"\x7fELF\x02\x01"
    struct.pack_into("<HH", header, 16, 3, 183)
    return bytes(header) + VERSION.encode() + extra


def zip_apk(path, changes=None, duplicate=False):
    entries = {"classes.dex": b"fixture dex", "resources.arsc": b"fixture resources",
               "AndroidManifest.xml": b"fixture manifest", "assets/licenses/font.txt": b"notice",
               "lib/arm64-v8a/libmain.so": elf(), "lib/arm64-v8a/libSDL2.so": elf(b"sdl")}
    entries.update(changes or {})
    with zipfile.ZipFile(path, "w") as package:
        for name, data in entries.items():
            package.writestr(name, data)
        if duplicate:
            with warnings.catch_warnings():
                warnings.simplefilter("ignore", UserWarning)
                package.writestr("classes.dex", b"duplicate")


class ReleaseChecks(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.built = self.root / "built.apk"
        self.candidate = self.root / "candidate.apk"
        self.previous = self.root / "previous.apk"
        for path in (self.built, self.candidate, self.previous):
            zip_apk(path)
        self.report = self.root / "build-report.json"
        self.build_report = {"source_commit": COMMIT, "native_version": VERSION,
                             "sha256": release.digest(self.built.read_bytes()),
                             "libraries": {name: {"sha256": release.digest(elf(b"sdl" if name == "libSDL2.so" else b""))}
                                           for name in release.LIBRARIES}}
        self.write_report()
        self.badging_extra = ""
        self.xml = "A: android:debuggable(0x0101000f)=(type 0x12)0x0\n"
        self.cert = CERT
        self.dn = "CN=Tooie test fixture"
        self.package = release.PACKAGE
        self.previous_code = 1

    def write_report(self):
        self.report.write_text(json.dumps(self.build_report))

    def sdk(self, tool, *args):
        # Real SDK execution is covered by --integration-work; this fixture
        # controls the external tool results while exercising actual ZIP checks.
        if tool.name == "apksigner":
            return ("Number of signers: 1\nSigner #1 certificate SHA-256 digest: " + self.cert +
                    "\nSigner #1 certificate DN: " + self.dn + "\n")
        if "xmltree" in args:
            return self.xml
        old = Path(args[-1]).name == "previous.apk"
        return (f"package: name='{self.package}' versionCode='{self.previous_code if old else 2}' versionName='{VERSION}'\n"
                "sdkVersion:'24'\ntargetSdkVersion:'35'\napplication-label:'Tooie Recompiled'\n"
                f"launchable-activity: name='{release.ACTIVITY}'\n" + self.badging_extra)

    def verify(self, previous=True, **kwargs):
        with patch.object(release, "run_tool", side_effect=self.sdk):
            return release.verify(self.candidate, self.built, self.report, self.root, VERSION, 2,
                                  kwargs.get("certificate", CERT), COMMIT,
                                  self.previous if previous else None)

    def test_valid_metadata_is_distinct_from_gameplay_or_distribution_approval(self):
        report = self.verify()
        self.assertTrue(report["previous_apk"]["direct_update_metadata_compatible"])
        self.assertTrue(report["native_libraries_and_assets_match_verified_build"])
        self.assertFalse(report["device_data_preservation_tested"])
        self.assertFalse(report["distribution_terms_reviewed"])
        self.assertFalse(report["install_performed"])
        self.assertIsNone(self.verify(previous=False)["previous_apk"])

    def test_unsigned_rejected(self):
        with patch.object(release, "run_tool", side_effect=ValueError("signature rejected")):
            with self.assertRaisesRegex(ValueError, "signature rejected"):
                release.apk_identity(self.candidate, self.root)

    def test_unexpected_signer_and_debug_key_rejected(self):
        with self.assertRaisesRegex(ValueError, "Unexpected release signing"):
            self.verify(certificate="34" * 32)
        self.dn = "CN=Android Debug, O=Android, C=US"
        with self.assertRaisesRegex(ValueError, "debug certificate"):
            self.verify()

    def test_no_upgrade_with_an_equal_or_lower_version_code(self):
        for code in (2, 3):
            self.previous_code = code
            with self.assertRaisesRegex(ValueError, "must increase"):
                self.verify()

    def test_previous_signer_mismatch_stops_without_data_reset(self):
        original = self.sdk
        def changed(tool, *args):
            text = original(tool, *args)
            if tool.name == "apksigner" and Path(args[-1]).name == "previous.apk":
                text = text.replace(CERT, "34" * 32)
            return text
        with patch.object(release, "run_tool", side_effect=changed):
            with self.assertRaisesRegex(ValueError, "Do not uninstall or reset"):
                release.verify(self.candidate, self.built, self.report, self.root, VERSION, 2,
                               CERT, COMMIT, self.previous)

    def test_debuggable_and_test_only_controls(self):
        self.badging_extra = "application-debuggable\n"
        with self.assertRaisesRegex(ValueError, "debuggable"):
            self.verify()
        self.badging_extra = ""
        for attribute in ("debuggable", "testOnly"):
            self.xml = f"A: android:{attribute}(0x0101000f)=(type 0x12)0xffffffff\n"
            with self.assertRaisesRegex(ValueError, attribute):
                self.verify()
        self.xml = "A: android:debuggable(0x0101000f)=\"unknown\"\n"
        with self.assertRaisesRegex(ValueError, "Unrecognised"):
            self.verify()

    def test_package_label_activity_and_version_mismatches(self):
        self.package = "wrong.package"
        with self.assertRaisesRegex(ValueError, "package identity"):
            self.verify()
        self.package = release.PACKAGE
        original = self.sdk
        for before, after in (("Tooie Recompiled", "Wrong label"), (release.ACTIVITY, "Wrong.Activity"),
                              ("versionCode='2'", "versionCode='3'"), ("sdkVersion:'24'", "sdkVersion:'23'")):
            with patch.object(release, "run_tool", side_effect=lambda tool, *args: original(tool, *args).replace(before, after)):
                with self.assertRaises(ValueError):
                    release.verify(self.candidate, self.built, self.report, self.root, VERSION, 2, CERT, COMMIT)

    def test_build_hash_source_and_native_version_are_bound(self):
        for key, value, message in (("sha256", "00" * 32, "APK hash"),
                                    ("source_commit", "cd" * 20, "different source"),
                                    ("native_version", "9.9.9", "versions must agree")):
            original = self.build_report[key]
            self.build_report[key] = value
            self.write_report()
            with self.assertRaisesRegex(ValueError, message):
                self.verify()
            self.build_report[key] = original

    def test_native_and_asset_mutations_are_rejected(self):
        for changes, message in (({"lib/arm64-v8a/libmain.so": elf(b"changed")}, "differs from"),
                                 ({"assets/licenses/font.txt": b"changed"}, "asset differs"),
                                 ({"assets/unexpected.txt": b"added"}, "inventory differs"),
                                 ({"lib/x86/libmain.so": elf()}, "Android ARM64")):
            zip_apk(self.candidate, changes)
            with self.assertRaisesRegex(ValueError, message):
                self.verify()

    def test_bad_native_headers_are_rejected(self):
        for data in (b"short", b"\x7fELF\x01\x01" + b"\x00" * 64):
            zip_apk(self.candidate, {"lib/arm64-v8a/libmain.so": data})
            with self.assertRaisesRegex(ValueError, "AArch64"):
                self.verify()

    def test_rom_credentials_and_duplicate_entries_are_rejected(self):
        for name in ("assets/game.z64", "assets/release.jks", "assets/private.key"):
            zip_apk(self.candidate, {name: b"unexpected"})
            with self.assertRaisesRegex(ValueError, "signing material"):
                self.verify()
        zip_apk(self.candidate, duplicate=True)
        with self.assertRaisesRegex(ValueError, "Duplicate"):
            self.verify()

    def test_sdk_timeout_and_error_are_not_retried(self):
        with patch.object(release.subprocess, "run", side_effect=subprocess.TimeoutExpired("apksigner", 45)) as call:
            with self.assertRaisesRegex(ValueError, "no retry"):
                release.run_tool(Path("apksigner"), "verify")
            self.assertEqual(call.call_count, 1)
        with patch.object(release.subprocess, "run", return_value=subprocess.CompletedProcess([], 1, "", "")) as call:
            with self.assertRaisesRegex(ValueError, "rejected the APK"):
                release.run_tool(Path("apksigner"), "verify")
            self.assertEqual(call.call_count, 1)

    def test_ambiguous_sdk_identity_is_rejected(self):
        original = self.sdk
        with patch.object(release, "run_tool", side_effect=lambda tool, *args: original(tool, *args) * 2):
            with self.assertRaisesRegex(ValueError, "ambiguous"):
                release.apk_identity(self.candidate, self.root)


def integration(work, tools):
    """Use actual Gradle, keytool, aapt and apksigner on the built Android app."""
    build_report = work / "artifacts/build-report.json"
    report = json.loads(build_report.read_text())
    built = work / "artifacts" / report["apk"]
    version = report["native_version"]
    package = work / "package"
    for option, reason in (("-PtooieVersionCode=0", "tooieVersionCode must be"),
                           ("-PtooieVersionName=invalid", "tooieVersionName must be")):
        control = subprocess.run(["gradle", "--no-daemon", "--console=plain", "-p", str(package),
                                  option, "help"], capture_output=True, text=True, timeout=120)
        release.require(control.returncode != 0 and reason in control.stdout + control.stderr,
                        "Gradle accepted an invalid release version or failed for a different reason")
    subprocess.run(["gradle", "--no-daemon", "--console=plain", "-p", str(package),
                    "-PtooieVersionCode=2", "-PtooieVersionName=" + version,
                    "assembleRelease", "assembleDebug"], check=True)
    unsigned = package / "app/build/outputs/apk/release/app-release-unsigned.apk"
    require = release.require
    require(unsigned.is_file(), "Gradle did not produce the unsigned release fixture")
    with tempfile.TemporaryDirectory(prefix="tooie-signing-fixture-") as temp:
        root = Path(temp)
        env = {**os.environ, "TOOIE_RELEASE_TEST_PASSWORD": "ephemeral-ci-fixture"}
        key = root / "fixture.jks"
        subprocess.run(["keytool", "-genkeypair", "-keystore", str(key), "-alias", "fixture",
                        "-keyalg", "RSA", "-keysize", "2048", "-validity", "2",
                        "-dname", "CN=Tooie CI Signing Fixture",
                        "-storepass:env", "TOOIE_RELEASE_TEST_PASSWORD",
                        "-keypass:env", "TOOIE_RELEASE_TEST_PASSWORD"], check=True, env=env)
        candidate, previous, debuggable = root / "candidate.apk", root / "previous.apk", root / "debuggable.apk"
        debug_variant = package / "app/build/outputs/apk/debug/app-debug.apk"
        for source, destination in ((unsigned, candidate), (built, previous), (debug_variant, debuggable)):
            subprocess.run([str(tools / "apksigner"), "sign", "--ks", str(key), "--ks-key-alias", "fixture",
                            "--ks-pass", "env:TOOIE_RELEASE_TEST_PASSWORD", "--key-pass", "env:TOOIE_RELEASE_TEST_PASSWORD",
                            "--out", str(destination), str(source)], check=True, env=env)
        identity = release.apk_identity(candidate, tools)
        result = release.verify(candidate, built, build_report, tools, version, 2,
                                identity["certificate_sha256"], report["source_commit"], previous)
        require(result["previous_apk"]["direct_update_metadata_compatible"], "Same-key upgrade fixture failed")
        for test_apk, old, message in ((unsigned, previous, "apksigner rejected"),
                                       (debuggable, previous, "Candidate is debuggable"),
                                       (candidate, built, "different signing certificate")):
            try:
                release.verify(test_apk, built, build_report, tools, version, 2,
                               identity["certificate_sha256"], report["source_commit"], old)
            except ValueError as error:
                require(message in str(error), "Wrong real-SDK rejection: " + str(error))
            else:
                raise ValueError("Missing real-SDK rejection: " + message)
        # The fixture is temporary, never exported with APK artifacts or logs.
    print("PASS: real AGP release variant, local temporary signing, manifest/version/native/assets checks, "
          "same-key update metadata and unsigned/debug/wrong-key controls; no device or production key used")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--integration-work", type=Path)
    parser.add_argument("--build-tools", type=Path)
    args = parser.parse_args()
    if args.integration_work:
        if not args.build_tools:
            parser.error("--build-tools is required for integration")
        integration(args.integration_work.resolve(), args.build_tools.resolve())
    else:
        unittest.main(argv=[__file__])
