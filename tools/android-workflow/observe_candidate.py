"""Read-only capture for the already installed, protected-key release candidate."""
import datetime
import hashlib
import json
import os
import re
import select
import subprocess
import sys
import threading
import time
import uuid
import zipfile
from pathlib import Path

PACKAGE = 'io.github.banjotooierecompiled.release'
SOURCE = 'af97304f201af98a2c735b671d0413792673273b'
APK_SHA256 = 'b02849bb4cd85fb6c3a70c373a5c302f81c1a13bb24aa16e81494746471c52e7'
RAW_LIMIT = 64 * 1024 * 1024
SHARE_LIMIT = 8 * 1024 * 1024


class Capture:
    def __init__(self, adb, serial, root):
        if not re.fullmatch(r'[A-Za-z0-9_.:-]+', serial):
            raise ValueError('Invalid device serial.')
        self.command = [str(adb), '-s', serial]
        self.root = Path(root)
        self.streams = []
        self.events = []
        self.clock = None
        self.stream_warning = False

    def event(self, kind, text):
        value = {'kind': kind, 'text': text, 'host_epoch': time.time(),
                 'host_time': datetime.datetime.now().astimezone().isoformat(),
                 'monotonic': time.monotonic()}
        self.events.append(value)
        with (self.root / 'Notes.jsonl').open('a', encoding='utf-8') as out:
            out.write(json.dumps(value, ensure_ascii=False) + '\n')

    def read(self, name, args, allowed=(0,)):
        started = time.time()
        try:
            result = subprocess.run(self.command + args, stdin=subprocess.DEVNULL,
                                    capture_output=True, timeout=12)
            output, error, code = result.stdout, result.stderr, result.returncode
            status = {'returncode': code, 'timed_out': False}
        except subprocess.TimeoutExpired as exc:
            output, error = exc.stdout or b'', exc.stderr or b''
            code = None
            status = {'returncode': None, 'timed_out': True,
                      'meaning': 'Unknown read result; not evidence that the app ended.'}
        except OSError as exc:
            output, error, code = b'', str(exc).encode(), None
            status = {'returncode': None, 'timed_out': False, 'start_failed': True}
        status.update(host_started=started, host_finished=time.time(), arguments=args)
        (self.root / (name + '.txt')).write_bytes(output)
        (self.root / (name + '-stderr.txt')).write_bytes(error)
        (self.root / (name + '-status.json')).write_text(json.dumps(status, indent=2) + '\n')
        if code not in allowed:
            raise RuntimeError(name + ' could not be verified. Read/status retained; no retry or app action.')
        return output.decode('utf-8', errors='replace').strip()

    def identity(self):
        if self.read('Connection', ['get-state']) != 'device':
            raise RuntimeError('ADB is not connected to the selected device.')
        self.read('Model', ['shell', '-n', '-T', 'getprop', 'ro.product.model'])
        value = self.read('APK-Path', ['shell', '-n', '-T', 'pm', 'path', PACKAGE])
        paths = value.splitlines()
        if len(paths) != 1 or not re.fullmatch(r'package:/data/app/[A-Za-z0-9_./+=~\-]+/base\.apk', paths[0]):
            raise RuntimeError('Expected one ordinary release base APK; path retained.')
        apk = paths[0][len('package:'):]
        digest = self.read('Installed-APK-Hash', ['shell', '-n', '-T', 'sha256sum', apk])
        if digest.split()[0:1] != [APK_SHA256]:
            raise RuntimeError('Installed release candidate differs from the verified signed APK.')
        package = self.read('Package', ['shell', '-n', '-T', 'dumpsys', 'package', PACKAGE])
        uids = set(re.findall(r'\buserId=(\d+)\b', package))
        versions = set(re.findall(r'\bversionCode=(\d+)\b', package))
        if len(uids) != 1 or versions != {'2'}:
            raise RuntimeError('Release UID or version code could not be verified.')
        pids = self.read('Initial-PIDs', ['shell', '-n', '-T', 'pidof', PACKAGE], allowed=(0, 1))
        if not pids or not all(word.isdigit() for word in pids.split()):
            raise RuntimeError('Release app is not confirmed running. Open Tooie Recompiled and leave it at the launcher.')
        help_text = self.read('Logcat-Help', ['logcat', '--help'])
        if '--uid' not in help_text:
            raise RuntimeError('This Logcat does not advertise UID filtering; no unfiltered app-log fallback.')
        before = time.time()
        device = self.read('Start-Device-Time', ['shell', '-n', '-T', 'date', '+%s'])
        after = time.time()
        if not device.isdigit():
            raise RuntimeError('Device clock read was not understood.')
        self.clock = {'device_epoch_seconds': int(device), 'host_request_epoch': before,
                      'host_reply_epoch': after,
                      'device_minus_host_interval_seconds': [int(device) - after, int(device) + 1 - before],
                      'meaning': 'Whole-second calibration bracket; marker device times are not measured exactly.'}
        return next(iter(uids))

    def stream(self, name, args):
        stderr = (self.root / (name + '-stderr.txt')).open('wb')
        try:
            process = subprocess.Popen(self.command + args, stdin=subprocess.DEVNULL,
                                       stdout=subprocess.PIPE, stderr=stderr, bufsize=0)
        except BaseException:
            stderr.close()
            raise
        state = {'name': name, 'arguments': args, 'process': process, 'stderr': stderr,
                 'received': 0, 'retained': 0}

        def drain():
            try:
                with (self.root / (name + '.txt')).open('wb') as output:
                    while True:
                        data = process.stdout.read(16384)
                        if not data:
                            break
                        state['received'] += len(data)
                        keep = data[:max(0, RAW_LIMIT - state['retained'])]
                        output.write(keep)
                        state['retained'] += len(keep)
            except OSError as error:
                state['reader_error'] = str(error)
        state['thread'] = threading.Thread(target=drain, daemon=True)
        state['thread'].start()
        self.streams.append(state)

    def close(self):
        for state in self.streams:
            process = state['process']
            state['exited_before_collector_stop'] = process.poll() is not None
            if process.poll() is None:
                try:
                    process.terminate()  # Only the host-side Logcat reader started above.
                except OSError as exc:
                    state['terminate_error'] = str(exc)
            try:
                process.wait(timeout=4)
            except subprocess.TimeoutExpired:
                try:
                    process.kill()
                except OSError as exc:
                    state['kill_error'] = str(exc)
                try:
                    process.wait(timeout=2)
                except subprocess.TimeoutExpired:
                    state['termination_unknown'] = True
            state['thread'].join(timeout=2)
            state['reader_still_running'] = state['thread'].is_alive()
            state['stderr'].close()
            process.stdout.close()
            status = {key: value for key, value in state.items()
                      if key not in ('process', 'thread', 'stderr')}
            status['returncode'] = process.poll()
            status['raw_truncated'] = state['received'] > state['retained']
            (self.root / (state['name'] + '-status.json')).write_text(json.dumps(status, indent=2) + '\n')

    def interact(self):
        print('\nCAPTURE IS ACTIVE. Use this ONE Ubuntu window for all notes.', flush=True)
        print('On Odin: import your original USA ROM if requested, check Controls/Settings, then Start Game.', flush=True)
        print('Type playing when you can move Banjo; that starts 15 gameplay minutes.', flush=True)
        print('At any time: 1-10 plus a short bug note, or save to finish this capture.', flush=True)
        print('For persistence: save INSIDE THE GAME; type restart before closing/reopening the release app, then resumed afterward.', flush=True)
        print('The terminal word save only finishes the capture. Do not type done.', flush=True)
        deadline, last_report, pending = None, time.monotonic(), b''
        overall = time.monotonic() + 3600
        while time.monotonic() < overall:
            now = time.monotonic()
            if any(s['process'].poll() is not None for s in self.streams) and not self.stream_warning:
                self.event('capture-warning', 'A Logcat reader ended; app state is unknown.')
                print('Logcat reader ended. This does not establish a crash. Notes can still be saved.', flush=True)
                self.stream_warning = True
            if deadline is not None and now >= deadline:
                print('Gameplay window finished. Check in-game save/restart if needed, then type save; continue adds five minutes.', flush=True)
                deadline = None
            if deadline is not None and now - last_report >= 60:
                print(f'Capture continues: {max(1, int((deadline-now+59)//60))} minutes left. Notes: 1-10 plus text. Finish: save.', flush=True)
                last_report = now
            if not select.select([sys.stdin], [], [], 0.5)[0]:
                continue
            chunk = os.read(sys.stdin.fileno(), 4096)
            if not chunk:
                self.event('input-ended', 'Terminal input ended; capture finalized.')
                return
            pending += chunk
            while b'\n' in pending:
                raw, pending = pending.split(b'\n', 1)
                line = raw.decode('utf-8', errors='replace').strip()[:2000]
                if not line:
                    continue
                command = line.casefold()
                if re.fullmatch(r'(?:10|[1-9])\s+\S.*', line):
                    self.event('bug-note', line)
                    print('Note saved.', flush=True)
                elif command == 'save':
                    self.event('finish', 'User requested capture save.')
                    return
                elif command == 'playing':
                    self.event('playing', 'User reports controllable gameplay.')
                    if deadline is None:
                        deadline, last_report = time.monotonic() + 900, time.monotonic()
                        print('15 gameplay minutes started. Walk, turn, jump and try egg aiming; note delays, visual/audio issues.', flush=True)
                elif command == 'continue':
                    deadline = max(deadline or 0, time.monotonic()) + 300
                    self.event('extend', 'User added five minutes.')
                elif command in ('restart', 'resumed'):
                    self.event(command, 'User reported ' + command + '; no app command was sent.')
                    print('Restart marker saved. Check your progress/settings after reopening.', flush=True)
                else:
                    self.event('unrecognized-input', line)
                    print('Use playing, 1-10 plus a note, restart, resumed, continue, or save.', flush=True)
        self.event('time-limit', 'One-hour capture limit reached.')


