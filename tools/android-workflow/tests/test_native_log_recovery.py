"""Real local byte-range pipelines and archive consumers; no real ADB/device."""
import base64
import contextlib
import importlib.util
import io
import json
from pathlib import Path
import random
import shlex
import subprocess
import tempfile
import unittest
from unittest.mock import patch
import zipfile

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('recover_native_fixture', ROOT / 'recover_native_log.py')
recovery = importlib.util.module_from_spec(spec)
spec.loader.exec_module(recovery)


class Device(recovery.Device):
    def __init__(self, root, apk, *, corrupt=False, short=False, grow=False,
                 windows=False, invalid_encoded=False):
        super().__init__('fixture-adb', 'fixture-device')
        self.root, self.apk = root, apk
        self.calls = []
        self.corrupt, self.short, self.grow = corrupt, short, grow
        self.windows, self.invalid_encoded = windows, invalid_encoded
        self.reads = 0

    def wire(self, data):
        return data.replace(b'\n', b'\r\n') if self.windows else data

    def command(self, *args):
        self.calls.append(args)
        if args == ('get-state',): return self.wire(b'device\n')
        if args == ('get-serialno',): return self.wire(b'fixture-device\n')
        if args == ('shell', '-T', 'pm path ' + recovery.PACKAGE):
            return self.wire(b'package:/data/app/~~fixture/package==/base.apk\n')
        if args[:2] == ('shell', '-T') and shlex.split(args[-1]) == ['sha256sum', '/data/app/~~fixture/package==/base.apk']:
            return self.wire((self.apk + '  /data/app/~~fixture/package==/base.apk\n').encode())
        self_test = shlex.split(args[-1])
        if self_test[:4] != ['run-as', recovery.PACKAGE, 'sh', '-c'] or len(self_test) != 5:
            raise AssertionError('Unexpected device operation: ' + repr(args))
        script = self_test[4]
        data_read = script.startswith('dd ') and script.endswith(' | base64')
        if data_read:
            self.reads += 1
            if self.grow and self.reads == 1:
                with (self.root / recovery.LOG).open('ab') as stream: stream.write(b'after snapshot\n')
        result = subprocess.run(['sh', '-c', script], cwd=self.root, capture_output=True, check=True)
        data = result.stdout
        if data_read and self.reads == 1:
            decoded = base64.b64decode(data)
            if self.corrupt: decoded = bytes([decoded[0] ^ 1]) + decoded[1:]
            if self.short: decoded = decoded[:-1]
            data = base64.encodebytes(decoded)
            if self.invalid_encoded: data = b'!' + data[1:]
        return self.wire(data)


