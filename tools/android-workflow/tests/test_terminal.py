"""Host-only capture tests; every device operation is replaced with a stub."""
from pathlib import Path
import ast
import contextlib
import importlib.util
import io
import json
import os
import pty
import queue
import shlex
import subprocess
import tempfile
import threading
import time
import unittest
from unittest.mock import patch
import zipfile

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'templates/collector.py'
ORIGINAL = ROOT / 'step100-review/handoff/output/Tooie-Step96-Player-Hierarchy-Icon-Odin-e1fcf93.py'

class OneWindow(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='tooie-one-window-')
        self.area = Path(self.temp.name)
        spec = importlib.util.spec_from_file_location('one_window_test', SOURCE)
        self.m = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(self.m)
        self.m.prefix = self.area / 'unique-capture'
        self.m.DOWNLOADS = self.area
        self.m.original_pid = '22851'
        self.commands = []
        def adb(*args, **kwargs):
            self.commands.append((args, kwargs))
            # The complete event must already be durable before device delivery.
            rows = self.rows('User-Markers')
            self.assertEqual(rows[-1]['delivery'], 'pending')
            return subprocess.CompletedProcess([], 0, b'', b'')
        self.m.adb = adb
        self.output = io.StringIO()
        self.redirect = contextlib.redirect_stdout(self.output)
        self.redirect.__enter__()

    def tearDown(self):
        if self.m.live_input is not None:
            self.m.live_input.close()
        self.redirect.__exit__(None, None, None)
        self.temp.cleanup()

    def path(self, suffix):
        return Path(str(self.m.prefix)+'-'+suffix+'.txt')

    def rows(self, suffix):
        return [json.loads(x) for x in self.path(suffix).read_text().splitlines()]

    def item(self, text):
        return {'text':text, 'host_read_wall':'2026-10-19T00:00:00.123+11:00',
                'host_read_mono_seconds':time.monotonic(), 'phase_at_read':'Gameplay-01'}

    def test_reader_split_unicode_burst_and_eof(self):
        read_fd, write_fd = os.pipe()
        with os.fdopen(read_fd, 'rb', buffering=0) as reader:
            hub = self.m.LiveInput(reader, self.path('Live-Input-Journal'))
            self.m.live_input = hub
            hub.phase = 'Gameplay-01'
            text = '1 NPC stretching\n10 羽毛\nnote 1 more detail'
            encoded = text.encode()
            boundary = encoded.index('羽'.encode())+1
            os.write(write_fd, encoded[:boundary])
            os.write(write_fd, encoded[boundary:])
            os.close(write_fd)
            items = [hub.items.get(timeout=2) for _ in range(4)]
            self.assertEqual([x['text'] for x in items[:-1]], text.splitlines())
            self.assertIsNone(items[-1])
            self.assertEqual(self.rows('Live-Input-Journal'), items[:-1])
            self.assertTrue(all(x['phase_at_read']=='Gameplay-01' for x in items[:-1]))
            self.assertEqual(sorted(x['host_read_mono_seconds'] for x in items[:-1]),
                             [x['host_read_mono_seconds'] for x in items[:-1]])
            hub.close()
            self.assertFalse(hub.thread.is_alive())

    def test_same_tty_prompt_records_marker_without_advancing(self):
        master, slave = pty.openpty()
        try:
            with os.fdopen(slave, 'r') as stream, patch.object(self.m.sys, 'stdin', stream):
                os.write(master, b'2 green NPC arms\nready\n')
                self.assertEqual(self.m.prompt('ready / stop: '), 'ready')
                self.assertEqual(self.m.live_events[0]['marker'], 2)
                self.assertEqual(self.m.live_events[0]['description'], 'green NPC arms')
                self.m.close_live_input()
                self.assertIn('green NPC arms', self.path('User-Marker-Summary').read_text())
        finally:
            os.close(master)

    def test_host_journal_keeps_time_while_adb_delivery_is_blocked(self):
        read_fd, write_fd = os.pipe()
        release = threading.Event()
        entered = threading.Event()
        with os.fdopen(read_fd, 'rb', buffering=0) as stream:
            hub = self.m.LiveInput(stream, self.path('Live-Input-Journal'))
            self.m.live_input = hub
            hub.phase = 'Gameplay-01'
            os.write(write_fd, b'1 first issue\n')
            item = hub.items.get(timeout=2)
            def blocked(*args, **kwargs):
                entered.set()
                if not release.wait(2): raise RuntimeError('test delivery timeout')
                return subprocess.CompletedProcess([], 0, b'', b'')
            self.m.adb = blocked
            worker = threading.Thread(target=self.m.handle_live_note, args=(item,'Gameplay-01'))
            worker.start()
            try:
                self.assertTrue(entered.wait(2))
                os.write(write_fd, b'2 second issue\n')
                second = hub.items.get(timeout=2)
                self.assertEqual(second['text'], '2 second issue')
                self.assertEqual(self.rows('Live-Input-Journal')[-1], second)
                self.assertTrue(worker.is_alive())
            finally:
                release.set()
                worker.join(2)
                os.close(write_fd)
            self.assertFalse(worker.is_alive())
            self.assertLess(second['host_read_mono_seconds'],
                            self.m.live_events[0]['host_handled_mono_seconds'])

    def test_repeated_quick_markers_and_later_description(self):
        for line in ('1', '10 tenth', '1 again', 'note 1 later explanation'):
            self.assertTrue(self.m.handle_live_note(self.item(line), 'Gameplay-01'))
        events = self.m.live_events
        self.assertEqual([x['marker'] for x in events], [1,10,1,1])
        self.assertEqual(len({x['id'] for x in events}),4)
        self.assertEqual(events[-1]['target_event_id'], events[-2]['id'])
        self.assertEqual([x['sequence'] for x in events],[1,2,3,4])
        self.assertTrue(all(x['delivery']=='command-acknowledged' for x in events))

    def test_unicode_shell_quotes_and_bounded_payload(self):
        short_description = "NPC ' $(printf bad); quote"
        self.assertTrue(self.m.handle_live_note(self.item('3 '+short_description), 'Gameplay-01'))
        short_words = shlex.split(self.commands[-1][0][2])
        self.assertEqual(len(short_words),6)
        self.assertEqual(json.loads(short_words[-1])['description'],short_description)
        description = "feathers ' $(printf bad); " + '羽' * 210
        self.assertTrue(self.m.handle_live_note(self.item('3 '+description), 'Gameplay-01'))
        command = self.commands[-1][0][2]
        words = shlex.split(command)
        self.assertEqual(words[:6], ['log','-p','i','-t','TooieUserMark',words[5]])
        self.assertEqual(len(words),6)
        self.assertLessEqual(len(words[-1].encode()),900)
        payload = json.loads(words[-1])
        self.assertTrue(payload['description_in_evidence'])
        self.assertEqual(self.rows('User-Markers')[-2]['description'], description)
        self.assertEqual(self.commands[0][1]['timeout'],5)

    def test_bad_marker_input_never_becomes_a_device_command(self):
        for value in ('0 issue','11 issue','done','save','continue'):
            self.assertFalse(self.m.handle_live_note(self.item(value), 'Gameplay-01'))
        for value in ('note 1 details','note 1','2 '+'x'*241,'3 bad\x01input'):
            self.assertTrue(self.m.handle_live_note(self.item(value), 'Gameplay-01'))
        self.assertEqual(self.commands,[])
        self.assertEqual(self.m.live_events,[])

    def test_marker_delivery_failure_retains_note_and_continues(self):
        def broken(*args, **kwargs): raise OSError('device unavailable')
        self.m.adb = broken
        self.assertTrue(self.m.handle_live_note(self.item('4 wall enemy'), 'Gameplay-01'))
        event = self.m.live_events[0]
        self.assertEqual(event['delivery'],'unconfirmed')
        self.assertIn('device unavailable', event['delivery_error'])
        self.assertEqual(self.rows('User-Markers')[0]['description'],'wall enemy')
        self.assertEqual(self.rows('User-Markers')[-1]['metadata']['delivery'],'unconfirmed')

    def test_save_ends_timed_window_and_keeps_pending_input(self):
        class Hub:
            phase = ''
            items = queue.Queue()
        hub = Hub()
        for value in ('5 enemy bug','save','6 more stretching'):
            hub.items.put(self.item(value))
        self.m.input_hub = lambda:hub
        self.m.wait_with_live_notes(0.02, 'Gameplay-01')
        self.assertEqual([x['marker'] for x in self.m.live_events], [5])
        self.assertTrue(self.m.finish_requested)
        self.assertEqual(hub.items.get_nowait()['text'], '6 more stretching')
        self.assertTrue(self.path('User-Finish-Request').is_file())

    def test_collect_still_runs_if_input_cleanup_fails(self):
        calls = []
        self.m.main = lambda:None
        def cleanup(): raise OSError('journal storage failure')
        self.m.close_live_input = cleanup
        self.m.collect = lambda:calls.append('collect')
        self.m.stop_capture = lambda:calls.append('stop')
        with patch.object(self.m.signal,'signal'):
            self.assertEqual(self.m.run(),0)
        self.assertEqual(calls,['collect','stop'])
        self.assertIn('journal storage failure',self.path('Live-Input-Cleanup-Error').read_text())

    def test_existing_archive_contains_journal_and_markers_together(self):
        self.path('Live-Input-Journal').write_text(json.dumps(self.item('7 stretching'))+'\n')
        self.m.handle_live_note(self.item('7 stretching'),'Gameplay-01')
        # Use the real unchanged archive implementation, no device access.
        bundles, index = self.m.archive_evidence()
        self.assertIsNone(index)
        self.assertEqual(len(bundles),1)
        with zipfile.ZipFile(bundles[0]) as archive:
            names=archive.namelist()
            self.assertTrue(any(x.endswith('-Live-Input-Journal.txt') for x in names))
            self.assertTrue(any(x.endswith('-User-Markers.txt') for x in names))
            self.assertIsNone(archive.testzip())

    def test_idle_reader_closes_without_enter_or_stdin_close(self):
        read_fd, write_fd = os.pipe()
        with os.fdopen(read_fd,'rb',buffering=0) as stream:
            hub = self.m.LiveInput(stream,self.path('Live-Input-Journal'))
            self.m.live_input = hub
            started=time.monotonic()
            hub.close()
            self.assertLess(time.monotonic()-started,1)
            self.assertFalse(hub.thread.is_alive())
        os.close(write_fd)


if __name__=='__main__':
    unittest.main(verbosity=2)