def crash_scope(data):
    lines = data.splitlines(keepends=True)
    pattern = re.compile(rb'^\s*\d+\.\d+\s+(\d+)\s+\d+\s+[VDIWEFAS]\s+')
    relevant = set()
    for line in lines:
        match = pattern.match(line)
        if PACKAGE.encode() in line and match:
            relevant.add(match[1])
    return b''.join(line for line in lines
                    if (match := pattern.match(line)) and match[1] in relevant)


def package(root, output):
    manifest = {'package': PACKAGE, 'expected_source': SOURCE, 'expected_apk_sha256': APK_SHA256,
                'clock': json.loads((root / 'Session.json').read_text()).get('clock'),
                'scope': 'Read-only observation of existing release; Logcat and user reports only. No private native log/save access.',
                'files': []}
    with zipfile.ZipFile(output, 'x', zipfile.ZIP_DEFLATED) as bundle:
        for path in sorted(root.iterdir()):
            if not path.is_file():
                continue
            raw = path.read_bytes()
            data = crash_scope(raw) if path.name == 'Crash-Buffer.txt' else raw
            scoped = data != raw
            excerpt = path.suffix == '.txt' and len(data) > SHARE_LIMIT
            if excerpt:
                half = SHARE_LIMIT // 2
                data = data[:half].rsplit(b'\n', 1)[0] + b'\n[SIZE-LIMITED EXCERPT; MIDDLE OMITTED]\n' + data[-half:].split(b'\n', 1)[-1]
            name = path.name if not excerpt else path.stem + '-excerpt' + path.suffix
            bundle.writestr(name, data)
            manifest['files'].append({'raw_name': path.name, 'included_name': name,
                                      'raw_bytes': len(raw), 'raw_sha256': hashlib.sha256(raw).hexdigest(),
                                      'included_bytes': len(data), 'included_sha256': hashlib.sha256(data).hexdigest(),
                                      'release_crash_scope_applied': scoped, 'size_excerpt': excerpt})
        bundle.writestr('Share-Manifest.json', json.dumps(manifest, indent=2) + '\n')


