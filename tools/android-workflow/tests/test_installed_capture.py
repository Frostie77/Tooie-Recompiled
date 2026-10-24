"""Real collector/packager with mocked device and SDK boundaries; no device access."""
from pathlib import Path
import contextlib
import importlib.util
import io
import json
import subprocess
import tempfile
import types
import unittest
from unittest.mock import patch
import zipfile

ROOT = Path(__file__).resolve().parents[1]


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


capture = module('installed_capture_tests', ROOT / 'capture_installed.py')
workflow = module('installed_workflow_tests', ROOT / 'tooie.py')


class InstalledCapture(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='tooie-installed-capture-test-')
        self.area = Path(self.temp.name)
        self.c = module('installed_collector_fixture', ROOT / 'templates/collector.py')
        self.c.prefix = self.area / ('Tooie-' + 'a' * 12 + '-Installed-Test')
        self.c.DOWNLOADS = self.area
        self.original_command = self.c.command
        self.source = 'a' * 40
        self.cert = 'c' * 64
        self.version = '0.1.0-ci.fixture'
        native = bytearray(64)
        native[:6] = b'\x7fELF\x02\x01'
        native[18:20] = b'\xb7\x00'
        native.extend(self.version.encode())
        self.native = bytes(native)
        self.apk = self.area / 'signed.apk'
        with zipfile.ZipFile(self.apk, 'w') as archive:
            archive.writestr('lib/arm64-v8a/libmain.so', self.native)
            archive.writestr('classes.dex', b'fixture-dex')
            archive.writestr('AndroidManifest.xml', b'fixture-manifest')
        self.sha = capture.digest(self.apk.read_bytes())
        self.receipt = {'source_commit': self.source, 'apk_sha256': self.sha,
                        'certificate_sha256': self.cert, 'all_unsigned_payload_matches': True}
        self.report = {'source_commit': self.source, 'native_version': self.version,
                       'libraries': {'libmain.so': {'sha256': capture.digest(self.native), 'build_id': 'b' * 40}}}
        self.receipt_path, self.report_path = self.area / 'verification.json', self.area / 'build-report.json'
        self.receipt_path.write_text(json.dumps(self.receipt))
        self.report_path.write_text(json.dumps(self.report))
        self.args = types.SimpleNamespace(apk=self.apk, verification=self.receipt_path,
            build_report=self.report_path, build_tools=self.area, expect_source=self.source,
            expect_sha256=self.sha, expect_cert=self.cert, downloads=self.area, tool_revision='test-tool')
        self.calls = []
        def command(args, *positional, **kwargs):
            values = list(map(str, args))
            self.calls.append(values)
            if values[1] == 'verify':
                output = 'Signer #1 certificate SHA-256 digest: ' + self.cert + '\n'
            else:
                output = "package: name='io.github.banjotooierecompiled' versionCode='1' versionName='0.1.0-android'\napplication-debuggable\n"
            return subprocess.CompletedProcess(values, 0, output.encode(), b'')
        self.c.command = command

    def tearDown(self):
        self.temp.cleanup()

    def test_real_local_apk_checks_use_expected_source_and_signer(self):
        receipt, report, build_id, _, _ = capture.verify_local(self.c, self.args)
        self.assertEqual(receipt, self.receipt)
        self.assertEqual(report, self.report)
        self.assertEqual(build_id, 'b' * 40)
        self.assertEqual(len(self.calls), 2)

    def test_wrong_bytes_source_and_signer_stop_before_capture(self):
        self.args.expect_sha256 = '0' * 64
        with self.assertRaises(RuntimeError): capture.verify_local(self.c, self.args)
        self.assertEqual(self.calls, [])
        self.args.expect_sha256 = self.sha
        self.report['source_commit'] = 'd' * 40
        self.report_path.write_text(json.dumps(self.report))
        with self.assertRaises(RuntimeError): capture.verify_local(self.c, self.args)
        self.assertEqual(self.calls, [])
        self.report['source_commit'] = self.source
        self.report_path.write_text(json.dumps(self.report))
        self.cert = 'e' * 64
        with self.assertRaises(RuntimeError): capture.verify_local(self.c, self.args)

    def test_observer_guard_blocks_mutations_but_accepts_marker_text(self):
        base = ['adb', '-s', 'device']
        for operation in (['install', '-r', 'old.apk'], ['uninstall', 'pkg'],
                          ['shell', '-T', 'am start -n pkg/.Activity'],
                          ['shell', '-T', 'am force-stop pkg'], ['shell', '-T', 'pm clear pkg'],
                          ['shell', '-T', 'input tap 1 2'], ['shell', '-T', 'logcat -c']):
            with self.assertRaises(RuntimeError): capture.guard_observer_command(base + operation, 'adb', 'device')
        capture.guard_observer_command(base + ['shell', '-T', 'pm path io.github.banjotooierecompiled'], 'adb', 'device')
        capture.guard_observer_command(base + ['shell', '-T', "log -p i -t TooieUserMark '{\"note\":\"am start; pm clear\"}'"], 'adb', 'device')
        capture.guard_observer_command(base + ['exec-out', 'screencap', '-p'], 'adb', 'device')

    def test_wrong_installed_apk_stops_without_starting_a_capture(self):
        self.c.SERIAL = 'fixture-device'
        def adb(*args, **kwargs):
            if args == ('get-state',): payload = 'device'
            elif args == ('get-serialno',): payload = 'fixture-device'
            elif args[-1].startswith('pm path '): payload = 'package:/data/app/fixture/base.apk'
            elif args[-1].startswith('sha256sum '): payload = '0' * 64 + '  /data/app/fixture/base.apk'
            else: raise AssertionError('Unexpected device operation: ' + repr(args))
            return subprocess.CompletedProcess(args, 0, payload.encode(), b'')
        self.c.adb = adb
        self.c.start_capture = lambda start: self.fail('Started capture for the wrong installed APK')
        class Terminal(io.StringIO):
            def isatty(self): return True
        with patch.object(self.c.sys, 'stdin', Terminal()), self.assertRaisesRegex(RuntimeError, 'Installed APK differs'):
            capture.installed_main(self.c, self.args)
        self.assertFalse(self.c.attempted)

    def test_continuous_observation_runs_past_old_timer_and_stops_on_save(self):
        self.c.input_hub = lambda: types.SimpleNamespace(phase='')
        self.c.require_original_process = lambda name: None
        self.c.health = lambda: None
        iterations = []
        def wait(seconds, phase):
            iterations.append((seconds, phase))
            if len(iterations) == 4: self.c.finish_requested = True
        self.c.wait_with_live_notes = wait
        clock = iter([0, 601, 1201, 1801, 3601, 3602])
        with patch.object(capture.time, 'monotonic', side_effect=lambda: next(clock)), contextlib.redirect_stdout(io.StringIO()):
            capture.observe_installed(self.c)
        self.assertEqual(len(iterations), 4)
        window = json.loads(Path(str(self.c.prefix) + '-Gameplay-01-Window.txt').read_text())
        self.assertIsNone(window['requested_seconds'])
        self.assertTrue(window['user_requested_finish'])
        self.assertGreater(window['host_end_monotonic_seconds'], 3600)

    def test_follow_up_keeps_repeated_numbers_and_attached_notes(self):
        events = [dict(id='first', sequence=1, marker=30, kind='event', host_read_wall='t1', description='first bug'),
                  dict(id='note', sequence=2, marker=30, kind='note', host_read_wall='t2', description='detail', target_event_id='first'),
                  dict(id='second', sequence=3, marker=30, kind='event', host_read_wall='t3', description='another occurrence')]
        result = capture.follow_up_records(events)
        self.assertEqual(result['events_recorded'], 2)
        self.assertEqual(result['notes_recorded'], 1)
        self.assertEqual([x['event_id'] for x in result['items']], ['first', 'second'])
        self.assertEqual(result['items'][0]['notes'][0]['description'], 'detail')
        self.assertEqual(result['items'][1]['notes'], [])

    def test_large_marker_records_are_kept_in_single_share_upload(self):
        prefix = 'Tooie-' + self.source[:12] + '-Installed-Test'
        evidence = self.area / (prefix + '-Evidence.zip')
        rows = ''.join(json.dumps(dict(id=str(i), marker=1 + i % 30, kind='event', sequence=i,
                                      description='x' * 240)) + '\n' for i in range(1500))
        self.assertGreater(len(rows), 256 * 1024)
        with zipfile.ZipFile(evidence, 'w') as archive:
            archive.writestr(prefix + '-Identity.txt', 'commit=' + self.source + '\n')
            archive.writestr(prefix + '-User-Markers.txt', rows)
            archive.writestr(prefix + '-Live-Input-Journal.txt', rows)
            archive.writestr(prefix + '-User-Follow-Up.txt', rows)
        with contextlib.redirect_stdout(io.StringIO()):
            output, summary = workflow.package_evidence(evidence, {'identity': {'commit': self.source}})
        with zipfile.ZipFile(output) as archive:
            self.assertEqual(archive.read(prefix + '-User-Markers.txt').decode(), rows)
            self.assertEqual(archive.read(prefix + '-Live-Input-Journal.txt').decode(), rows)
            self.assertEqual(archive.read(prefix + '-User-Follow-Up.txt').decode(), rows)
            inventory = json.loads(archive.read('share-manifest.json'))
            self.assertTrue(all(x['mode'] == 'verbatim' for x in inventory['sources']))
        self.assertEqual(len(summary['user_markers']), 1500)

    def test_installed_entrypoint_attaches_and_packages_all_markers(self):
        self.c.ADB, self.c.SERIAL = 'fixture-adb', 'fixture-device'
        operations = []
        def adb(*args, **kwargs):
            capture.guard_observer_command(['fixture-adb', '-s', 'fixture-device', *args],
                                           'fixture-adb', 'fixture-device')
            operations.append(args)
            if args == ('get-state',): payload = 'device'
            elif args == ('get-serialno',): payload = 'fixture-device'
            elif args[-1].startswith('pm path '): payload = 'package:/data/app/fixture/base.apk'
            elif args[-1].startswith('sha256sum '): payload = self.sha + '  /data/app/fixture/base.apk'
            elif args[-1].startswith('run-as '): payload = 'old log\n'
            elif args[-1].startswith('date '): payload = '2026-10-24 21:30:00'
            elif args[-1].startswith('dumpsys activity '):
                payload = 'mResumedActivity: ActivityRecord{abc u0 io.github.banjotooierecompiled/.BanjoTooieSDLActivity t1}'
            elif args[-1].startswith('log -p i -t TooieUserMark '): payload = ''
            else: raise AssertionError('Unexpected device operation: ' + repr(args))
            return subprocess.CompletedProcess(args, 0, payload.encode(), b'')
        self.c.adb = adb
        self.c.start_capture = lambda start: None
        self.c.pids = lambda name: (1234,)
        self.c.prompt = lambda message: 'playing'
        self.c.screenshot = lambda label: None
        self.c.stop_capture = lambda: None
        self.c.collect = lambda: self.c.archive_evidence()
        def observe(c):
            import time
            for number in range(1, 31):
                item = dict(text=str(number) + ' detail', host_read_wall='2026-10-24T21:30:00+11:00',
                            host_read_mono_seconds=time.monotonic(), phase_at_read='Gameplay-Installed')
                c.handle_live_note(item, 'Gameplay-Installed')
            c.handle_live_note({**item, 'text': 'note 30 later detail'}, 'Gameplay-Installed')
            c.finish_requested = True
        archive = self.c.archive_evidence
        def with_follow_up():
            self.c.save('User-Follow-Up', json.dumps(capture.follow_up_records(self.c.live_events)))
            return archive()
        self.c.archive_evidence = with_follow_up
        class Terminal(io.StringIO):
            def isatty(self): return True
        self.c.main = lambda: capture.installed_main(self.c, self.args)
        with patch.object(sys_module := self.c.sys, 'stdin', Terminal()), \
             patch.object(capture, 'observe_installed', observe), contextlib.redirect_stdout(io.StringIO()):
            code = self.c.run()
            evidence = Path(str(self.c.prefix) + '-Evidence.zip')
            upload, summary = workflow.package_evidence(evidence, {'identity': {'commit': self.source}})
        self.assertEqual(code, 0)
        self.assertEqual(summary['original_pid'], '1234')
        self.assertEqual(summary['source_commit'], self.source)
        self.assertEqual(len(self.c.live_events), 31)
        self.assertTrue(all(op[0] not in ('install', 'uninstall') for op in operations))
        with zipfile.ZipFile(upload) as bundle:
            follow = json.loads(bundle.read(self.c.prefix.name + '-User-Follow-Up.txt'))
            self.assertEqual(follow['events_recorded'], 30)
            self.assertEqual(follow['notes_recorded'], 1)
            self.assertEqual(follow['items'][-1]['notes'][0]['description'], 'later detail')

    def test_capture_scope_does_not_inherit_old_timing_claims(self):
        self.c.CAPTURE_SCOPE_OVERRIDE = 'Installed CI observation; timing flags are not inferred.\n'
        self.c.TIMING_SCOPE_OVERRIDE = 'Available appended diagnostics only.\n'
        with contextlib.redirect_stdout(io.StringIO()):
            self.c.collect()
        prefix = str(self.c.prefix)
        self.assertEqual(Path(prefix + '-Capture-Scope.txt').read_text(), self.c.CAPTURE_SCOPE_OVERRIDE)
        self.assertEqual(Path(prefix + '-Timing-Capture-Scope.txt').read_text(), self.c.TIMING_SCOPE_OVERRIDE)
        summary = Path(prefix + '-Evidence.txt').read_text()
        self.assertIn(self.c.CAPTURE_SCOPE_OVERRIDE, summary)
        self.assertNotIn('Timing observation includes', summary)

    def test_marker_budget_failure_preserves_raw_and_produces_no_partial_share(self):
        prefix = 'Tooie-' + self.source[:12] + '-Budget-Test'
        evidence = self.area / (prefix + '-Evidence.zip')
        rows = ''.join(json.dumps({'sequence': i, 'marker': 30, 'description': 'x' * 240}) + '\n'
                       for i in range(1500))
        with zipfile.ZipFile(evidence, 'w') as archive:
            archive.writestr(prefix + '-Identity.txt', 'commit=' + self.source + '\n')
            archive.writestr(prefix + '-User-Markers.txt', rows)
        before = capture.digest(evidence.read_bytes())
        with patch.object(workflow, 'UPLOAD_LIMIT', 2048), self.assertRaisesRegex(RuntimeError, 'marker records exceed'):
            workflow.package_evidence(evidence, {'identity': {'commit': self.source}})
        self.assertEqual(capture.digest(evidence.read_bytes()), before)
        self.assertEqual(list(self.area.glob(prefix + '-Share-*.zip')), [])


if __name__ == '__main__':
    unittest.main()
