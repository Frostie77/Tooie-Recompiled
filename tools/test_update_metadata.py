#!/usr/bin/env python3
"""Feed generation rejects mismatched signed reports and package identities."""
import hashlib
import json
import sys
import tempfile
import unittest
import zipfile
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parent / "release"))
import prepare_update


class UpdateMetadata(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.apk = self.root / "signed.apk"
        with zipfile.ZipFile(self.apk, "w") as bundle:
            bundle.writestr("lib/arm64-v8a/libmain.so", b"fixture ELF")
        self.certificate = json.loads((Path(prepare_update.__file__).with_name("signing-identities.json")).read_text())["release"]["certificate_sha256"]
        self.report = dict(apk_sha256=hashlib.sha256(self.apk.read_bytes()).hexdigest(),
            version_name="1.0.1", version_code=4, certificate_sha256=self.certificate,
            package=prepare_update.release.PACKAGE, source_commit="a" * 40,
            all_unsigned_payload_matches=True, native_libraries_and_assets_match_verified_build=True)
        self.identity = dict(package=self.report["package"], version_name="1.0.1", version_code=4,
                             certificate_sha256=self.certificate, badging="SDK fixture")
        self.verification = self.root / "verification.json"

    def generate(self):
        self.verification.write_text(json.dumps(self.report))
        with patch.object(prepare_update.release, "apk_identity", return_value=self.identity), \
             patch.object(prepare_update.release, "check_release_manifest") as checked:
            result = prepare_update.metadata(self.apk, self.verification, self.root)
            checked.assert_called_once()
            return result

    def test_exact_published_identity_and_no_tokens_or_urls_from_report(self):
        result = self.generate()
        self.assertEqual(result["release_tag"], "v1.0.1")
        self.assertEqual(result["apk_asset"], "Tooie-Recompiled-v1.0.1-Android-arm64.apk")
        self.assertEqual(result["apk_size"], self.apk.stat().st_size)
        self.assertEqual(result["certificate_sha256"], self.certificate)
        self.assertEqual(result["channel"], "stable")

    def test_report_mutations(self):
        original = dict(self.report)
        for key, value in [("apk_sha256", "0" * 64), ("certificate_sha256", "0" * 64),
                           ("version_name", "2.0.0"), ("version_code", 5), ("package", "debug"),
                           ("source_commit", "main"), ("all_unsigned_payload_matches", False),
                           ("native_libraries_and_assets_match_verified_build", False)]:
            with self.subTest(key=key):
                self.report = {**original, key: value}
                with self.assertRaises(ValueError): self.generate()

    def test_sdk_identities_and_prerelease_rejected(self):
        original = dict(self.identity)
        for key, value in [("package", "io.github.banjotooierecompiled"), ("certificate_sha256", "0" * 64),
                           ("version_name", "1.0.1-beta"), ("version_code", 0)]:
            with self.subTest(key=key):
                self.identity = {**original, key: value}
                with self.assertRaises(ValueError): self.generate()

    def test_wrong_abi_rejected(self):
        with zipfile.ZipFile(self.apk, "a") as bundle: bundle.writestr("lib/x86_64/libmain.so", b"desktop")
        self.report["apk_sha256"] = hashlib.sha256(self.apk.read_bytes()).hexdigest()
        with self.assertRaisesRegex(ValueError, "ARM64 only"): self.generate()

    def test_signature_verification_failure_propagates(self):
        self.verification.write_text(json.dumps(self.report))
        with patch.object(prepare_update.release, "apk_identity", side_effect=ValueError("bad APK signature")):
            with self.assertRaisesRegex(ValueError, "signature"):
                prepare_update.metadata(self.apk, self.verification, self.root)

    def test_driver_release_uses_its_actual_launcher_and_process_roles(self):
        release = prepare_update.release
        self.report.update(driver_picker=True, version_name="1.1.0", version_code=5)
        self.identity.update(version_name="1.1.0", version_code=5, badging=(
            "sdkVersion:'24'\ntargetSdkVersion:'35'\napplication-label:'Tooie Recompiled'\n"
            "launchable-activity: name='" + release.DRIVER_ACTIVITY + "'\n"))
        lines = ["E: manifest (line=1)", "  E: application (line=2)"]
        for name, exported, suffix, process, launcher in (
                ("BanjoTooieGameActivity", "0x0", ".game", ":game", False),
                ("GraphicsDriverActivity", "0xffffffff", ".drivers", None, True),
                ("GraphicsDriverSettingsActivity", "0x0", ".drivers", None, False)):
            lines += ["    E: activity (line=3)",
                      f'      A: android:name="{release.DEVELOPMENT_PACKAGE}.{name}"',
                      f"      A: android:exported=(type 0x12){exported}",
                      f'      A: android:taskAffinity="{release.PACKAGE + suffix}"',
                      "      A: android:hardwareAccelerated=(type 0x12)0x0"]
            if process: lines += [f'      A: android:process="{process}"']
            if launcher:
                lines += ["      E: intent-filter (line=4)", "        E: action (line=5)",
                          '          A: android:name="android.intent.action.MAIN"',
                          "        E: category (line=6)", '          A: android:name="android.intent.category.LAUNCHER"']
        lines += ["    E: activity-alias (line=7)",
                  f'      A: android:name="{release.ACTIVITY}"',
                  f'      A: android:targetActivity="{release.DRIVER_ACTIVITY}"',
                  '      A: android:exported=(type 0x12)0xffffffff',
                  '      A: android:enabled=(type 0x12)0xffffffff']
        xml = "\n".join(lines)
        self.verification.write_text(json.dumps(self.report))
        with patch.object(release, "apk_identity", return_value=self.identity), patch.object(release, "run_tool", return_value=xml):
            result = prepare_update.metadata(self.apk, self.verification, self.root)
            self.assertEqual((result['release_tag'], result['version_code']), ('v1.1.0', 5))
        for changed in (xml.replace('android:process=":game"', 'android:process=":other"'), ""):
            with patch.object(release, "apk_identity", return_value=self.identity), patch.object(release, "run_tool", return_value=changed):
                with self.assertRaises(ValueError): prepare_update.metadata(self.apk, self.verification, self.root)
        self.report['driver_picker'] = False
        self.verification.write_text(json.dumps(self.report))
        with patch.object(release, "apk_identity", return_value=self.identity), patch.object(release, "run_tool", return_value=xml):
            with self.assertRaisesRegex(ValueError, 'launch activity'):
                prepare_update.metadata(self.apk, self.verification, self.root)


if __name__ == "__main__":
    unittest.main()