def observe(adb, serial, downloads, work):
    if not sys.stdin.isatty():
        raise RuntimeError('Run observe-candidate directly in an interactive Ubuntu terminal.')
    stamp = datetime.datetime.now().strftime('%Y%m%d-%H%M%S') + '-' + uuid.uuid4().hex[:8]
    root = Path(work) / 'candidate-captures' / stamp
    root.mkdir(parents=True, mode=0o700)
    downloads = Path(downloads)
    downloads.mkdir(parents=True, exist_ok=True)
    output = downloads / ('Tooie-' + SOURCE[:12] + '-Release-Check-' + stamp + '-Share.zip')
    capture, failure = Capture(adb, serial, root), None
    try:
        print('Checking the already installed release APK. Keep its launcher open.', flush=True)
        uid = capture.identity()
        capture.event('capture-start', 'Exact installed signed release verified; UID=' + uid)
        capture.stream('App-Logcat', ['logcat', '--uid=' + uid, '-b', 'all', '-v', 'epoch', '-T', '1'])
        capture.stream('Crash-Buffer', ['logcat', '-b', 'crash', '-v', 'epoch', '-T', '1'])
        capture.interact()
    except (OSError, RuntimeError, KeyboardInterrupt) as error:
        failure = str(error) or 'Interrupted from terminal.'
        capture.event('stopped', failure)
        print('STOP: ' + failure, flush=True)
    finally:
        capture.close()
        (root / 'Session.json').write_text(json.dumps({'package': PACKAGE, 'source_commit': SOURCE,
            'expected_apk_sha256': APK_SHA256, 'clock': capture.clock, 'failure': failure,
            'install_performed': False, 'launch_performed': False, 'force_stop_performed': False,
            'native_private_log_available': False, 'publication_approved': False}, indent=2) + '\n')
        package(root, output)
        print('\nUPLOAD THIS ONE FILE:\n' + str(output), flush=True)
        print('Full raw evidence stays on your PC. Matching crash records and any size excerpts are identified inside.', flush=True)
        print('Capture finished. You may close the app normally. Do not type done.', flush=True)
    return 1 if failure else 0
