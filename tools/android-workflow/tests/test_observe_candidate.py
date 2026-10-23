#!/usr/bin/env python3
"""No-device controls for the existing release-candidate observer."""
import importlib.util
import io
import json
import subprocess
import sys
import threading
import time
import tempfile
import unittest
import zipfile
from pathlib import Path
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('candidate_observer', ROOT / 'observe_candidate.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
sys.path.insert(0, str(ROOT))
import tooie


class Observer(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.capture = module.Capture('fixture-adb', '7907333f', self.root)

    def identity(self, changes=None):
        replies = {'Connection': 'device', 'Model': 'Odin2 Portal',
                   'APK-Path': 'package:/data/app/~~abc/pkg-def/base.apk',
                   'Installed-APK-Hash': module.APK_SHA256 + '  /data/app/~~abc/pkg-def/base.apk',
                   'Package': 'Package [' + module.PACKAGE + '] userId=10042 versionCode=2',
                   'Initial-PIDs': '12345', 'Logcat-Help': 'Usage: logcat (UID option omitted from help)',
                   'Logcat-UID-Probe': '',
                   'Start-Device-Time': '1792760000'}
        replies.update(changes or {})
        calls = []
        def read(name, args, allowed=(0,)):
            calls.append(args)
            return replies[name]
        with patch.object(self.capture, 'read', side_effect=read):
            uid = self.capture.identity()
        return uid, calls

    def test_exact_release_identity_uses_only_read_commands(self):
        uid, calls = self.identity()
        self.assertEqual(uid, '10042')
        expected = {'get-state', 'getprop', 'pm', 'sha256sum', 'dumpsys', 'pidof', 'logcat', 'date'}
        for args in calls:
            verb = args[3] if args[0] == 'shell' else args[0]
            self.assertIn(verb, expected)
        self.assertIn(module.PACKAGE, calls[2])

    def test_unadvertised_but_accepted_uid_uses_actual_device_read(self):
        uid, calls = self.identity({'Logcat-Help': 'Usage: logcat (no UID advertised)'})
        self.assertEqual(uid, '10042')
        self.assertNotIn(['logcat', '--help'], calls)
        self.assertIn(['shell', '-n', '-T', 'logcat', '--uid=10042',
                       '-b', 'all', '-d', '-t', '1', '-v', 'epoch'], calls)

    def test_probe_rejects_returned_help_even_if_process_exit_is_zero(self):
        for text in ('Usage: logcat', 'Android Debug Bridge version 1.0.41', 'logcat: unknown option --uid'):
            with self.subTest(text=text), self.assertRaises(RuntimeError):
                self.identity({'Logcat-UID-Probe': text})

    def test_failed_uid_device_probe_is_not_retried(self):
        result = subprocess.CompletedProcess([], 1, b'', b'unknown option --uid')
        args = ['shell', '-n', '-T', 'logcat', '--uid=10042', '-d', '-t', '1']
        with patch.object(module.subprocess, 'run', return_value=result) as run:
            with self.assertRaises(RuntimeError):
                self.capture.read('Logcat-UID-Probe', args)
        self.assertEqual(run.call_count, 1)
        self.assertEqual(json.loads((self.root / 'Logcat-UID-Probe-status.json').read_text())['returncode'], 1)
        self.assertIn(b'unknown option', (self.root / 'Logcat-UID-Probe-stderr.txt').read_bytes())

    def test_actual_cli_routes_without_a_debug_artifact_or_installer(self):
        with patch.object(tooie, 'load_profile', return_value={'device': {'serial': '7907333f'}}), \
             patch.object(tooie, 'default_adb', return_value='fixture-adb'), \
             patch.object(tooie.observe_candidate, 'observe', return_value=0) as observe, \
             patch.object(tooie, 'select_artifact') as artifact, \
             patch.object(tooie, 'capture') as installer:
            code = tooie.main(['--downloads', str(self.root), '--tool-root', str(self.root), 'observe-candidate'])
        self.assertEqual(code, 0)
        self.assertEqual(observe.call_args.args[:2], ('fixture-adb', '7907333f'))
        artifact.assert_not_called()
        installer.assert_not_called()

    def test_wrong_hash_code_path_or_missing_process_cannot_pass(self):
        controls = [{'Installed-APK-Hash': '0' * 64}, {'Package': 'userId=10042 versionCode=3'},
                    {'APK-Path': 'package:/data/app/a/base.apk;touch x'},
                    {'APK-Path': 'package:/data/app/a/base.apk\npackage:/data/app/b/base.apk'},
                    {'Initial-PIDs': ''}, {'Package': 'versionCode=2'}]
        for change in controls:
            with self.subTest(change=change), self.assertRaises(RuntimeError):
                self.identity(change)

    def test_timeout_is_unknown_and_never_retried(self):
        exc = subprocess.TimeoutExpired(['fixture-adb'], 12, output=b'partial', stderr=b'blocked')
        with patch.object(module.subprocess, 'run', side_effect=exc) as run:
            with self.assertRaises(RuntimeError):
                self.capture.read('PID', ['shell', '-n', '-T', 'pidof', module.PACKAGE])
        self.assertEqual(run.call_count, 1)
        status = json.loads((self.root / 'PID-status.json').read_text())
        self.assertTrue(status['timed_out'])
        self.assertIsNone(status['returncode'])
        self.assertIn('not evidence', status['meaning'])
        self.assertEqual((self.root / 'PID.txt').read_bytes(), b'partial')

    def test_failed_preflight_still_saves_one_archive_without_readers(self):
        downloads, work = self.root / 'downloads', self.root / 'work'
        with patch.object(module.sys, 'stdin') as stdin, \
             patch.object(module.sys, 'stdout', io.StringIO()), \
             patch.object(module.Capture, 'identity', side_effect=RuntimeError('Unknown connection read')), \
             patch.object(module.Capture, 'stream') as stream:
            stdin.isatty.return_value = True
            result = module.observe('fixture-adb', '7907333f', downloads, work)
        self.assertEqual(result, 1)
        stream.assert_not_called()
        archives = list(downloads.glob('*.zip'))
        self.assertEqual(len(archives), 1)
        with zipfile.ZipFile(archives[0]) as bundle:
            session = json.loads(bundle.read('Session.json'))
            self.assertFalse(session['install_performed'])
            self.assertFalse(session['launch_performed'])
            self.assertIn('Unknown connection', session['failure'])

    def test_host_logcat_reader_stops_and_retains_actual_pipe_bytes(self):
        fixture = self.root / 'fixture-adb'
        fixture.write_text('#!/usr/bin/env python3\nimport signal\nprint("fixture release log", flush=True)\nsignal.pause()\n')
        fixture.chmod(0o700)
        capture = module.Capture(fixture, '7907333f', self.root)
        capture.stream('Pipe', ['logcat', '--uid=10042', '-v', 'epoch', '-T', '1'])
        self.addCleanup(capture.close)
        process = capture.streams[0]['process']
        limit = time.monotonic() + 2
        while time.monotonic() < limit and capture.streams[0]['retained'] == 0:
            threading.Event().wait(0.01)
        capture.close()
        self.assertIsNotNone(process.poll())
        self.assertIn(b'fixture release log', (self.root / 'Pipe.txt').read_bytes())
        self.assertFalse(json.loads((self.root / 'Pipe-status.json').read_text())['reader_still_running'])

    def test_start_failure_and_nonzero_are_retained(self):
        with patch.object(module.subprocess, 'run', side_effect=OSError('no adb')):
            with self.assertRaises(RuntimeError):
                self.capture.read('Missing', ['get-state'])
        self.assertTrue(json.loads((self.root / 'Missing-status.json').read_text())['start_failed'])
        result = subprocess.CompletedProcess([], 1, b'', b'not running')
        with patch.object(module.subprocess, 'run', return_value=result):
            self.assertEqual(self.capture.read('PID', ['shell', '-n', '-T', 'pidof', module.PACKAGE], allowed=(0, 1)), '')

    def test_release_crash_blocks_are_scoped_by_reporter_pid(self):
        data = (b'1792760000.000 99 99 F DEBUG : header\n'
                b'1792760000.001 99 99 F DEBUG : >>> ' + module.PACKAGE.encode() + b' <<<\n'
                b'1792760000.002 99 99 F DEBUG : stack\n'
                b'1792760000.003 88 88 E AndroidRuntime : unrelated app\n')
        scoped = module.crash_scope(data)
        self.assertIn(b'header', scoped)
        self.assertIn(b'stack', scoped)
        self.assertNotIn(b'unrelated', scoped)

    def test_one_archive_keeps_notes_and_marks_excerpts(self):
        self.capture.event('bug-note', '1 aiming seems reversed')
        (self.root / 'Session.json').write_text(json.dumps({'clock': None}))
        (self.root / 'App-Logcat.txt').write_bytes(b'first\n' + b'middle\n' * 80 + b'last\n')
        (self.root / 'Crash-Buffer.txt').write_bytes(b'1792760000.000 88 88 E AndroidRuntime : unrelated\n')
        output = self.root.parent / (self.root.name + '-Share.zip')
        self.addCleanup(lambda: output.unlink(missing_ok=True))
        with patch.object(module, 'SHARE_LIMIT', 100):
            module.package(self.root, output)
        with zipfile.ZipFile(output) as bundle:
            self.assertIsNone(bundle.testzip())
            self.assertIn('1 aiming seems reversed', bundle.read('Notes.jsonl').decode())
            self.assertEqual(bundle.read('Crash-Buffer.txt'), b'')
            manifest = json.loads(bundle.read('Share-Manifest.json'))
            record = next(x for x in manifest['files'] if x['raw_name'] == 'App-Logcat.txt')
            self.assertTrue(record['size_excerpt'])
            self.assertIn(b'first', bundle.read(record['included_name']))
            self.assertIn(b'last', bundle.read(record['included_name']))

    def test_numbered_notes_gameplay_restart_and_finish_use_one_input(self):
        lines = [b'1 menu note\nplaying\n', b'restart\nresumed\n2 sound note\nsave\n']
        with patch.object(module.select, 'select', return_value=([object()], [], [])), \
             patch.object(module.os, 'read', side_effect=lines), \
             patch.object(module.sys, 'stdin') as stdin, patch.object(module.sys, 'stdout', io.StringIO()):
            stdin.fileno.return_value = 0
            self.capture.interact()
        self.assertEqual([x['kind'] for x in self.capture.events],
                         ['bug-note', 'playing', 'restart', 'resumed', 'bug-note', 'finish'])


if __name__ == '__main__':
    unittest.main()