class NativeRecovery(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='tooie-native-recovery-')
        self.root = Path(self.temporary.name)
        self.prefix = b'p' * (3 * 1024 * 1024 - 2000)
        self.appended = random.Random(17).randbytes(145_003)
        native = self.root / recovery.LOG
        native.parent.mkdir(parents=True)
        native.write_bytes(self.prefix + self.appended)
        self.apk = recovery.digest(b'correct signed APK')
        self.identity = dict(source_commit='a' * 40, apk_sha256=self.apk, serial='fixture-device',
                             baseline_bytes=len(self.prefix), baseline_sha256=recovery.digest(self.prefix),
                             original_pid='27464', capture_share_sha256='b' * 64, original_archive='fixture-Evidence.zip')
        self.patches = patch.multiple(recovery, CHUNK=4096, PART_TARGET=19000)
        self.patches.start()

    def tearDown(self):
        self.patches.stop()
        self.temporary.cleanup()

    def consume(self, outputs):
        parts = []
        for output in outputs:
            with zipfile.ZipFile(output) as archive:
                self.assertIsNone(archive.testzip())
                row = json.loads(archive.read('recovery.json'))
                data = archive.read(row['entry'])
                self.assertEqual(len(data), row['length'])
                self.assertEqual(recovery.digest(data), row['sha256'])
                parts.append((row, data))
        cursor = len(self.prefix)
        combined = b''
        for row, data in parts:
            self.assertEqual(row['offset'], cursor)
            self.assertEqual(row['appended_sha256'], recovery.digest(self.appended))
            cursor += len(data); combined += data
        self.assertEqual(combined, self.appended)
        self.assertGreater(len(outputs), 1)

    def capture_fixture(self, baseline, *, loose=False):
        original = 'Tooie-fixture-Evidence.zip'
        prefix = original.removesuffix('-Evidence.zip') + '-'
        member = prefix + 'Baseline.txt'
        manifest = dict(source_commit='a' * 40, original_archive=original, original_pid='27464', sources=[dict(
            name=member, size_bytes=len(baseline), sha256=recovery.digest(baseline))])
        identity = 'commit=' + 'a' * 40 + '\nhash=' + self.apk + '\nserial=fixture-device\n'
        with zipfile.ZipFile(self.root / original, 'w', zipfile.ZIP_DEFLATED) as archive:
            archive.writestr(member, baseline)
        if loose:
            (self.root / member).write_bytes(baseline)
        share = self.root / 'share.zip'
        with zipfile.ZipFile(share, 'w') as archive:
            archive.writestr('share-manifest.json', json.dumps(manifest))
            archive.writestr(prefix + 'Identity.txt', identity)
        return share, recovery.capture_identity(share, recovery.file_digest(share))

    def test_real_ranges_roll_parts_and_preserve_exact_snapshot_despite_append(self):
        device = Device(self.root, self.apk, grow=True)
        outputs = recovery.recover(device, self.identity, self.root)
        self.consume(outputs)
        self.assertTrue(all(path.stat().st_size <= recovery.MAX_UPLOAD for path in outputs))
        self.assertEqual((self.root / recovery.LOG).read_bytes(), self.prefix + self.appended + b'after snapshot\n')
        self.assertGreater(device.reads, 1)

    def test_wrong_serial_apk_and_prefix_stop_before_data_export(self):
        for reason in ('serial', 'apk', 'prefix'):
            with self.subTest(reason=reason):
                identity = dict(self.identity)
                identity[{'serial': 'serial', 'apk': 'apk_sha256', 'prefix': 'baseline_sha256'}[reason]] = 'wrong'
                device = Device(self.root, self.apk)
                with self.assertRaises(RuntimeError): recovery.recover(device, identity, self.root)
                self.assertEqual(device.reads, 0)
                self.assertFalse(list(self.root.glob('*Native-Recovery-*')))

    def test_short_and_corrupt_reads_never_publish_verified_archives(self):
        for arguments, message in [({'short': True}, 'Short native-log read'), ({'corrupt': True}, 'Recovered bytes differ')]:
            with self.subTest(arguments=arguments):
                device = Device(self.root, self.apk, **arguments)
                before = set(self.root.glob('*Native-Recovery-*'))
                with self.assertRaisesRegex(RuntimeError, message): recovery.recover(device, self.identity, self.root)
                folder, = set(self.root.glob('*Native-Recovery-*')) - before
                self.assertFalse(list(folder.glob('*.zip')))
                self.assertFalse((folder / 'recovery-complete.json').exists())
                self.assertFalse(json.loads((folder / 'recovery-failed.json').read_text())['verified'])
                if arguments.get('short'): self.assertEqual(device.reads, 1)

    def test_share_pin_and_baseline_identity(self):
        share, result = self.capture_fixture(self.prefix)
        self.assertEqual(result['baseline_bytes'], len(self.prefix))
        self.assertEqual(result['apk_sha256'], self.apk)
        with self.assertRaisesRegex(RuntimeError, 'Share ZIP digest differs'):
            recovery.capture_identity(share, '0' * 64)

    def test_windows_line_expansion_round_trips_raw_bytes_through_real_cli(self):
        # The archived CRLF pair crosses a read boundary; legitimate CRLF and
        # a trailing CR must also survive the inverse Windows conversion.
        self.prefix = b'p' * (recovery.CHUNK - 1) + b'\nmsg\r\n\x00\x1a\xff\n' + b'x' * recovery.CHUNK + b'\r'
        (self.root / recovery.LOG).write_bytes(self.prefix + self.appended)
        share, identity = self.capture_fixture(self.prefix.replace(b'\n', b'\r\n'))
        device = Device(self.root, self.apk, windows=True, grow=True)
        state = self.root / 'capture-state'; state.mkdir()
        output = io.StringIO()
        with patch('sys.argv', ['recovery', '--share', str(share), '--expect-share-sha256', recovery.file_digest(share),
                                '--adb', 'fixture-adb', '--serial', 'fixture-device', '--state', str(state)]), \
                patch.object(recovery, 'Device', return_value=device), contextlib.redirect_stdout(output):
            recovery.main()
        outputs = [Path(line.removeprefix('UPLOAD: ')) for line in output.getvalue().splitlines() if line.startswith('UPLOAD: ')]
        self.consume(outputs)
        self.assertIn('PASS:', output.getvalue())
        with zipfile.ZipFile(outputs[0]) as archive:
            record = json.loads(archive.read('recovery.json'))
        self.assertEqual(record['native_baseline_bytes'], len(self.prefix))
        self.assertEqual(record['native_baseline_sha256'], recovery.digest(self.prefix))
        self.assertEqual(record['baseline_bytes'], identity['baseline_bytes'])
        self.assertGreater(record['baseline_bytes'], record['native_baseline_bytes'])
        self.assertEqual(record['baseline_mapping'], 'windows-crlf-to-lf')
        self.assertEqual(record['transfer_encoding'], 'base64-decoded-to-raw-bytes')

    def test_unchanged_native_crlf_uses_exact_archive_candidate(self):
        self.prefix = b'raw\r\nfile\r\nend\r'
        (self.root / recovery.LOG).write_bytes(self.prefix + self.appended)
        share, identity = self.capture_fixture(self.prefix, loose=True)
        # Both raw baseline sources are supported, with no unverified offset.
        candidates = recovery.baseline_candidates(share, identity)
        outputs = recovery.recover(Device(self.root, self.apk), identity, self.root, candidates)
        self.consume(outputs)
        with zipfile.ZipFile(outputs[0]) as archive:
            self.assertEqual(json.loads(archive.read('recovery.json'))['baseline_mapping'], 'as-archived')

    def test_archived_baseline_digest_is_mandatory(self):
        share, identity = self.capture_fixture(self.prefix)
        with zipfile.ZipFile(self.root / identity['original_archive'], 'w') as archive:
            archive.writestr(identity['baseline_entry'], b'q' + self.prefix[1:])
        with self.assertRaisesRegex(RuntimeError, 'Archived baseline digest differs'):
            recovery.baseline_candidates(share, identity)
        self.assertFalse(list(self.root.glob('*Native-Recovery-*')))

    def test_changed_native_prefix_rejects_both_verified_baseline_candidates(self):
        self.prefix = b'original log\n' * 200
        share, identity = self.capture_fixture(self.prefix.replace(b'\n', b'\r\n'))
        (self.root / recovery.LOG).write_bytes(b'X' + self.prefix[1:] + self.appended)
        device = Device(self.root, self.apk, windows=True)
        candidates = recovery.baseline_candidates(share, identity)
        self.assertEqual(len(candidates), 2)
        with self.assertRaisesRegex(RuntimeError, 'Existing log prefix differs'):
            recovery.recover(device, identity, self.root, candidates)
        self.assertEqual(device.reads, 0)
        self.assertFalse(list(self.root.glob('*Native-Recovery-*')))

    def test_encoded_corruption_stops_without_a_verified_export(self):
        with self.assertRaisesRegex(RuntimeError, 'Invalid encoded native-log read'):
            recovery.recover(Device(self.root, self.apk, invalid_encoded=True), self.identity, self.root)
        folder, = self.root.glob('*Native-Recovery-*')
        self.assertFalse(list(folder.glob('*.zip')))
        self.assertFalse((folder / 'recovery-complete.json').exists())

    def test_native_script_rejects_unknown_operations_and_invalid_ranges(self):
        for operation, start, length in [('remove', 0, 10), ('bytes', -1, 10), ('bytes', 0, 0), ('bytes', 0, recovery.MAX_APPENDED + 1), ('bytes', True, 1)]:
            with self.assertRaises(RuntimeError): recovery.native_script(operation, start, length)

    def test_existing_capture_lock_blocks_recovery(self):
        state = self.root / 'capture-state'; state.mkdir()
        with recovery.device_lock(state):
            with self.assertRaisesRegex(RuntimeError, 'still holds the device lock'):
                with recovery.device_lock(state): self.fail('Acquired the active capture lock')
        with recovery.device_lock(state): pass
        with self.assertRaisesRegex(RuntimeError, 'folder is missing'):
            with recovery.device_lock(self.root / 'missing-state'): self.fail('Created missing capture state')


if __name__ == '__main__':
    unittest.main()
