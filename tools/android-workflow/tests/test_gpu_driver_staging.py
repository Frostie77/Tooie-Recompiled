#!/usr/bin/env python3
import importlib.util
import io
import json
from pathlib import Path
import stat
import tempfile
import unittest
from unittest import mock
import zipfile

SCRIPT = Path(__file__).resolve().parents[1] / "stage_gpu_driver.py"
spec = importlib.util.spec_from_file_location("gpu_stage", SCRIPT)
gpu = importlib.util.module_from_spec(spec); spec.loader.exec_module(gpu)


def elf():
    data = bytearray(64); data[:6] = b"\x7fELF\x02\x01"; data[16] = 3; data[18] = 183
    return bytes(data)


class FakeDevice(gpu.Device):
    def __init__(self, package):
        super().__init__("adb", "device-1")
        self.package = package; self.commands = []; self.files = {}; self.bad_hash = False

    def call(self, *args, data=None):
        self.commands.append(args)
        if args[:3] == ("shell", "am", "force-stop"):
            assert args[3] == self.package and gpu.PACKAGE.fullmatch(args[3]); return b""
        assert args[1:3] == ("run-as", self.package)
        op = args[3]
        if op == "dd":
            self.files[args[4][3:]] = data; return b""
        if op == "sha256sum":
            value = "0" * 64 if self.bad_hash else gpu.digest(self.files[args[4]])
            return (value + "  " + args[4]).encode()
        if op == "mv":
            self.files[args[5]] = self.files.pop(args[4]); return b""
        if op == "cat":return self.files[args[4]]
        assert op in ("mkdir", "chmod"); return b""


class StagingTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(); self.root = Path(self.tmp.name)
        self.commit = "a" * 40; self.package = "io.github.banjotooierecompiled.gputest.g" + self.commit[:12]
        self.report = {"source_commit": self.commit, "package": self.package, "variant": "android-gpu-driver-test",
                       "min_api": 28, "apk": "Tooie-Recompiled-Android-arm64-" + self.commit[:12] + "-GPU-Test.apk"}
        (self.root / self.report["apk"]).write_bytes(b"verified-apk")
        self.report["sha256"] = gpu.digest(b"verified-apk")
        self.report_path = self.root / "build-report.json"; self.save_report()
        self.metadata = {"schemaVersion": 1, "name": "Mesa Turnip", "vendor": "Mesa", "minApi": 27, "libraryName": "driver.so"}
        self.zip = self.root / "driver.zip"; self.make_zip()

    def tearDown(self): self.tmp.cleanup()
    def save_report(self): self.report_path.write_text(json.dumps(self.report))
    def make_zip(self, entries=None):
        with zipfile.ZipFile(self.zip, "w") as archive:
            for name, data in (entries or {"meta.json": json.dumps(self.metadata).encode(), "driver.so": elf()}).items():
                archive.writestr(name, data)
        self.sha = gpu.digest(self.zip.read_bytes())

    def test_verified_test_report(self):
        self.assertEqual(gpu.read_report(self.report_path)["package"], self.package)

    def test_normal_apps_and_wrong_source_refused(self):
        for package in ("io.github.banjotooierecompiled", "io.github.banjotooierecompiled.release", self.package[:-1] + "b"):
            with self.subTest(package=package):
                self.report["package"] = package; self.save_report()
                with self.assertRaises(ValueError): gpu.read_report(self.report_path)

    def test_changed_local_apk_refused(self):
        (self.root / self.report["apk"]).write_bytes(b"changed")
        with self.assertRaisesRegex(ValueError, "APK"): gpu.read_report(self.report_path)

    def test_minimum_accounts_for_loader(self):
        meta, files, minimum = gpu.read_driver(self.zip, self.sha, 31)
        self.assertEqual(minimum, 28); self.assertEqual(files["driver.so"], elf())

    def test_wrong_hash(self):
        with self.assertRaisesRegex(ValueError, "SHA-256"): gpu.read_driver(self.zip, "b" * 64, 31)

    def test_bad_api_and_non_turnip(self):
        for field, value in (("minApi", 32), ("minApi", True), ("schemaVersion", 2), ("vendor", "Qualcomm"), ("name", "other")):
            with self.subTest(field=field):
                old = self.metadata[field]; self.metadata[field] = value; self.make_zip()
                with self.assertRaises(ValueError): gpu.read_driver(self.zip, self.sha, 31)
                self.metadata[field] = old

    def test_paths_and_symlinks(self):
        for name in ("../bad.so", "dir/bad.so", "x\\bad.so", ".hidden", "a..so"):
            self.make_zip({"meta.json": json.dumps(self.metadata).encode(), name: elf()})
            with self.assertRaisesRegex(ValueError, "Unsafe"): gpu.read_driver(self.zip, self.sha, 31)
        with zipfile.ZipFile(self.zip, "w") as archive:
            info = zipfile.ZipInfo("driver.so"); info.create_system = 3; info.external_attr = (stat.S_IFLNK | 0o777) << 16
            archive.writestr("meta.json", json.dumps(self.metadata)); archive.writestr(info, "elsewhere")
        with self.assertRaisesRegex(ValueError, "Unsafe"): gpu.read_driver(self.zip, gpu.digest(self.zip.read_bytes()), 31)

    def test_wrong_architecture(self):
        bad = bytearray(elf()); bad[18] = 62
        self.make_zip({"meta.json": json.dumps(self.metadata).encode(), "driver.so": bad})
        with self.assertRaisesRegex(ValueError, "ARM64"): gpu.read_driver(self.zip, self.sha, 31)

    def test_success_publishes_after_hash_and_readonly(self):
        device = FakeDevice(self.package)
        receipt = gpu.stage(device, self.report, self.zip, self.sha, 31)
        self.assertFalse(receipt["active_driver_verified"])
        config = device.files[gpu.ROOT + "/selection.txt"].decode()
        self.assertIn("min_api=28\n", config); self.assertIn(gpu.digest(elf()), config)
        move = next(i for i, a in enumerate(device.commands) if len(a) > 3 and a[3] == "mv")
        readonly = [i for i, a in enumerate(device.commands) if len(a) > 4 and a[3:5] == ("chmod", "400")]
        self.assertEqual(len(readonly), 2); self.assertLess(max(readonly), move)

    def test_failed_remote_hash_retains_previous_selection(self):
        device = FakeDevice(self.package); device.bad_hash = True
        device.files[gpu.ROOT + "/selection.txt"] = b"previous"
        with self.assertRaisesRegex(ValueError, "Remote"): gpu.stage(device, self.report, self.zip, self.sha, 31)
        self.assertEqual(device.files[gpu.ROOT + "/selection.txt"], b"previous")
        self.assertFalse(any(len(a) > 3 and a[3] == "mv" for a in device.commands))

    def test_invalid_zip_does_not_stop_app(self):
        device = FakeDevice(self.package)
        with self.assertRaises(ValueError): gpu.stage(device, self.report, self.zip, "b" * 64, 31)
        self.assertEqual(device.commands, [])

    def test_device_guards_cannot_target_normal_apps(self):
        device = FakeDevice("io.github.banjotooierecompiled")
        for operation in (device.stop, lambda: device.private("mkdir", "anything"), lambda: device.verify(self.report | {"package": device.package})):
            with self.assertRaisesRegex(ValueError, "non-test"): operation()
        self.assertEqual(device.commands, [])
        device.package = self.package
        with self.assertRaisesRegex(ValueError, "path"): device.put(gpu.ROOT + "/../bad.so", b"x")
        self.assertEqual(device.commands, [])


if __name__ == "__main__": unittest.main(verbosity=2)
