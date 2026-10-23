#!/usr/bin/env python3
"""Regressions for the APK verifier's DEX entry selection and marker policy.

These small ZIP fixtures isolate the entry-selection policy. The normal CI
packaging step still verifies the actual Gradle-built and signed Android APK.
"""
import contextlib
import io
import unittest
import zipfile

from verify_android import check_update_java


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


if __name__ == "__main__":
    unittest.main(verbosity=2)
