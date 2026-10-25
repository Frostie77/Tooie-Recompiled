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
from unittest.mock import patch
import zipfile

from verify_android import (check_update_java, check_touch_java, touch_test_profile,
                            check_configuration, check_native_entries, apk_profile, check_driver_manifest, check_driver_java)


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


class TouchPackagingTests(unittest.TestCase):
    markers = (b"TouchControlsView", b"TouchControllerModel", b"showTouchControlsSettings",
               b"nativeStatus", b"nativeSubmit", b"nativeCancel", b"nativeMenu")

    def check(self, entries):
        data = io.BytesIO()
        with zipfile.ZipFile(data, "w") as package:
            for name, content in entries.items():
                package.writestr(name, content)
        with zipfile.ZipFile(io.BytesIO(data.getvalue())) as package:
            check_touch_java(package)

    def test_executable_secondary_dex_and_each_missing_entry_point(self):
        self.check({"classes.dex": b"primary", "classes2.dex": b"\0".join(self.markers)})
        for marker in self.markers:
            with self.subTest(marker=marker), self.assertRaisesRegex(ValueError, marker.decode()):
                self.check({"classes.dex": b"\0".join(m for m in self.markers if m != marker)})

    def test_assets_cannot_supply_touch_code(self):
        for name in ("assets/classes2.dex", "classes02.dex", "classes1.dex"):
            with self.subTest(name=name), self.assertRaisesRegex(ValueError, "TouchControlsView"):
                self.check({"classes.dex": b"primary", name: b"\0".join(self.markers)})
        with self.assertRaisesRegex(ValueError, "primary"):
            self.check({"classes2.dex": b"\0".join(self.markers)})

    def test_touch_candidate_retains_established_debug_identity(self):
        for mode, variant in (("0", "Debug"), ("1", "Touch-Debug-Candidate")):
            with patch.dict(os.environ, TOOIE_ANDROID_TOUCH_TEST=mode):
                self.assertEqual(touch_test_profile("a" * 40),
                                 ("io.github.banjotooierecompiled", "Tooie Recompiled Debug", variant))
        with patch.dict(os.environ, TOOIE_ANDROID_TOUCH_TEST="invalid"), self.assertRaises(ValueError):
            touch_test_profile("a" * 40)
class DriverVariantPackagingTests(unittest.TestCase):
    def test_isolated_identity_and_min_api(self):
        revision = "a" * 40
        normal, experimental = apk_profile(revision, False), apk_profile(revision, True)
        self.assertEqual(normal["package"], "io.github.banjotooierecompiled")
        self.assertEqual(experimental["package"], normal["package"] + ".gputest.g" + revision[:12])
        self.assertEqual((normal["min_api"], experimental["min_api"]), (24, 28))

    def test_unexpected_native_entries_are_rejected(self):
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

    def configuration(self, mode, cache_mode=None, flags=None, loader_source=True, touch_sources=True, driver_ui=True):
        with tempfile.TemporaryDirectory() as temp:
            source = Path(temp) / "source"; work = Path(temp) / "work"; ndk = Path(temp) / "ndk"
            native = work / "native"; native.mkdir(parents=True)
            cache = "TOOIE_ANDROID_RUNTIME:BOOL=ON\nTOOIE_VULKAN_DESCRIPTOR_ALLOCATION_PROBE:BOOL=OFF\nANDROID_ABI:STRING=arm64-v8a\n"
            cache += "TOOIE_ANDROID_VULKAN_DRIVERS:BOOL=" + (cache_mode or "ON") + "\nANDROID_STL:STRING=c++_shared\n"
            (native / "CMakeCache.txt").write_text(cache)
            targets = ["src/main.cpp", "src/recomp_api.cpp", "src/aiming_controls.cpp", "lib/rt64/src/contrib/plume/plume_vulkan.cpp",
                       "lib/rt64/src/hle/rt64_present_queue.cpp", "lib/rt64/src/render/rt64_transform_processor.cpp"]
            if touch_sources: targets += ["src/android_touch_input.cpp", "src/android_touch_controls.cpp", "src/android_touch_jni.cpp"]
            if loader_source: targets += ["src/android_vulkan_driver.cpp"]
            if driver_ui: targets += ["src/android_driver_jni.cpp", "src/android_driver_settings.cpp"]
            defines = ["-D" + name + "=0" for name in ("TOOIE_ANDROID_TIMING_TRACE", "TOOIE_ANDROID_GEOMETRY_TRACE", "TOOIE_ANDROID_CORRECTION_TRACE", "TOOIE_VULKAN_QUEUE_ISOLATION_TEST")]
            defines += flags if flags is not None else ["-DTOOIE_ANDROID_VULKAN_DRIVERS=1"]
            commands = [{"file": str(source / path), "arguments": [str(ndk / "bin/aarch64-linux-android24-clang++"), *defines]} for path in targets]
            (native / "compile_commands.json").write_text(json.dumps(commands))
            with mock.patch.dict(os.environ, {"TOOIE_ANDROID_DRIVER_TEST": mode}), contextlib.redirect_stdout(io.StringIO()):
                check_configuration(source, work, ndk)

    def test_actual_compile_mode_is_checked(self):
        self.configuration("0"); self.configuration("1")
        for mode in ("0","1"):
            for options in ({"cache_mode":"OFF"},{"flags":[]},{"flags":["-DTOOIE_ANDROID_DRIVER_TEST=1"]},
                            {"loader_source":False},{"touch_sources":False},{"driver_ui":False}):
                with self.subTest(mode=mode,options=options),self.assertRaises(ValueError):self.configuration(mode,**options)


