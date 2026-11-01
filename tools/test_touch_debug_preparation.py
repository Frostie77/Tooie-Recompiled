#!/usr/bin/env python3
"""Check touch candidate binding, local re-signing and the installed-build capture launcher."""
import argparse
import contextlib
import importlib.util
import io
import json
import os
import shlex
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
import zipfile

ROOT = Path(__file__).resolve().parents[1]
HELPER = ROOT / "tools/android-workflow/prepare_touch_debug.py"
spec = importlib.util.spec_from_file_location("touch_preparation", HELPER)
preparation = importlib.util.module_from_spec(spec)
spec.loader.exec_module(preparation)
SOURCE = "a" * 40
VERSION = "0.1.0-ci.fixture+g" + SOURCE[:12]


def zipped(entries):
    data = io.BytesIO()
    with zipfile.ZipFile(data, "w") as bundle:
        for name, value in entries.items():
            bundle.writestr(name, value)
    return data.getvalue()


class CandidateChecks(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.path = Path(self.temp.name) / "candidate.zip"
        self.app = {"classes.dex": b"real-code-fixture", "AndroidManifest.xml": b"manifest",
                    "lib/arm64-v8a/libmain.so": VERSION.encode(), "lib/arm64-v8a/libSDL2.so": b"sdl"}
        self.tools = {name: b"# fixture " + name.encode() for name in ("capture_installed.py", "collector.py", "tooie.py")}
        self.filename = "Tooie-Recompiled-Android-arm64-" + SOURCE[:12] + "-Touch-Debug-Candidate.apk"
        self.report = {"source_commit": SOURCE, "native_version": VERSION,
                       "package": "io.github.banjotooierecompiled", "variant": "Touch-Debug-Candidate",
                       "apk": self.filename, "libraries": {},
                       "capture_tools": {name: preparation.digest(value) for name, value in self.tools.items()}}

    def write(self, extra=None):
        apk = zipped(self.app)
        self.report["sha256"] = preparation.digest(apk)
        self.report["libraries"] = {name.rsplit("/", 1)[-1]: {"sha256": preparation.digest(value)}
                                    for name, value in self.app.items() if name.startswith("lib/arm64-v8a/")}
        self.entries = {self.filename: apk, "build-report.json": json.dumps(self.report), **self.tools, **(extra or {})}
        self.path.write_bytes(zipped(self.entries))
        return preparation.digest(self.path.read_bytes())

    def load(self, sha=None, source=SOURCE):
        return preparation.load_candidate(self.path, sha or preparation.digest(self.path.read_bytes()), source)

    def test_exact_source_payload_and_capture_tools(self):
        sha = self.write()
        report, app, tools = self.load(sha)
        self.assertEqual(report, self.report)
        self.assertEqual(preparation.payload(app), {name: preparation.digest(data) for name, data in self.app.items()})
        self.assertEqual(tools, self.tools)

    def test_wrong_zip_or_source_stops(self):
        self.write()
        with self.assertRaisesRegex(ValueError, "ZIP SHA-256"):
            self.load("0" * 64)
        with self.assertRaisesRegex(ValueError, "source"):
            self.load(source="b" * 40)

    def test_other_app_or_variant_cannot_be_used_as_a_debug_update(self):
        for key, value in (("package", "io.github.banjotooierecompiled.release"), ("variant", "GPU-Test")):
            original = self.report[key]
            self.report[key] = value
            self.write()
            with self.assertRaisesRegex(ValueError, "identity"):
                self.load()
            self.report[key] = original

    def test_archive_paths_and_duplicate_entries_stop(self):
        for name in ("../escape", "dir/file", "dir\\file", "C:escape"):
            self.write({name: b"unexpected"})
            with self.assertRaisesRegex(ValueError, "paths"):
                self.load()
        self.write()
        with contextlib.redirect_stderr(io.StringIO()), zipfile.ZipFile(self.path, "a") as bundle:
            bundle.writestr("build-report.json", "duplicate")
        with self.assertRaisesRegex(ValueError, "duplicate"):
            self.load()

    def test_apk_and_capture_mutations_are_rejected(self):
        for name in (self.filename, "capture_installed.py", "collector.py", "tooie.py"):
            self.write()
            self.entries[name] = b"changed"
            self.path.write_bytes(zipped(self.entries))
            with self.subTest(name=name), self.assertRaisesRegex(ValueError, "digest"):
                self.load()

    def test_native_library_inventory_is_bound(self):
        self.app["lib/x86/libmain.so"] = b"wrong-abi"
        self.write()
        with self.assertRaisesRegex(ValueError, "library set"):
            self.load()

    def driver_profile(self):
        self.report["driver_picker"] = True
        for name in ("libmain_hook.so", "libhook_impl.so", "libfile_redirect_hook.so",
                     "libgsl_alloc_hook.so", "libc++_shared.so"):
            self.app["lib/arm64-v8a/" + name] = ("fixture-" + name).encode()

    def test_driver_candidate_accepts_complete_seven_library_profile(self):
        self.driver_profile()
        self.write()
        report, apk, _ = self.load()
        self.assertEqual(len(report["libraries"]), 7)
        self.assertEqual(preparation.payload(apk), {name: preparation.digest(value) for name, value in self.app.items()})

    def test_normal_debug_artifact_uses_the_same_verified_local_update_path(self):
        self.driver_profile()
        self.report['variant'] = 'debug'
        self.filename = 'Tooie-Recompiled-Android-arm64-' + SOURCE[:12] + '-Debug.apk'
        self.report['apk'] = self.filename
        self.write()
        report, _, tools = self.load()
        self.assertEqual(report['variant'], 'debug')
        self.assertEqual(len(report['libraries']), 7)
        self.assertEqual(tools, self.tools)
        self.report['apk'] = self.filename.replace('-Debug.apk', '-GPU-Test.apk')
        self.write()
        with self.assertRaisesRegex(ValueError, 'APK filename'):
            self.load()

    def test_driver_inventory_cannot_be_missing_extra_or_assigned_to_legacy_profile(self):
        self.driver_profile()
        original = self.app.copy()
        for change in ("missing", "extra", "legacy", "wrong-abi"):
            self.app = original.copy()
            self.report["driver_picker"] = True
            if change == "missing": del self.app["lib/arm64-v8a/libhook_impl.so"]
            elif change == "extra": self.app["lib/arm64-v8a/unreported.so"] = b"extra"
            elif change == "legacy": self.report["driver_picker"] = False
            else: self.app["lib/x86/libhook_impl.so"] = b"wrong-abi"
            self.write()
            with self.subTest(change=change), self.assertRaisesRegex(ValueError, "library set"):
                self.load()

    def test_each_driver_library_must_match_its_reported_hash(self):
        self.driver_profile()
        self.write()
        report = self.report.copy()
        for name in ("libmain_hook.so", "libhook_impl.so", "libfile_redirect_hook.so",
                     "libgsl_alloc_hook.so", "libc++_shared.so"):
            changed = zipped({**self.app, "lib/arm64-v8a/" + name: b"changed-native-code"})
            self.entries[self.filename] = changed
            self.entries["build-report.json"] = json.dumps({**report, "sha256": preparation.digest(changed)})
            self.path.write_bytes(zipped(self.entries))
            with self.subTest(name=name), self.assertRaisesRegex(ValueError, "Native library differs"):
                self.load()

    def test_driver_flag_must_be_boolean_and_report_inventory_complete(self):
        self.driver_profile()
        for flag in ("true", 1, None):
            self.report["driver_picker"] = flag
            self.write()
            with self.subTest(flag=flag), self.assertRaisesRegex(ValueError, "driver-picker profile"):
                self.load()
        self.report["driver_picker"] = True
        self.write()
        del self.report["libraries"]["libc++_shared.so"]
        self.entries["build-report.json"] = json.dumps(self.report)
        self.path.write_bytes(zipped(self.entries))
        with self.assertRaisesRegex(ValueError, "Incomplete native library report"):
            self.load()

    def test_signature_metadata_is_excluded_but_code_and_resources_are_bound(self):
        original = zipped(self.app)
        signed = zipped({**self.app, "META-INF/CERT.RSA": b"signature", "META-INF/CERT.SF": b"signature",
                         "META-INF/MANIFEST.MF": b"signature"})
        self.assertEqual(preparation.payload(original), preparation.payload(signed))
        for name in ("classes.dex", "AndroidManifest.xml", "res/new.bin"):
            self.assertNotEqual(preparation.payload(original), preparation.payload(zipped({**self.app, name: b"altered"})))


def integration(work, tools):
    artifacts = work / "artifacts"
    report = json.loads((artifacts / "build-report.json").read_text())
    preparation.require(report["variant"] in ("debug", "Touch-Debug-Candidate"), "Expected real Debug candidate")
    with tempfile.TemporaryDirectory(prefix="tooie-touch-signing-fixture-") as temp:
        area = Path(temp)
        key = area / "debug.keystore"
        subprocess.run(["keytool", "-genkeypair", "-keystore", str(key), "-alias", "androiddebugkey",
                        "-keyalg", "RSA", "-keysize", "2048", "-validity", "2", "-dname", "CN=Touch CI Fixture",
                        "-storepass", "android", "-keypass", "android"], check=True, capture_output=True)
        cert = subprocess.check_output(["keytool", "-exportcert", "-keystore", str(key), "-alias", "androiddebugkey",
                                        "-storepass", "android"], timeout=60)
        fingerprint = preparation.digest(cert)
        candidate = area / "candidate.zip"
        with zipfile.ZipFile(candidate, "w", zipfile.ZIP_DEFLATED) as bundle:
            for path in sorted(artifacts.iterdir()):
                if path.is_file():
                    bundle.write(path, path.name)
        # Any attempted device operation fails the fixture, even if it is read-only.
        adb = area / "forbidden-adb"
        device_access = area / "unexpected-device-access"
        adb.write_text("#!/bin/sh\ntouch '" + str(device_access) + "'\nexit 99\n")
        adb.chmod(0o700)
        command = [sys.executable, str(HELPER), "--zip", str(candidate), "--expect-zip-sha256",
                   preparation.digest(candidate.read_bytes()), "--expect-source", report["source_commit"],
                   "--expect-cert", fingerprint, "--keystore", str(key), "--build-tools", str(tools),
                   "--output-root", str(area / "prepared"), "--downloads", str(area / "downloads"),
                   "--adb", str(adb), "--serial", "fixture-device", "--camera-trace"]
        result = subprocess.run(command, text=True, capture_output=True, timeout=240)
        preparation.require(result.returncode == 0, "Real SDK local signing failed: " + result.stdout + result.stderr)
        folder, = (area / "prepared").iterdir()
        checked = json.loads((folder / "touch-verification.json").read_text())
        signed, = folder.glob("*-Local-Debug.apk")
        original = artifacts / report["apk"]
        preparation.require(checked["certificate_sha256"] == fingerprint and checked["all_unsigned_payload_matches"] and
                            checked["source_commit"] == report["source_commit"] and not checked["install_performed"] and
                            not checked["device_data_preservation_tested"], "Wrong verification receipt scope")
        preparation.require(checked["apk_sha256"] == preparation.digest(signed.read_bytes()) and
                            preparation.payload(signed.read_bytes()) == preparation.payload(original.read_bytes()),
                            "Re-signed payload or receipt differs")
        launcher = folder / "capture-touch.sh"
        subprocess.run(["sh", "-n", str(launcher)], check=True)
        text = launcher.read_text()
        preparation.require(checked["apk_sha256"] in text and report["source_commit"] in text and fingerprint in text and
                            "fixture-device" in text and "capture-state" in text, "Capture launcher is not bound to new build")
        capture_args = shlex.split(text.splitlines()[-1])
        preparation.require(capture_args[0] == "exec" and capture_args[-1] == "--camera-trace" and
                            capture_args.count("--camera-trace") == 1, "Camera option was not forwarded to the pinned recorder")
        changed = folder / "capture-tools/capture_installed.py"
        changed.chmod(0o600)
        changed.write_text("raise RuntimeError('Changed capture code executed')\n")
        blocked = subprocess.run([str(launcher)], capture_output=True, text=True, timeout=15,
                                 env={**os.environ, "PYTHONOPTIMIZE": "1"})
        preparation.require(blocked.returncode != 0 and "Capture tools changed" in blocked.stderr and
                            "Changed capture code executed" not in blocked.stderr, "Capture tool mutation did not stop")
        wrong = command.copy()
        wrong[wrong.index("--expect-cert") + 1] = "0" * 64
        rejected = subprocess.run(wrong, capture_output=True, text=True, timeout=60)
        preparation.require(rejected.returncode != 0 and "expected Debug signer" in rejected.stderr,
                            "Wrong established signer was accepted")
        preparation.require(len(list((area / "prepared").iterdir())) == 1 and not device_access.exists(),
                            "Failed signing created an output or accessed a device")
    print("PASS: real APK locally re-signed with an ephemeral Debug key; complete payload, certificate, version, "
          "receipt and pinned capture launcher verified; wrong signer and changed capture code rejected; no device used")


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
