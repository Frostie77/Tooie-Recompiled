#!/usr/bin/env python3
"""Release package rejection controls and optional real SDK/Gradle fixture."""
import argparse
import json
import os
import re
import struct
import subprocess
import sys
import tempfile
import unittest
import warnings
import zipfile
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).parent / "release"))
import verify_apk as release
import prepare_unsigned as preparation
import sign_candidate as signing
VERSION = "1.0.0"
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
                             "package": release.DEVELOPMENT_PACKAGE,
                             "application_label": "Tooie Recompiled Debug",
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
        self.assertEqual(report["package"], "io.github.banjotooierecompiled.release")
        self.assertEqual(report["development_package"], "io.github.banjotooierecompiled")
        self.assertEqual(report["activity"], "io.github.banjotooierecompiled.BanjoTooieSDLActivity")
        self.assertEqual(report["installation_strategy"], "separate-apps")
        self.assertFalse(report["shared_app_data"])
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
        for package in ("wrong.package", release.DEVELOPMENT_PACKAGE):
            self.package = package
            with self.assertRaisesRegex(ValueError, "package identity"):
                self.verify()
        self.package = release.PACKAGE
        original = self.sdk
        for before, after in (("Tooie Recompiled", "Wrong label"), (release.ACTIVITY, "Wrong.Activity"),
                              (release.ACTIVITY, release.PACKAGE + ".BanjoTooieSDLActivity"),
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
        for name in ("assets/game.z64", "assets/release.jks", "assets/private.key", "assets/release.p12",
                     "assets/release.pfx", "assets/private.pk8"):
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

    def test_every_unsigned_payload_member_is_bound(self):
        self.build_report["artifact_kind"] = "unsigned-release-candidate"
        self.write_report()
        for name in ("classes.dex", "resources.arsc", "AndroidManifest.xml"):
            zip_apk(self.candidate, {name: b"altered"})
            with self.assertRaisesRegex(ValueError, "unsigned payload differs"):
                self.verify()
        zip_apk(self.candidate, {"res/unexpected.bin": b"added"})
        with self.assertRaisesRegex(ValueError, "payload inventory"):
            self.verify()
        zip_apk(self.candidate, {"META-INF/CERT.RSA": b"signature", "META-INF/MANIFEST.MF": b"signature"})
        self.assertTrue(self.verify()["all_unsigned_payload_matches"])

    def prepare(self, **kwargs):
        with patch.object(preparation, "require_unsigned"), patch.object(release, "run_tool", side_effect=self.sdk):
            return preparation.prepare(self.candidate, self.built, self.report, self.root,
                                       kwargs.get("commit", COMMIT), kwargs.get("code", 2),
                                       kwargs.get("output", self.root / "export"))

    def test_unsigned_export_is_bound_to_verified_inputs(self):
        result = self.prepare()
        output = self.root / "export"
        self.assertEqual((output / result["apk"]).read_bytes(), self.candidate.read_bytes())
        self.assertEqual(result["artifact_kind"], "unsigned-release-candidate")
        self.assertEqual(result["package"], release.PACKAGE)
        self.assertEqual(result["application_label"], "Tooie Recompiled")
        self.assertEqual(result["development_package"], release.DEVELOPMENT_PACKAGE)
        self.assertFalse(result["shared_app_data"])
        self.assertFalse(result["publication_approved"])
        self.assertFalse(any(p.suffix in (".p12", ".jks", ".keystore") for p in output.iterdir()))
        for line in (output / "SHA256SUMS").read_text().splitlines():
            expected, name = line.split("  ", 1)
            self.assertEqual(release.digest((output / name).read_bytes()), expected)
        with self.assertRaisesRegex(ValueError, "already exists"):
            self.prepare()

    def test_unsigned_export_rejects_wrong_provenance_and_manifest(self):
        with self.assertRaisesRegex(ValueError, "different source"):
            self.prepare(commit="cd" * 20)
        for value in (0, True, 2_100_000_001):
            with self.assertRaisesRegex(ValueError, "Android range"):
                self.prepare(code=value)
        self.badging_extra = "application-debuggable\n"
        with self.assertRaisesRegex(ValueError, "debuggable"):
            self.prepare()
        self.badging_extra = ""
        self.build_report["sha256"] = "00" * 32
        self.write_report()
        with self.assertRaisesRegex(ValueError, "APK hash"):
            self.prepare()
        self.assertFalse((self.root / "export").exists())

    def test_unsigned_sdk_failure_is_not_assumed_to_mean_unsigned(self):
        for code, text in ((0, "verified"), (1, "unrelated error")):
            with patch.object(preparation.subprocess, "run", return_value=subprocess.CompletedProcess([], code, "", text)):
                with self.assertRaisesRegex(ValueError, "Expected an unsigned"):
                    preparation.require_unsigned(self.candidate, self.root)
        with patch.object(preparation.subprocess, "run", side_effect=subprocess.TimeoutExpired("apksigner", 45)):
            with self.assertRaisesRegex(ValueError, "timed out; no retry"):
                preparation.require_unsigned(self.candidate, self.root)

    def local_sign_inputs(self):
        report = self.prepare()
        key = self.root / "fixture.p12"
        key.write_bytes(b"fixture placeholder; no actual private key")
        identities = self.root / "identities.json"
        identities.write_text(json.dumps({"schema": 1, "release": {"alias": "fixture", "certificate_sha256": CERT}}))
        return report, key, identities

    def test_local_signing_rejects_mutated_input_or_incompatible_update_before_prompt(self):
        report, key, identities = self.local_sign_inputs()
        exported = self.root / "export"
        apk = exported / report["apk"]
        original = apk.read_bytes()
        apk.write_bytes(original + b"altered")
        with patch.object(signing.subprocess, "run") as process:
            with self.assertRaisesRegex(ValueError, "candidate hash"):
                signing.sign(exported, self.root, key, identities, self.root / "signed")
            process.assert_not_called()
        apk.write_bytes(original)
        self.cert = "34" * 32
        with patch.object(release, "run_tool", side_effect=self.sdk), patch.object(signing.subprocess, "run") as process:
            with self.assertRaisesRegex(ValueError, "do not uninstall or reset"):
                signing.sign(exported, self.root, key, identities, self.root / "signed", self.previous)
            process.assert_not_called()

    def test_older_same_package_report_stops_before_signing_prompt(self):
        report, key, identities = self.local_sign_inputs()
        report["package"] = release.DEVELOPMENT_PACKAGE
        (self.root / "export/build-report.json").write_text(json.dumps(report))
        with patch.object(signing.subprocess, "run") as process:
            with self.assertRaisesRegex(ValueError, "rebuild older same-package"):
                signing.sign(self.root / "export", self.root, key, identities, self.root / "signed")
            process.assert_not_called()

    def test_actual_legacy_package_stops_before_signing_prompt(self):
        _, key, identities = self.local_sign_inputs()
        self.package = release.DEVELOPMENT_PACKAGE
        with patch.object(release, "run_tool", side_effect=self.sdk), patch.object(signing.subprocess, "run") as process:
            with self.assertRaisesRegex(ValueError, "package identity"):
                signing.sign(self.root / "export", self.root, key, identities, self.root / "signed")
            process.assert_not_called()

    def test_local_signer_publishes_only_a_verified_result_to_a_new_directory(self):
        report, key, identities = self.local_sign_inputs()
        original = (self.root / "export" / report["apk"]).read_bytes()
        def fixture_sign(args, **kwargs):
            Path(args[args.index("--out") + 1]).write_bytes(Path(args[-1]).read_bytes())
            return subprocess.CompletedProcess(args, 0)
        with patch.object(release, "run_tool", side_effect=self.sdk), patch.object(signing.subprocess, "run", side_effect=fixture_sign):
            result = signing.sign(self.root / "export", self.root, key, identities, self.root / "signed")
            self.assertTrue(result["all_unsigned_payload_matches"])
            self.assertFalse(result["install_performed"])
            self.assertFalse(result["publication_approved"])
            self.assertEqual((self.root / "export" / report["apk"]).read_bytes(), original)
            with self.assertRaisesRegex(ValueError, "already exists"):
                signing.sign(self.root / "export", self.root, key, identities, self.root / "signed")
        self.assertEqual(len(list((self.root / "signed").glob("*.apk"))), 1)

    def test_wrong_actual_signer_leaves_no_exported_result(self):
        _, key, identities = self.local_sign_inputs()
        def fixture_sign(args, **kwargs):
            Path(args[args.index("--out") + 1]).write_bytes(Path(args[-1]).read_bytes())
            return subprocess.CompletedProcess(args, 0)
        self.cert = "34" * 32
        with patch.object(release, "run_tool", side_effect=self.sdk), patch.object(signing.subprocess, "run", side_effect=fixture_sign):
            with self.assertRaisesRegex(ValueError, "Unexpected release signing"):
                signing.sign(self.root / "export", self.root, key, identities, self.root / "signed")
        self.assertFalse((self.root / "signed").exists())


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
    unsigned = package / "app/build/outputs/apk/release/app-release-unsigned.apk"
    require = release.require
    debug_identity = release.manifest_identity(built, tools)
    require(debug_identity["package"] == release.DEVELOPMENT_PACKAGE and
            release.one(r"^application-label:'([^']*)'", debug_identity["badging"], "debug label", re.M)
            == "Tooie Recompiled Debug", "Established development package/label differs")
    with tempfile.TemporaryDirectory(prefix="tooie-signing-fixture-") as temp:
        root = Path(temp)
        # A previous release must be the same release app, not a re-signed debug
        # APK. Build both version codes with the real Android manifest merger.
        subprocess.run(["gradle", "--no-daemon", "--console=plain", "-p", str(package),
                        "-PtooieVersionCode=1", "-PtooieVersionName=" + version, "assembleRelease"], check=True)
        previous_unsigned = root / "previous-unsigned.apk"
        previous_unsigned.write_bytes(unsigned.read_bytes())
        subprocess.run(["gradle", "--no-daemon", "--console=plain", "-p", str(package),
                        "-PtooieVersionCode=2", "-PtooieVersionName=" + version,
                        "assembleRelease", "assembleDebug"], check=True)
        require(unsigned.is_file(), "Gradle did not produce the unsigned release fixture")
        debug_variant = package / "app/build/outputs/apk/debug/app-debug.apk"
        debug = release.manifest_identity(debug_variant, tools)
        require(debug["package"] == release.DEVELOPMENT_PACKAGE and
                release.one(r"^application-label:'([^']*)'", debug["badging"], "debug label", re.M)
                == "Tooie Recompiled Debug" and
                release.one(r"^launchable-activity: name='([^']+)'", debug["badging"], "debug activity", re.M)
                == release.ACTIVITY and "application-debuggable" in debug["badging"],
                "Real AGP debug package, label, activity or debuggable flag differs")
        prepared = root / "prepared"
        unsigned_report = preparation.prepare(unsigned, built, build_report, tools,
                                              report["source_commit"], 2, prepared)
        reference = prepared / unsigned_report["apk"]
        reference_report = prepared / "build-report.json"
        env = {**os.environ, "TOOIE_RELEASE_TEST_PASSWORD": "ephemeral-ci-fixture"}
        key = root / "fixture.jks"
        subprocess.run(["keytool", "-genkeypair", "-keystore", str(key), "-alias", "fixture",
                        "-keyalg", "RSA", "-keysize", "2048", "-validity", "2",
                        "-dname", "CN=Tooie CI Signing Fixture",
                        "-storepass:env", "TOOIE_RELEASE_TEST_PASSWORD",
                        "-keypass:env", "TOOIE_RELEASE_TEST_PASSWORD"], check=True, env=env)
        candidate, previous, debuggable = root / "candidate.apk", root / "previous.apk", root / "debuggable.apk"
        for source, destination in ((unsigned, candidate), (previous_unsigned, previous), (debug_variant, debuggable)):
            subprocess.run([str(tools / "apksigner"), "sign", "--ks", str(key), "--ks-key-alias", "fixture",
                            "--ks-pass", "env:TOOIE_RELEASE_TEST_PASSWORD", "--key-pass", "env:TOOIE_RELEASE_TEST_PASSWORD",
                            "--out", str(destination), str(source)], check=True, env=env)
        identity = release.apk_identity(candidate, tools)
        result = release.verify(candidate, reference, reference_report, tools, version, 2,
                                identity["certificate_sha256"], report["source_commit"], previous)
        require(result["previous_apk"]["direct_update_metadata_compatible"], "Same-key upgrade fixture failed")
        require(result["all_unsigned_payload_matches"], "Complete unsigned payload fixture failed")
        fixture_identities = root / "fixture-identities.json"
        fixture_identities.write_text(json.dumps({"schema": 1, "release": {
            "alias": "fixture", "certificate_sha256": identity["certificate_sha256"]}}))
        helper = subprocess.run([sys.executable, str(prepared / "sign_candidate.py"),
                                 "--candidate-dir", str(prepared), "--build-tools", str(tools),
                                 "--keystore", str(key), "--identities", str(fixture_identities),
                                 "--output", str(root / "helper-signed"), "--new-install"],
                                input=env["TOOIE_RELEASE_TEST_PASSWORD"] + "\n",
                                capture_output=True, text=True, timeout=120)
        require(helper.returncode == 0, "Bundled local signer fixture failed: " +
                (helper.stdout + helper.stderr)[-4000:].replace(env["TOOIE_RELEASE_TEST_PASSWORD"], "[fixture-password]"))
        require(env["TOOIE_RELEASE_TEST_PASSWORD"] not in helper.stdout + helper.stderr,
                "Local signer echoed its fixture password")
        checked = json.loads((root / "helper-signed/verification.json").read_text())
        require(checked["all_unsigned_payload_matches"] and not checked["install_performed"] and
                not checked["publication_approved"] and checked["installation_strategy"] == "separate-apps" and
                checked["package"] == release.PACKAGE and not checked["shared_app_data"],
                "Bundled local signer report has the wrong scope or app identity")
        wrong_key, wrong_previous = root / "wrong-key.jks", root / "wrong-previous.apk"
        subprocess.run(["keytool", "-genkeypair", "-keystore", str(wrong_key), "-alias", "fixture",
                        "-keyalg", "RSA", "-keysize", "2048", "-validity", "2",
                        "-dname", "CN=Wrong CI Signing Fixture",
                        "-storepass:env", "TOOIE_RELEASE_TEST_PASSWORD",
                        "-keypass:env", "TOOIE_RELEASE_TEST_PASSWORD"], check=True, env=env)
        subprocess.run([str(tools / "apksigner"), "sign", "--ks", str(wrong_key), "--ks-key-alias", "fixture",
                        "--ks-pass", "env:TOOIE_RELEASE_TEST_PASSWORD", "--key-pass", "env:TOOIE_RELEASE_TEST_PASSWORD",
                        "--out", str(wrong_previous), str(previous_unsigned)], check=True, env=env)
        for test_apk, old, message in ((unsigned, previous, "apksigner rejected"),
                                       (debuggable, previous, "Android package identity differs"),
                                       (candidate, built, "Previous APK uses a different package"),
                                       (candidate, wrong_previous, "different signing certificate")):
            try:
                release.verify(test_apk, reference, reference_report, tools, version, 2,
                               identity["certificate_sha256"], report["source_commit"], old)
            except ValueError as error:
                require(message in str(error), "Wrong real-SDK rejection: " + str(error))
            else:
                raise ValueError("Missing real-SDK rejection: " + message)
        altered, altered_signed = root / "altered.apk", root / "altered-signed.apk"
        with zipfile.ZipFile(reference) as src, zipfile.ZipFile(altered, "w") as dst:
            for entry in src.infolist():
                data = src.read(entry.filename)
                dst.writestr(entry, data + b"altered" if entry.filename == "classes.dex" else data)
        subprocess.run([str(tools / "apksigner"), "sign", "--ks", str(key), "--ks-key-alias", "fixture",
                        "--ks-pass", "env:TOOIE_RELEASE_TEST_PASSWORD", "--key-pass", "env:TOOIE_RELEASE_TEST_PASSWORD",
                        "--out", str(altered_signed), str(altered)], check=True, env=env)
        try:
            release.verify(altered_signed, reference, reference_report, tools, version, 2,
                           identity["certificate_sha256"], report["source_commit"])
        except ValueError as error:
            require("unsigned payload differs: classes.dex" in str(error), "Wrong signed-DEX rejection: " + str(error))
        else:
            raise ValueError("A valid new signature masked altered DEX contents")
        # The fixture is temporary, never exported with APK artifacts or logs.
    print("PASS: real AGP separate debug/release packages and labels, unchanged activity namespace, "
          "unsigned export and bundled local signer, manifest/version/native/assets "
          "and complete unsigned payload checks, same-key update metadata and unsigned/debug/wrong-key/re-signed-DEX "
          "controls; no device or production key used")


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