class DriverUiPackagingTests(unittest.TestCase):
    package = "io.github.banjotooierecompiled.gputest.g" + "a" * 12

    def manifest(self):
        prefix = "io.github.banjotooierecompiled."
        lines = ["E: manifest (line=1)", "  E: application (line=2)"]
        for name, exported, affinity, process, launcher in (
                ("BanjoTooieGameActivity", "0x0", ".game", ":game", False),
                ("GraphicsDriverActivity", "0xffffffff", ".drivers", None, True),
                ("GraphicsDriverSettingsActivity", "0x0", ".drivers", None, False)):
            lines += ["    E: activity (line=3)", f'      A: android:name(0x01010003)="{prefix + name}"',
                      f"      A: android:exported(0x01010010)=(type 0x12){exported}",
                      f'      A: android:taskAffinity(0x01010012)="{self.package + affinity}"',
                      "      A: android:hardwareAccelerated(0x010102d3)=(type 0x12)0x0"]
            if process: lines += [f'      A: android:process(0x01010011)="{process}"']
            if launcher:
                lines += ["      E: intent-filter (line=4)", "        E: action (line=5)",
                          '          A: android:name(0x01010003)="android.intent.action.MAIN"',
                          "        E: category (line=6)", '          A: android:name(0x01010003)="android.intent.category.LAUNCHER"']
        lines += ["    E: activity-alias (line=7)",
                  f'      A: android:name="{prefix}BanjoTooieSDLActivity"',
                  f'      A: android:targetActivity="{prefix}GraphicsDriverActivity"',
                  '      A: android:exported=(type 0x12)0xffffffff',
                  '      A: android:enabled=(type 0x12)0xffffffff']
        return "\n".join(lines)

    def test_compiled_manifest_process_roles(self):
        xml = self.manifest(); check_driver_manifest(xml, self.package)
        check_driver_manifest(xml.replace('=":game"', '="' + self.package + ':game"'), self.package)
        changes = [
            ("=(type 0x12)0x0", "=(type 0x12)0xffffffff", 1),
            ('=":game"', '=":other"', 1),
            (self.package + '.game"', self.package + '.drivers"', 1),
            ('="io.github.banjotooierecompiled.GraphicsDriverActivity"', '="missing.Activity"', 1),
            ('      A: android:hardwareAccelerated', '      A: android:process=":game"\n      A: android:hardwareAccelerated', 3),
            ("      A: android:exported(0x01010010)=(type 0x12)0xffffffff", "      A: android:exported(0x01010010)=(type 0x12)0x0", 1),
            ('android:targetActivity="io.github.banjotooierecompiled.GraphicsDriverActivity"',
             'android:targetActivity="io.github.banjotooierecompiled.BanjoTooieGameActivity"', 1),
            ('android:enabled=(type 0x12)0xffffffff', 'android:enabled=(type 0x12)0x0', 1),
        ]
        for old, new, count in changes:
            with self.subTest(change=old), self.assertRaises(ValueError):
                check_driver_manifest(xml.replace(old, new, count), self.package)
        for text in ("", xml.replace("A: android:", "unknown: ")):
            with self.assertRaises(ValueError): check_driver_manifest(text, self.package)
        alias=xml[xml.index('    E: activity-alias'):]
        before=xml[:xml.index('    E: activity-alias')].splitlines()
        for text in (xml[:xml.index('    E: activity-alias')],xml+'\n'+alias,
                     '\n'.join(before[:2]+alias.splitlines()+before[2:]),
                     xml+'\n      E: intent-filter\n        E: category\n          A: android:name="android.intent.category.LAUNCHER"'):
            with self.assertRaises(ValueError):check_driver_manifest(text,self.package)

    def test_driver_java_must_be_executable_dex(self):
        markers = (b"DriverPackage", b"DriverStore", b"DriverRuntime", b"DriverBootPolicy", b"GraphicsDriverActivity",
                   b"GraphicsDriverSettingsActivity", b"BanjoTooieGameActivity", b"showGraphicsDriverSettings", b"getGraphicsDriverState",
                   b"recordGraphicsDriverSelection", b"recordGraphicsDriverBackend", b"recordGraphicsMenuReady")
        def check(entries):
            with io.BytesIO() as stream:
                with zipfile.ZipFile(stream, "w") as archive:
                    for name, content in entries.items(): archive.writestr(name, content)
                with zipfile.ZipFile(stream) as archive: check_driver_java(archive)
        check({"classes.dex": b"primary", "classes2.dex": b"\0".join(markers)})
        for missing in markers:
            with self.subTest(missing=missing), self.assertRaises(ValueError):
                check({"classes.dex": b"\0".join(m for m in markers if m != missing), "assets/classes2.dex": missing})


if __name__ == "__main__":
    unittest.main(verbosity=2)
