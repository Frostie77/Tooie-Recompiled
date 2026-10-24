#!/usr/bin/env python3
"""Regressions for the APK verifier's DEX entry selection and marker policy.

These small ZIP fixtures isolate the entry-selection policy. The normal CI
packaging step still verifies the actual Gradle-built and signed Android APK.
"""
import contextlib
import io
import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest import mock
import zipfile

from verify_android import check_update_java, check_configuration, check_native_entries, apk_profile


class UpdateJavaPackagingTests(unittest.TestCase):
    client = b"UpdateClient\0UpdateManifest\0AndroidUpdates\0"
    bridge = b"getUpdateState\0requestUpdateCheck\0openUpdateRelease\0"

    def check(self, entries):
        data = io.BytesIO()
        with zipfile.ZipFile(data, "w") as package:
            for name, content in entries.items():
                package.writestr(name, content)
        data.seek(0)
        with zipfile.ZipFile(data) as package, contextlib.redirect_stdout(io.StringIO()):
            check_update_java(package)

    def test_single_dex(self):
        self.check({"classes.dex": self.client + self.bridge})

    def test_update_classes_in_secondary_dex(self):
        self.check({"classes.dex": self.bridge, "classes2.dex": self.client})

    def test_double_digit_secondary_dex(self):
        entries = {"classes.dex": self.bridge, "classes10.dex": self.client}
        entries.update({f"classes{i}.dex": b"unrelated" for i in range(2, 10)})
        self.check(entries)

    def test_missing_marker_still_fails(self):
        for marker in (b"UpdateClient", b"UpdateManifest", b"AndroidUpdates", b"getUpdateState", b"requestUpdateCheck", b"openUpdateRelease"):
            with self.subTest(marker=marker), self.assertRaisesRegex(ValueError, marker.decode()):
                self.check({"classes.dex": self.bridge.replace(marker, b""),
                            "classes2.dex": self.client.replace(marker, b"")})

    def test_asset_or_nonstandard_dex_cannot_satisfy_check(self):
        for name in ("assets/classes2.dex", "nested/classes2.dex", "classes1.dex", "classes02.dex", "classes2.dex.bak"):
            with self.subTest(entry=name), self.assertRaisesRegex(ValueError, "UpdateClient"):
                self.check({"classes.dex": self.bridge, name: self.client})

    def test_no_primary_dex_fails(self):
        with self.assertRaisesRegex(ValueError, "primary|classes.dex"):
            self.check({"classes2.dex": self.client + self.bridge})

    def test_marker_split_across_dex_files_does_not_count(self):
        with self.assertRaisesRegex(ValueError, "UpdateClient"):
            self.check({"classes.dex": b"Update", "classes2.dex": b"Client\0" + self.bridge})


class DriverVariantPackagingTests(unittest.TestCase):
    def test_isolated_identity_and_min_api(self):
        revision = "a" * 40
        normal, experimental = apk_profile(revision, False), apk_profile(revision, True)
        self.assertEqual(normal["package"], "io.github.banjotooierecompiled")
        self.assertEqual(experimental["package"], normal["package"] + ".gputest.g" + revision[:12])
        self.assertEqual((normal["min_api"], experimental["min_api"]), (24, 28))

    def test_native_hooks_cannot_leak_into_normal_package(self):
        originals = {"libmain.so": None, "libSDL2.so": None}
        names = ["lib/arm64-v8a/" + n for n in originals]
        check_native_entries(names, originals)
        with self.assertRaisesRegex(ValueError, "native library"):
            check_native_entries(names + ["lib/arm64-v8a/libmain_hook.so"], originals)
        with self.assertRaisesRegex(ValueError, "native library"):
            check_native_entries(names + ["lib/x86_64/libmain.so"], originals)

    def test_missing_hook_is_rejected(self):
        names = ["libmain.so", "libSDL2.so", "libmain_hook.so", "libhook_impl.so", "libfile_redirect_hook.so", "libgsl_alloc_hook.so", "libc++_shared.so"]
        for missing in names:
            with self.subTest(missing=missing), self.assertRaisesRegex(ValueError, "native library"):
                check_native_entries(["lib/arm64-v8a/" + n for n in names if n != missing], dict.fromkeys(names))

    def configuration(self, mode, cache_mode=None, flags=None, loader_source=True):
        with tempfile.TemporaryDirectory() as temp:
            source = Path(temp) / "source"; work = Path(temp) / "work"; ndk = Path(temp) / "ndk"
            native = work / "native"; native.mkdir(parents=True)
            cache = "TOOIE_ANDROID_RUNTIME:BOOL=ON\nTOOIE_VULKAN_DESCRIPTOR_ALLOCATION_PROBE:BOOL=OFF\nANDROID_ABI:STRING=arm64-v8a\n"
            cache += "TOOIE_ANDROID_DRIVER_TEST:BOOL=" + (cache_mode or ("ON" if mode == "1" else "OFF")) + "\nANDROID_STL:STRING=c++_shared\n"
            (native / "CMakeCache.txt").write_text(cache)
            targets = ["src/main.cpp", "src/recomp_api.cpp", "src/aiming_controls.cpp", "lib/rt64/src/contrib/plume/plume_vulkan.cpp",
                       "lib/rt64/src/hle/rt64_present_queue.cpp", "lib/rt64/src/render/rt64_transform_processor.cpp"]
            if mode == "1" and loader_source: targets += ["src/android_vulkan_driver.cpp"]
            defines = ["-D" + name + "=0" for name in ("TOOIE_ANDROID_TIMING_TRACE", "TOOIE_ANDROID_GEOMETRY_TRACE", "TOOIE_ANDROID_CORRECTION_TRACE", "TOOIE_VULKAN_QUEUE_ISOLATION_TEST")]
            defines += flags if flags is not None else (["-DTOOIE_ANDROID_DRIVER_TEST=1"] if mode == "1" else [])
            commands = [{"file": str(source / path), "arguments": [str(ndk / "bin/aarch64-linux-android24-clang++"), *defines]} for path in targets]
            (native / "compile_commands.json").write_text(json.dumps(commands))
            with mock.patch.dict(os.environ, {"TOOIE_ANDROID_DRIVER_TEST": mode}), contextlib.redirect_stdout(io.StringIO()):
                check_configuration(source, work, ndk)

    def test_actual_compile_mode_is_checked(self):
        self.configuration("0"); self.configuration("1")
        for mode, options in (("1", {"cache_mode": "OFF"}), ("1", {"flags": []}),
                              ("0", {"flags": ["-DTOOIE_ANDROID_DRIVER_TEST=1"]}), ("1", {"loader_source": False})):
            with self.subTest(mode=mode, options=options), self.assertRaises(ValueError): self.configuration(mode, **options)


if __name__ == "__main__":
    unittest.main(verbosity=2)
