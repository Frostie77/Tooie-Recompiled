# Reusable task-delivery collector. One install/launch in this session; repeat sessions have distinct evidence.
import datetime as dt
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import signal
import sys
import subprocess as sp
import tempfile
import time
import uuid
import zipfile

SHA = '3a8ef82eafc02a5318f4a16c6981f75628af1f7e'
HASH = 'f4e0ec9a30d2f1a57e3b19a052f6e284fec7c26910913ab87d2a622c20b6038a'
BUILD_ID = '7da65931e98aa0e45eda5c237c2ba9e8662d67d1'
DOWNLOADS = Path('/mnt/c/Users/Trent/Downloads')
APK = DOWNLOADS / 'Tooie-3a8ef82eafc0-Task-Delivery-Timing-Debug.apk'
REPORT = DOWNLOADS / 'Tooie-3a8ef82eafc0-Task-Delivery-Timing-Debug-build-report.txt'
ADB = '/mnt/c/Users/Trent/AppData/Local/Android/Sdk/platform-tools/adb.exe'
READELF = Path.home() / 'Android/Sdk/ndk/28.2.13676358/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-readelf'
SERIAL = '7907333f'
PKG = 'io.github.banjotooierecompiled'
MARKER = Path.home() / 'projects/tooie-tools/odin-runs' / (SHA + '-task-delivery-timing-' + HASH[:12] + '.started')
prefix = None
reader = None
reader_files = []
attempted = False
baseline = None
capture_start = None
OBSERVE_SECONDS = 20
DEMO_SECONDS = 300
DEMO_ONLY = False
GAMEPLAY_SECONDS = 600
EXTENSION_SECONDS = 300
MAX_UPLOAD_BYTES = 30 * 1024 * 1024
PART_RAW_BYTES = 20 * 1024 * 1024
CHUNK_BYTES = 8 * 1024 * 1024
original_pid = None
finish_requested = False
ENGINEERING = '9ec05e8667b9529d7b1096608a74c846c1bda4eb'
NATIVE_SOURCE = SHA
ARTWORK_HASH = '87b0fdf84f30e5bff5516e6acfc7fa812713fdae2f4a4fb22b68333b16b89f26'
RUNTIME_DOMAINS = ('audio', 'vi', 'present', 'config', 'workload-stages',
                   'render-stages', 'present-stages', 'vulkan-build', 'shader-build', 'interpolation', 'present-queue', 'matching-stages', 'guest-fatal', 'skeletal-source', 'skeletal-rejected', 'skeletal', 'actor-source', 'actor-draw', 'lookup-failure', 'lookup-registers', 'lookup-neighbors', 'lookup-words', 'lookup-map', 'lookup-section', 'lookup-guest-table', 'lookup-guest-overlay', 'lookup-event', 'guest-frame-sync', 'present-input', 'full-sync', 'gfx-submit', 'gfx-dispatch', 'display-list', 'correction-rom', 'correction-clock', 'correction-clock-event', 'correction-clock-bucket')
RUNTIME_RE = re.compile(r'^Runtime (?:' + '|'.join(map(re.escape, RUNTIME_DOMAINS)) + r'):')

def require(ok, message):
    if not ok:
        raise RuntimeError(message)

def save(name, value):
    path = Path(str(prefix) + '-' + name + '.txt')
    with path.open('xb') as f:
        f.write(value if isinstance(value, bytes) else value.encode())

def command(args, name=None, strict=True, timeout=30):
    try:
        r = sp.run([str(x) for x in args], stdin=sp.DEVNULL,
                   capture_output=True, timeout=timeout,
                   env={**os.environ, 'LC_ALL': 'C'})
    except sp.TimeoutExpired as e:
        r = sp.CompletedProcess(args, -999, e.stdout or b'',
                                (e.stderr or b'') + b'\nTIMED OUT: completion is unknown; no retry.\n')
    if name is not None:
        save(name, r.stdout)
        save(name + '-status', f'returncode={r.returncode}\n' + r.stderr.decode(errors='replace'))
    if strict:
        require(r.returncode == 0, f'Command failed ({name or args[0]}): ' +
                (r.stdout + r.stderr).decode(errors='replace'))
    return r

def adb(*args, **kwargs):
    return command([ADB, '-s', SERIAL, *args], **kwargs)

def text(r):
    return r.stdout.decode(errors='replace').replace('\r\n', '\n').strip()

def parse_pids(result):
    # pidof itself ends its nonempty output with a newline. Our status marker
    # adds another one. Accept blank separator lines, validate all other lines.
    require(result.returncode == 0 and not result.stderr.strip(), 'Cannot reliably check app processes.')
    lines = [line.strip() for line in text(result).splitlines() if line.strip()]
    require(len(lines) in (1, 2), 'Unexpected PID-check response.')
    marker = re.fullmatch(r'TOOIE_PIDOF_RC=([01])', lines[-1])
    require(marker is not None, 'Missing or invalid PID-check status marker.')
    payload = lines[0] if len(lines) == 2 else ''
    require(not payload or re.fullmatch(r'[0-9]+(?:[ \t]+[0-9]+)*', payload), 'Invalid app PID list.')
    values = tuple(int(value) for value in payload.split())
    require(all(value > 0 for value in values) and len(set(values)) == len(values), 'Invalid or duplicate app PID.')
    require(bool(values) == (marker[1] == '0'), 'Inconsistent pidof result.')
    return values

def pids(name=None):
    cmd = f'pidof {PKG}; result=$?; printf "\\nTOOIE_PIDOF_RC=%s\\n" "$result"'
    return parse_pids(adb('shell', '-T', cmd, name=name))

def sample_pids(name):
    # Post-launch observation is read-only. A sampling error must not truncate
    # the evidence window or trigger another launch.
    try:
        return pids(name)
    except Exception as error:
        save(name + '-error', str(error) + '\n')
        print('ADB process status is unconfirmed; the error details were saved.', flush=True)
        return None

def screenshot(label):
    # Host-only PNG, and only while the original app is foreground. No recording
    # files are created on Odin and no input/focus change is sent to the app.
    active = sample_pids(label + '-PIDs')
    if original_pid is None or active is None or original_pid not in active:
        save(label + '-Skipped', 'Original process unavailable; no screenshot taken.\n')
        return
    try:
        foreground = adb('shell', '-T', 'dumpsys activity activities', name=label + '-Foreground', strict=False, timeout=10)
        resumed = [line for line in text(foreground).splitlines()
                   if re.search(r'\b(?:topResumedActivity|mResumedActivity)\s*[:=]\s*ActivityRecord\{', line)]
        if foreground.returncode != 0 or not resumed or not all(re.search(r'\s' + re.escape(PKG) + r'/', line) for line in resumed):
            save(label + '-Skipped', 'Expected app foreground could not be confirmed; no screenshot taken.\n')
            return
        clock(label + '-Device-Time')
        result = command([ADB, '-s', SERIAL, 'exec-out', 'screencap', '-p'], strict=False, timeout=10)
        save(label + '-status', f'returncode={result.returncode}\n' + result.stderr.decode(errors='replace'))
        if result.returncode == 0 and result.stdout.startswith(b'\x89PNG\r\n\x1a\n') and result.stdout.endswith(bytes.fromhex('0000000049454e44ae426082')):
            with Path(str(prefix) + '-' + label + '.png').open('xb') as output:
                output.write(result.stdout)
        else:
            save(label + '-error', 'Screenshot was not a complete PNG response; no image claimed.\n')
    except Exception as error:
        save(label + '-error', str(error) + '\n')



# Live notes use the SAME terminal and the SAME evidence ZIP. A host reader
# timestamps complete input lines even while an ADB read is in progress.
import queue as _input_queue
import select as _input_select
import threading as _input_threads

class LiveInput:
    def __init__(self, stream, journal):
        self.fd = stream.fileno()
        self.journal = journal
        self.items = _input_queue.Queue()
        self.stop = _input_threads.Event()
        self.phase = 'readiness'
        self.error = None
        self.thread = _input_threads.Thread(target=self._read, name='tooie-live-notes')
        self.thread.start()

    def _publish(self, raw):
        item = {'host_read_wall': dt.datetime.now().astimezone().isoformat(timespec='milliseconds'),
                'host_read_mono_seconds': time.monotonic(), 'phase_at_read': self.phase,
                'text': raw.decode('utf-8', errors='replace').rstrip('\r')}
        with self.journal.open('ab') as stream:
            stream.write((json.dumps(item, ensure_ascii=True) + '\n').encode())
            stream.flush(); os.fsync(stream.fileno())
        self.items.put(item)

    def _read(self):
        pending = b''
        try:
            while not self.stop.is_set():
                ready, _, _ = _input_select.select([self.fd], [], [], 0.1)
                if not ready:
                    continue
                block = os.read(self.fd, 4096)
                if not block:
                    if pending: self._publish(pending)
                    self.items.put(None)
                    return
                pending += block
                while b'\n' in pending:
                    raw, pending = pending.split(b'\n', 1)
                    self._publish(raw)
        except Exception as error:
            self.error = str(error)
            self.items.put(None)

    def close(self):
        self.stop.set()
        self.thread.join(timeout=2)
        require(not self.thread.is_alive(), 'Host note reader did not finish.')

live_input = None
live_events = []
live_latest = {}

def input_hub():
    global live_input
    if live_input is None:
        require(prefix is not None, 'Capture identity must exist before accepting live notes.')
        live_input = LiveInput(sys.stdin, Path(str(prefix) + '-Live-Input-Journal.txt'))
    return live_input

def handle_live_note(item, phase):
    value = item['text'].strip()
    match = re.fullmatch(r'(?:(note)\s+)?(10|[1-9])(?:\s+(.*))?', value, re.I)
    if match is None:
        return False
    kind, number, description = ('note' if match[1] else 'event'), int(match[2]), match[3] or ''
    if len(description) > 240 or any(ord(c) < 32 for c in description):
        print('Keep marker descriptions to 240 characters on one line. Your raw input was retained.', flush=True)
        return True
    if kind == 'note' and (number not in live_latest or not description):
        print('Use note NUMBER description after recording that marker. Raw input retained.', flush=True)
        return True
    identifier = uuid.uuid4().hex
    record = {'id': identifier, 'sequence':len(live_events)+1, 'marker':number,
              'kind':kind, 'description':description, 'original_game_pid':original_pid,
              'host_read_wall':item['host_read_wall'],
              'host_read_mono_seconds':item['host_read_mono_seconds'],
              'phase_at_read':item['phase_at_read'], 'phase_at_handling':phase,
              'delivery':'pending',
              'timing_scope':'Host timestamp when the terminal line was read, not proof of bug onset. Device log timestamp is later if an ADB operation delayed handling.'}
    if kind == 'note':
        record['target_event_id'] = live_latest[number]['id']
    path = Path(str(prefix) + '-User-Markers.txt')
    def append(row):
        with path.open('ab') as stream:
            stream.write((json.dumps(row, ensure_ascii=True)+'\n').encode())
            stream.flush(); os.fsync(stream.fileno())
    append(record)
    live_events.append(record)
    if kind == 'event': live_latest[number] = record
    device = {key:record[key] for key in ('id','sequence','marker','kind','description','original_game_pid')}
    device['capture'] = prefix.name
    payload = json.dumps(device, ensure_ascii=True, separators=(',',':'))
    if len(payload.encode()) > 900:
        del device['description']; device['description_in_evidence'] = True
        payload = json.dumps(device, ensure_ascii=True, separators=(',',':'))
    require(len(payload.encode()) <= 900, 'Complete marker payload is too large.')
    # One bounded diagnostic message, never a game input or lifecycle action.
    try:
        result = adb('shell','-T','log -p i -t TooieUserMark '+shlex.quote(payload),
                     name='User-Marker-'+str(record['sequence']).zfill(3), strict=False, timeout=5)
        record['delivery'] = 'command-acknowledged' if result.returncode == 0 else 'unconfirmed'
    except Exception as error:
        # A diagnostic delivery failure must not end a gameplay capture. The
        # durable host event already contains the complete note and receipt time.
        record['delivery'] = 'unconfirmed'
        record['delivery_error'] = str(error)
    record['host_handled_mono_seconds'] = time.monotonic()
    append({'kind':'delivery','event_id':identifier,'metadata':record.copy()})
    print('Saved marker '+str(number)+' (entry '+str(record['sequence'])+'). Capture continues.', flush=True)
    return True

def wait_with_live_notes(seconds, phase):
    global finish_requested
    hub = input_hub(); hub.phase = phase
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        try:
            item = hub.items.get(timeout=max(0, deadline-time.monotonic()))
        except _input_queue.Empty:
            return
        if item is None:
            return  # Capture continues if terminal input ends; raw evidence stays open.
        if handle_live_note(item, phase):
            continue
        if item['text'].strip().casefold() in ('save', 'stop'):
            finish_requested = True
            save('User-Finish-Request', json.dumps(item, ensure_ascii=False, indent=2) + '\n')
            print('Finish request saved. Keeping the evidence; leave the app as it is.', flush=True)
            return
        print('During capture: type 1–10 and a short note, or save to finish. Other choices are available when the timer ends. Raw input retained.', flush=True)

def close_live_input():
    if live_input is None:
        return
    live_input.close()
    while True:
        try: item = live_input.items.get_nowait()
        except _input_queue.Empty: break
        if item is not None: handle_live_note(item, 'capture-ending')
    if live_input.error:
        save('Live-Input-Error', live_input.error+'\n')
    rows = ['User bug notes; original entry times retained. Notes may describe an earlier visible issue.']
    for item in live_events:
        target = ' target='+item.get('target_event_id','') if item['kind']=='note' else ''
        rows.append(str(item['sequence'])+' | '+item['host_read_wall']+' | '+item['kind']+' '+str(item['marker'])+target+' | '+(item['description'] or '[quick marker]'))
    save('User-Marker-Summary','\n'.join(rows)+'\n')


def observe(label):
    demo = label.startswith('Demo-')
    timed = demo or label.startswith('Gameplay-')
    duration = DEMO_SECONDS if demo else (GAMEPLAY_SECONDS if label == 'Gameplay-01' else EXTENSION_SECONDS) if timed else OBSERVE_SECONDS
    if demo:
        print('INTRO/DEMO CAPTURE: FIVE MINUTES. Leave Odin untouched so the intro and automatic demos can run. Do not skip scenes or load a save yet. In THIS Ubuntu window, mark visible speed changes, demo transitions or hitches with 1–10 and a short description.', flush=True)
    elif timed:
        print(f'GAMEPLAY CAPTURE: {duration // 60} MINUTES. Play normally from your existing save. Mark input delay, freezes, stretching or sound problems in THIS Ubuntu window. First-person entrance/boss success was already observed; another visit is optional.', flush=True)
    else:
        print(f'Collecting launcher evidence for {duration} seconds. Leave the app untouched; do not reopen it if it closes.', flush=True)
    if timed:
        print('Notes: 1 intro looks fast  [then Enter]. A number alone also works; reuse 1–10 for more markers. Use note 1 DESCRIPTION to add detail later. Type save at any time to finish. Keep graphics/volume settings unchanged. No camera or second window is needed.', flush=True)
    started = time.monotonic()
    started_wall = dt.datetime.now().astimezone().isoformat()
    deadline = started + duration
    index = shot = 0
    shot_times = tuple(range(60, duration, 120)) if timed else ()
    next_progress = 60
    reader_stopped = process_lost = completed = False
    try:
        while time.monotonic() < deadline and not (timed and finish_requested):
            active = sample_pids(f'{label}-PIDs-{index:03d}')
            index += 1
            if timed and active is not None and original_pid not in active:
                process_lost = True
                save(label + '-Original-Process-Unavailable', 'Original PID absent or changed. No relaunch or extension.\n')
                print('Original game process ended or changed. Leave the app closed; retaining crash evidence without relaunch.', flush=True)
                time.sleep(5)
                break
            if reader is not None and reader.poll() is not None and not reader_stopped:
                save(label + '-Reader-Stopped', 'Live Logcat ended early; observation and final dump continue.\n')
                reader_stopped = True
            elapsed = time.monotonic() - started
            if timed and shot < len(shot_times) and elapsed >= shot_times[shot]:
                screenshot(f'Screenshot-{label}-{shot + 1:02d}')
                shot += 1
            if timed and elapsed >= next_progress:
                remaining = max(0, int(deadline - time.monotonic()))
                print(f'{label}: {remaining // 60}:{remaining % 60:02d} remaining. Capture continues.', flush=True)
                next_progress = (int(elapsed) // 60 + 1) * 60
            remaining = deadline - time.monotonic()
            if remaining > 0:
                wait_with_live_notes(min(5, remaining), label) if timed else time.sleep(min(2, remaining))
        completed = not process_lost and not (timed and finish_requested) and time.monotonic() >= deadline
        return completed
    finally:
        if timed:
            ended = time.monotonic()
            save(label + '-Window', json.dumps({
                'stage': 'intro-automatic-demo' if demo else 'gameplay',
                'requested_seconds': duration, 'measured_observer_seconds': ended - started,
                'completed_requested_window': completed,
                'user_requested_finish': finish_requested,
                'original_process_unavailable': process_lost,
                'live_reader_ended': reader_stopped,
                'host_start': started_wall, 'host_end': dt.datetime.now().astimezone().isoformat(),
                'host_start_monotonic_seconds': started, 'host_end_monotonic_seconds': ended,
                'screenshot_attempts': shot,
                'scope': ('Untouched intro/automatic-demo observation after user declared Start Game pressed; observed state/map and game-input absence are user observations, not inferred from elapsed time.' if demo else 'Timed observation after user declared a playable area; elapsed time alone does not prove active play, correct speed or area coverage. Loading/picker/readiness waits are excluded.') + ' Raw logging continues outside this window. Includes timing observer/logging overhead.',
            }, indent=2) + '\n')


def clock(name):
    r = adb('shell', '-T', 'date "+%Y-%m-%d %H:%M:%S"', name=name)
    value = text(r)
    require(re.fullmatch(r'\d{4}-\d\d-\d\d \d\d:\d\d:\d\d', value), 'Unexpected device clock format.')
    return value


def prompt(message):
    require(sys.stdin.isatty(), 'Run directly in the interactive Ubuntu terminal.')
    hub = input_hub(); hub.phase = 'prompt: '+message
    print(message, end='', flush=True)
    while True:
        item = hub.items.get()
        require(item is not None, 'Interactive input ended; retaining evidence.')
        if handle_live_note(item, hub.phase):
            print(message, end='', flush=True)
            continue
        return item['text'].strip()

def health():
    require(reader is not None and reader.poll() is None, 'Logcat capture stopped unexpectedly; no further install/launch.')

def start_capture(start):
    global reader
    # This shell stops ONLY its own Logcat reader when sent STOP or EOF.
    remote = ('logcat -b all -v threadtime,year,zone,usec -T ' +
              shlex.quote(start + '.000') + ' "*:V" & reader=$!; '
              'trap \'kill "$reader" 2>/dev/null; wait "$reader" 2>/dev/null || :\' EXIT; '
              'trap \'exit 1\' HUP INT TERM; sleep 1; '
              'kill -0 "$reader" 2>/dev/null || exit 1; '
              'printf "TOOIE_CAPTURE_READY\\n" >&2; IFS= read -r stop')
    out = open(str(prefix) + '-Logcat.txt', 'xb', buffering=0)
    err = open(str(prefix) + '-Logcat-reader.txt', 'xb', buffering=0)
    reader_files.extend([out, err])
    reader = sp.Popen([ADB, '-s', SERIAL, 'shell', '-T', remote],
                      stdin=sp.PIPE, stdout=out, stderr=err)
    for _ in range(100):
        health()
        if b'TOOIE_CAPTURE_READY' in Path(err.name).read_bytes():
            return
        time.sleep(0.1)
    raise RuntimeError('Logcat readiness was not confirmed. No recovery launch.')

def stop_capture():
    if reader is not None and reader.poll() is None:
        try:
            reader.stdin.write(b'STOP\n')
            reader.stdin.flush()
            reader.stdin.close()
            reader.wait(timeout=10)
        except (OSError, ValueError, sp.TimeoutExpired):
            print('WARNING: Logcat reader cleanup did not finish normally.')
            reader.terminate()
            try:
                reader.wait(timeout=3)
            except sp.TimeoutExpired:
                reader.kill()
                try:
                    reader.wait(timeout=3)
                except sp.TimeoutExpired:
                    print('WARNING: host Logcat client may still be running.', flush=True)
    for file in reader_files:
        file.close()

def expected_report():
    return {'Source tree': 'ff0fec4c3652caac561a7e5b3b0a638aa2daec5b',
     'Engineering revision': '9ec05e8667b9529d7b1096608a74c846c1bda4eb',
     'Player implementation': 'b679975a352bf2f63b650f78845a2bf1eb5f8d9c',
     'Mod/branding tested source': '82a6641c228d71756222bc0da96b0f159e51abd4',
     'Approved icon/name source': 'e1fcf938dcc24bb9865b743f900d107a9de26d9d',
     'Approved title source': 'c9f5efc17df77be6a53e4f475a227092bb7e9c9f',
     'Integration': 'exact disjoint 98-file engineering and 61-file tested mod/branding/host delta '
                    'from ffd520a; no implementation edits',
     'App name': 'Tooie Recompiled (compiled application/activity labels verified)',
     'Branding': 'approved round BT/Jiggy icon, legacy/round/foreground visible RGBA and adaptive '
                 'references verified in final APK',
     'Menu asset SHA-256': '87b0fdf84f30e5bff5516e6acfc7fa812713fdae2f4a4fb22b68333b16b89f26 (both '
                           'source copies and exact APK member)',
     'Icon master SHA-256': '4b6db92f4ad3280c10131b5cec745508b19257a93ab1b2615279813ebca06784',
     'Build mode': 'INDEPENDENT TASK DELIVERY TIMING DIAGNOSTIC DEBUG',
     'Native CMake build type': 'Release',
     'Android packaging variant': 'Debug (existing isolated Gradle workflow; no release/key '
                                  'configuration changes)',
     'Descriptor allocation probe': 'OFF',
     'Geometry trace': 'OFF',
     'Correction geometry capture': 'OFF',
     'Guest-clock/frame-sync trace': 'ON (independent TOOIE_ANDROID_TIMING_TRACE=1 in all native C/C++ '
                                     'commands)',
     'Presentation-input/split-wait trace': 'ON (same timing profile)',
     'Full-sync producer trace': 'ON (same timing profile; prepare/advance/publish_reset, eligibility '
                                 'counts, separate lifetime/report-window entry gaps and copied task '
                                 'correlation)',
     'Task delivery/display-list trace': 'ON (same timing profile; '
                                         'gfx-submit/gfx-dispatch/display-list, immutable '
                                         'IDs/timestamps, nested TLS correlation)',
     'Task timing scope': 'submit age includes enqueue/observation overhead; interpreter includes '
                          'full-sync; entry gaps include previous work/logging; independent/nested '
                          'maxima are not additive',
     'Producer diagnostics scope': 'observer only; queued work, candidate ordering, publication '
                                   'predicates and ownership waits unchanged',
     'Timing behavior': 'diagnostic observation only; no speed multiplier, divisor, replay '
                        'consumption, graphics policy or synchronization correction',
     'Timing measurement scope': 'includes bounded observer/logging overhead; not the normal '
                                 'observer-OFF performance baseline',
     'Player hierarchy': 'ON by Android default; eleven actual C/C++ environments verify player/actor '
                         'switches and enabled macros equal 1',
     'Player hierarchy scope': 'validated animated player slots; owned pose/schema/occupant/frame '
                               'metadata; exact submitted endpoints; invalid recognized history uses '
                               'current pose; standard actor policy separately validated',
     'Player hierarchy validation': 'native hooks/task/RSP/workload/matcher/consumer fixture, GCC '
                                    'ASan/UBSan, independent Clang SIMD and exact-60c0c89 failing '
                                    'control passed; actual Odin appearance/overhead unverified',
     'Hierarchy host coverage': 'six committed player/constructed-actor triangle cases in scalar/SIMD '
                                'plus rigid actor reordering and native lifetime controls; external '
                                '192 cases were separate Codex validation, not captured NPC poses',
     'Actor hierarchy': 'ON by Android default; validated marker-constructor generation, canonical '
                        'movable actor-array ownership and owned authored pose',
     'Actor fallback': 'unsupported registered bones retain exact submitted matrices; '
                       'custom/sprite/particle paths unchanged',
     'Queue isolation': '0; transfer/fallback/barrier selectors UNDEFINED',
     'Transfer clear policy': 'production full transfer and scissored partial draw',
     'ARM64 API level': '24',
     'Mod packaged checks': 'ModImporter/openModPicker/nativeModFileDialogResult in DEX, actual JNI '
                            'export, archive validation/rollback and texture startup code in '
                            'libmain.so',
     'Lookup provenance': 'Android failure-only caller/line/raw registers, decoded syscall/thread '
                          'entry, native/guest mappings, bounded words and recent load/unload journal',
     'Lookup fatal handling': 'primary guest-fatal before detail records; strict failure/quick-exit '
                              'retained; no guessed/stubbed function or timing correction',
     'Full lookup stderr trace': 'not enabled by this builder; opt-in host regression control is '
                                 'separate',
     'First-person transition': 'corrections retained; entrance/boss success observed on prior b3401dd '
                                'hardware run; this timing-diagnostic APK remains untested on Odin',
     'First-person consumer': 'exact complete normalized function SHA-256 '
                              '997edfb876fe4a765e4dce7d7ede0bbbdcf9cbbe55bbb610c4bfec3f6fcb2412',
     'Four-consumer ROM excerpt SHA-256': '7bc7cd52c090d8da384833f342c9f9bfca6803515ebf72dbe0fd4d6f94612acd '
                                          '(23 slots and 2512 verified instruction bytes; no full ROM '
                                          'bundled)',
     'Step 115 fixture SHA-256': '1019de60047286e9a5c6723e25d945ff0df624b73c9f9e31bfd2f060f28698e5 '
                                 '(236987 committed LF bytes; no external archive required)',
     'Step 118 fixture SHA-256': '523ffe51c04051d383aa0891f8e0d2a2341378862e51b736128740cbc52bde49 '
                                 '(212256 committed LF bytes; external evidence-archive validation NOT '
                                 'RUN by this builder)',
     'Clock attribution limit': 'timer-input and attract ratios are separate observations; intended '
                                'demo cadence and an entire-game speed multiplier are not established',
     'First-person compile audit': 'funcs_44.c and funcs_45.c required in the normal ARM64 command '
                                   'database and effective Android contextual-lookup preprocessing',
     'Generator': 'host library/emitter built Release; analyzer and consumer adapters retain their '
                  'source suite sanitizer settings; no full ROM bundled; independent timing observer '
                  'enabled separately',
     'Clear modes': 'six comparison plus exact production; timeout control passed',
     'Exact production recorder': 'clipping, partial MSAA/view clear, full transfers and four '
                                  'rejection controls',
     'Descriptor recorder': '55 rejection cases; reservation/count/overlap and five-second safeguards '
                            'retained',
     'Software Vulkan, GPU tile lifecycle and pixel readbacks': 'NOT RUN by this builder (external '
                                                                'Codex results are separate)',
     'Hardware gameplay, Android provider behavior, code-mod execution and rendered texture replacement with this APK': 'NOT '
                                                                                                                        'YET '
                                                                                                                        'TESTED',
     'Outstanding NPC stretching, demo cadence, audio timeouts, sustained presentation, temple/jigsaw and full-game coverage': 'UNRESOLVED; '
                                                                                                                               'global '
                                                                                                                               'pacing '
                                                                                                                               'unchanged',
     'Automatic update checking': 'planned only; not implemented here',
     'Existing package/activity': 'io.github.banjotooierecompiled / .BanjoTooieSDLActivity (unchanged)',
     'Odin': 'NOT installed; NOT launched; no ADB commands used',
     'App data, settings and existing PC build folders/artifacts': 'preserved',
     'Source commit': '3a8ef82eafc02a5318f4a16c6981f75628af1f7e',
     'Source branch': 'build/android-task-delivery-mods-20261020-9ec05e8',
     'Source parent': '9ec05e8667b9529d7b1096608a74c846c1bda4eb '
                      'c9f5efc17df77be6a53e4f475a227092bb7e9c9f'} | {'APK':str(APK),'ELF Build ID':BUILD_ID,'APK SHA-256':HASH}



def require_original_process(name):
    active=sample_pids(name)
    if active is None:
        save(name+'-Assessment','state=unknown; ADB process check failed; app exit is not established.\n')
        raise RuntimeError('ADB could not confirm the app process. Its state is UNKNOWN; this does not confirm an app exit. Evidence is retained; no further game-start instruction or launch.')
    if original_pid not in active:
        save(name+'-Assessment','state=original-absent; verified PID list='+str(active)+'\n')
        raise RuntimeError('A successful process check did not find the original PID. No continuation or relaunch; this alone does not establish the reason it ended.')
    save(name+'-Assessment','state=original-present; verified PID list='+str(active)+'\n')
    return active


def main():
    global prefix, attempted, baseline, capture_start
    require(len(sys.argv) == 1, 'This collector takes no arguments.')
    require(sys.stdin.isatty(), 'Run this file directly in an interactive Ubuntu terminal; no piped input. No installation attempted.')
    require(not os.path.lexists(MARKER), 'This session already has a run marker. Start a new session through the workflow runner.')
    for file in (APK, REPORT, Path(ADB), READELF):
        require(file.is_file(), f'Missing file: {file}')
    with APK.open('rb') as f:
        digest = hashlib.sha256()
        for chunk in iter(lambda: f.read(1024 * 1024), b''):
            digest.update(chunk)
    require(digest.hexdigest() == HASH, 'APK SHA-256 mismatch.')
    report = REPORT.read_bytes()
    require(report.decode().splitlines()[0] == 'Tooie Recompiled Android verified task-delivery-timing build report', 'Unexpected build report header.')
    fields = {}
    for line in report.decode().splitlines():
        key, sep, value = line.partition(': ')
        if sep:
            require(key not in fields, f'Duplicate report field: {key}')
            fields[key] = value
    expected = expected_report()
    for key, value in expected.items():
        require(fields.get(key) == value, f'Unexpected report field: {key}')
    with zipfile.ZipFile(APK) as z:
        require(z.testzip() is None, 'APK ZIP integrity failure.')
        names = z.namelist()
        require(len(names) == len(set(names)), 'Duplicate APK ZIP entries.')
        for name in ('lib/arm64-v8a/libmain.so', 'lib/arm64-v8a/libSDL2.so', 'assets/program/assets/menu.png', 'AndroidManifest.xml', 'resources.arsc'):
            require(names.count(name) == 1, 'Missing or duplicate APK entry: ' + name)
        require(hashlib.sha256(z.read('assets/program/assets/menu.png')).hexdigest() == ARTWORK_HASH,
                'Packaged TOOIE artwork hash mismatch.')
        native = z.read('lib/arm64-v8a/libmain.so')
        sdl = z.read('lib/arm64-v8a/libSDL2.so')
        require(hashlib.sha256(sdl).hexdigest() == fields.get('Packaged SDL2 SHA-256'), 'Packaged SDL2 hash/report mismatch.')
        for marker in (b'Descriptor indexing discovery:', b'Descriptor layout support:',
                       b'Descriptor pool capacity:', b'Descriptor pool creation returned:',
                       b'Descriptor allocation returned:', b'Descriptor rejected: reason=',
                       b'framebuffer-shader-views', b'Descriptor pool device:',
                       b'Descriptor pool policy:', b'Descriptor pool retired:',
                       b'adreno-740-proprietary-declared-bound', b'reservedType=',
                       b'Descriptor allocation ABI:', b'Descriptor allocation arguments:',
                       b'descriptor-array-element-not-allocated',
                       b'descriptor-pool-update-after-bind-limit',
                       b'GPU tile buffer:', b'initialized-empty',
                       b'gpu-tile-buffer-not-prepared',
                       b'gpu-tile-buffer-range-too-small',
                       b'scissored-draw', b'Clear draw:',
                       b'Partial color clear rejected: reason=',
                       b'COLOR->TRANSFER_DST; clear mip=0 layer=0; TRANSFER_DST->COLOR; no clear render pass',
                       b'Runtime %s: mono_us=', b'event=open sdl_runtime=', b'sdl_revision=',
                       b'event=rate input_hz=', b'sdl_queue_us=', b'hardware_pending_excluded=1',
                       b'conversion-size-overflow', b'invalid-converted-frame-range', b'queue-failed',
                       b'event=effective-graphics-config values=', b'vi_iterations=',
                       b'vi_message_requests_total=', b'native_present_calls=', b'interpolation_frames='):
            require(marker in native, f'Missing native diagnostic: {marker!r}')
        for marker in (b'workload-stages', b'render-stages', b'present-stages', b'vulkan-build', b'shader-build',
                       b'stage_units=n/sum_us/max_us/end_mono_us', b'previous_log_us=',
                       b'player_mapped=', b'duplicate_player_ids=', b'invalid_decomposition_pairs=',
                       b'projection_mismatch_total=', b'gpu_ns='):
            require(marker in native, f'Missing rendering diagnostic: {marker!r}')
        for marker in (b'tooie_demo_pacing', b'present-queue', b'cursor_wait', b'select_write=', b'ready_write=',
                       b'decisions_total=', b'current_%s', b'decisions=%llu backlog=%llu',
                       b'matching-stages', b'logical_call_pairs=', b'transform_pairs=',
                       b'expansion_visits=', b'tile_lookups=', b'input_calls=',
                       b'scene_candidates', b'velocity_prepare'):
            require(marker in native, f'Missing progress/matching diagnostic: {marker!r}')

        for marker in (
            b'Android mod picker: opening document selection', b'openModPicker',
            b'Could not register texture-pack support.', b'rt64.json',
            b'The ZIP bundle contains no .nrm mods or .rtz texture packs.',
            b'targets a different game. Select a compatible Tooie mod.',
            b'This mod requires a native library that has not been validated for Android ARM64.',
            b'The archive contains an unsafe entry path.',
            b'An existing mod could not be restored. Its backup was retained in ',
            b'guest-fatal', b'action=terminate-without-static-destructors',
        ):
            require(marker in native, f'Missing mod/primary-failure component: {marker!r}')
        for marker in (
            b'identity=unknown edge_metric=endpoint-envelope-departure-not-error',
            b'correction-source', b'correction-data', b'correction-link', b'correction-sample',
            b'correction-consumer-cost', b'correction-budget', b'layout recursion: depth=',
        ):
            require(marker not in native, f'Geometry/correction observer or recursion control remains in timing APK: {marker!r}')

        for marker in (b'skeletal-source', b'skeletal-rejected', b'policy=authored-hierarchy',
                       b'npc_policy=validated-marker-hierarchy', b'action=current-submitted-pose', b'current_pose_fallback=',
                       b'evaluated_pairs=', b'rejected_owner=', b'changed_builder_output='):
            require(marker in native, f'Missing authored player-hierarchy component: {marker!r}')
        for marker in (b'actor-source', b'actor-draw', b'actor_applied=', b'player_applied=',
                       b'identity=marker-constructor-generation', b'array_stride=156',
                       b'marker=0x', b'generation=', b'callback=0x',
                       b'owned-hierarchy', b'unsupported-current-pose'):
            require(marker in native, f'Missing actor ownership/hierarchy component: {marker!r}')
        for marker in (b'lookup-failure', b'lookup-registers', b'lookup-neighbors', b'lookup-words', b'lookup-map', b'lookup-section', b'lookup-guest-table', b'lookup-guest-overlay', b'lookup-event', b'caller=%.160s source_line=%u', b'guest_ra_is_raw=1', b'thunk=0x%08X overlay=0x%08X entry_offset=0x%X auto_unload=%u', b'readable_words=%u', b'loaded_reported=%zu loaded_omitted=%zu', b'declared_exact=%u map_present=%u', b'entries_scanned=%u omitted=%u limit=64', b'computed_code=0x%llX code_in_rdram=%u native_table_index=%zu', b'sequence=%llu operation=%s', b'run_thread_function', b'recomp_syscall_handler'):
            require(marker in native, f'Missing failure-only lookup provenance: {marker!r}')
        for marker in (b'correction-rom',
         b'configured_usa_hash=00F70DE5F2D70EA2',
         b'correction-clock',
         b'correction-clock-event',
         b'correction-clock-bucket',
         b'guest-frame-sync',
         b'counters=guest_VI timing=observer_only',
         b'max_start_us=%llu max_end_us=%llu',
         b'before_pending=%u before_swap_ack=%u divisor=%u barrier=%u after_elapsed=%d after_pending=%u after_swap_'
         b'ack=%u',
         b'window_start_us=%llu window_end_us=%llu elapsed_us=%llu timers=%llu',
         b'replay_result=%d recorded_span=%u elapsed_span=%d applied_span=%d',
         b'caller=%u mode_bucket=%u events=%llu records=%llu',
         b'present-input',
         b'cursor_lock',
         b'cursor_condition',
         b'vi_flags_known=%u memory_flags_known=%u',
         b'entry_gap_max_us=%llu entry_gap_end_us=%llu publish_gap_max_us=%llu publish_gap_end_us=%llu',
         b'gaps=lifetime_max result_codes=repeat/early/unchanged/published'):
            require(marker in native, f'Missing independent timing diagnostic: {marker!r}')
        for marker in (
            b'full-sync', b'publish_reset',
            b'result=%u pairs=%u candidates=%u history=%u workload=%llu present=%llu',
            b'disabled=%llu empty=%llu no_candidate=%llu no_history_match=%llu published=%llu',
            b'gaps=lifetime_and_report_window result_codes=disabled/empty/no_candidate/no_history/published',
        ):
            require(marker in native, f'Missing full-sync producer diagnostic: {marker!r}')
        for marker in (
            b'entry_gap_window_max_us=%llu entry_gap_window_end_us=%llu',
            b'present=%llu task=%llu correlation_valid=%u',
            b'gfx-submit', b'gfx-dispatch', b'display-list', b'enqueue', b'submit_age',
            b'texture_actions', b'rsp_reset', b'ucode', b'interpret',
            b'task=%llu entry_us=%llu submit_us=%llu dl=0x%X bytes=%u',
            b'task=%llu submit_us=%llu dispatch_us=%llu dl=0x%X bytes=%u',
            b'task=%llu dl=0x%X bytes=%u correlation_valid=%u',
            b'calls=%llu invalid=%llu entry_gap_max_us=%llu entry_gap_end_us=%llu',
        ):
            require(marker in native, f'Missing task-delivery/window-gap diagnostic: {marker!r}')
        for marker in (b'Descriptor allocation probe start:', b'Descriptor allocation probe complete:',
                       b'production-path control begin', b'production-path control retired',
                       b'intentional diagnostic stop'):
            require(marker not in native, f'Early-exit probe remains in normal APK: {marker!r}')
        for marker in (b'load-op-control', b'load-op D barrier only',
                       b'Comparison only: explicit attachment-clear fallback enabled',
                       b'DIAG D: swapchain vkCmdClearColorImage recorded'):
            require(marker not in native, f'Diagnostic clear control remains in production APK: {marker!r}')
        dex = b''.join(z.read(n) for n in z.namelist() if re.fullmatch(r'classes\d*\.dex', n))
        for marker in (b'BanjoTooieSDLActivity', b'RomImporter', b'openRomPicker',
                       b'ModImporter', b'openModPicker', b'nativeModFileDialogResult',
                       b'mod-selection-', b'The mod archive exceeds the 1 GiB import limit.',
                       b'nativePresentationLifecycle', b'nativeFileDialogResult',
                       b'WindowSnapshotLog', b'Runtime window:', b'mono_uptime_ms=', b'insets_ltrb=', b'sequence='):
            require(marker in dex, f'Missing Java component: {marker!r}')
    with tempfile.TemporaryDirectory() as tmp:
        elf = Path(tmp) / 'libmain.so'
        elf.write_bytes(native)
        notes = text(command([READELF, '-n', elf]))
        require(re.search(r'Build ID:\s*' + BUILD_ID + r'\b', notes), 'ELF Build ID mismatch.')
        header = text(command([READELF, '-h', elf]))
        require(re.search(r'Class:\s+ELF64', header) and re.search(r'Machine:\s+AArch64', header), 'Unexpected ELF architecture.')
        symbols = text(command([READELF, '--dyn-syms', '--wide', elf]))
        for suffix in ('nativeFileDialogResult', 'nativeModFileDialogResult', 'nativePresentationLifecycle'):
            symbol = 'Java_io_github_banjotooierecompiled_BanjoTooieSDLActivity_' + suffix
            require(any(len(parts := line.split()) >= 8 and parts[-1] == symbol and
                        parts[-2] != 'UND' for line in symbols.splitlines()), f'Missing defined JNI export: {symbol}')
    print('PASS: exact Step 119 APK hash/report, task-delivery/display-list/full-sync window gaps and independent timing ON, geometry/correction capture OFF, hierarchy/mods/artwork retained, ZIP, ARM64 Build ID, DEX and all three JNI exports.', flush=True)
    apk_windows = text(command(['wslpath', '-w', APK]))
    require(text(adb('get-state')) == 'device', 'Expected Odin is unavailable.')
    require(text(adb('get-serialno')) == SERIAL, 'Device serial mismatch.')
    require(text(adb('shell', '-T', 'getprop ro.product.model')) == 'Odin2 Portal', 'Device model mismatch.')
    initial_pids = pids()
    print('PASS: Odin2 Portal / 7907333f. Existing app PIDs: ' + (', '.join(map(str, initial_pids)) or 'none'), flush=True)
    print('This test performs one replacement install and one launch of this exact APK. Updating ends any running app session. App data is retained. No force-stop, uninstall, data reset or settings change is performed.', flush=True)
    print('Replay wait correction is enabled. Timing diagnostics include observer/logging cost. This profile observes only the intro and automatic demos.' if DEMO_ONLY else 'Replay wait correction and timing diagnostics are enabled; measurements include observer/logging cost. Optional gameplay is available after demo capture.', flush=True)
    print(('INTRO/DEMO ONLY. Follow ready and watching in this ONE Ubuntu window; leave Odin untouched after Start Game. Type 1–10 plus a note during capture, or save to finish. Notes go in the SAME evidence ZIP. Do not type done.' if DEMO_ONLY else 'ONE UBUNTU WINDOW. Follow ready, watching and playing. Type 1–10 plus a note during capture, or save to finish. Notes go in the SAME evidence ZIP. Do not type done.'), flush=True)
    # Running this supplied collector requests the described one-install/one-launch test.
    stamp = dt.datetime.now().strftime('%Y%m%d-%H%M%S')
    prefix = DOWNLOADS / f'Tooie-3a8ef82eafc0-Task-Delivery-Timing-Odin-{stamp}-{uuid.uuid4().hex[:8]}'
    save('Identity', f'commit={SHA}\nengineering_ancestor={ENGINEERING}\nnative_source_commit={NATIVE_SOURCE}\napp_name=Tooie Recompiled\nicon_master_sha256=4b6db92f4ad3280c10131b5cec745508b19257a93ab1b2615279813ebca06784\nartwork_sha256={ARTWORK_HASH}\nhash={HASH}\nbuild_id={BUILD_ID}\nserial={SERIAL}\nhost_time={dt.datetime.now().astimezone().isoformat()}\n')
    save('Build-Report', report)
    save('Authorization', ('User invoked the demo-only collector: one replacement install and one launch, responsive launcher check, one manual Start Game, then untouched 300-second intro/demo windows with optional extensions. No save loading or gameplay is requested. App data/settings retained; no reset, force-stop or automatic input.\n' if DEMO_ONLY else 'User invoked the supplied one-window collector: one exact-APK replacement install and one launch, responsive-launcher check, optional one manual Start Game, 300-second untouched intro/automatic-demo observation with optional same-process extensions, then optional 600-second gameplay and 300-second extensions. Task-delivery/display-list, report-window producer gaps and independent timing diagnostics are enabled; rendering/pacing policies are unchanged. Existing app session ends during update; app data retained. No mod/ROM selection, automatic game input, forced stop, uninstall, reset, setting change or retry.\n'))
    save('Preflight-PIDs', ' '.join(map(str, initial_pids)) + '\n')
    capture_start = clock('Capture-Start-Device-Time')
    start_capture(capture_start)
    pids('Before-Install-PIDs')  # Record existing processes; no separate force-stop action.
    MARKER.parent.mkdir(parents=True, exist_ok=True)
    MARKER.mkdir()
    (MARKER / 'evidence-prefix.txt').write_text(str(prefix) + '\n')
    attempted = True
    print('Installing ONCE...', flush=True)
    clock('Install-Device-Time')
    r = adb('install', '-r', apk_windows, name='Install', strict=False, timeout=180)
    print(text(r), flush=True)
    require(r.returncode == 0 and 'Success' in text(r).splitlines(), 'Install success not confirmed. NO retry or launch.')
    health()
    require(not pids('After-Install-PIDs'), 'App is already running after installation. NO launch.')
    installed_paths = text(adb('shell', '-T', 'pm path ' + PKG, name='Installed-APK-Path')).splitlines()
    require(len(installed_paths) == 1 and installed_paths[0].startswith('package:/data/app/') and installed_paths[0].endswith('/base.apk'), 'Unexpected installed APK path; no launch.')
    installed = installed_paths[0][len('package:'):]
    installed_hash = text(adb('shell', '-T', 'sha256sum ' + shlex.quote(installed), name='Installed-APK-SHA256'))
    require(re.match(r'^[0-9a-fA-F]{64}(?:\s|$)', installed_hash) and installed_hash.split()[0].lower() == HASH, 'Installed APK does not match the exact verified file; no launch.')
    r = adb('shell', '-T', f'run-as {PKG} cat files/data/banjotooi-debug.log', name='Baseline', strict=False)
    baseline = r.stdout if r.returncode == 0 else None
    health()
    require(not pids('Before-Launch-PIDs'), 'App started before the single launch. NO launch.')
    clock('Launch-Device-Time')
    launch_log_offset = Path(str(prefix) + '-Logcat.txt').stat().st_size
    save('Launch-Logcat-Offset', str(launch_log_offset) + '\n')
    save('Launch-Attempted', 'Exactly one am start will be attempted. No retry.\n')
    print('Launching ONCE...', flush=True)
    r = adb('shell', '-T', f'am start -n {PKG}/.BanjoTooieSDLActivity', name='Launch', strict=False)
    print(text(r), flush=True)
    confirmed = r.returncode == 0 and not re.search(r'Error:|Warning:|Exception', text(r) + r.stderr.decode(errors='replace'), re.I)
    save('Launch-Assessment', 'Launch command unambiguous.\n' if confirmed else 'Launch completion failed or was ambiguous. No retry; evidence collection continues.\n')
    if not confirmed:
        print('Launch response was ambiguous or failed. No retry; retaining the observation window.', flush=True)
    observe('Launcher')
    require(confirmed, 'Launch failed or was ambiguous. Evidence retained; no retry.')
    maybe_start_game(launch_log_offset)


def original_launch_pid(offset):
    raw = Path(str(prefix) + '-Logcat.txt').read_bytes()[offset:].decode(errors='replace')
    match = re.search(r'Start proc (\d+):' + re.escape(PKG) + r'[/\s]', raw)
    require(match is not None, 'Original launch PID not identified in Logcat. No Start Game attempt.')
    return int(match[1])


def maybe_start_game(offset):
    global original_pid
    original_pid = original_launch_pid(offset)
    save('Original-Launch-PID', str(original_pid) + '\n')
    require_original_process('After-Launcher-PIDs')
    screenshot('Screenshot-Launcher')

    print('Launcher capture finished. Check launcher response and enabled Start Game. NOW type ready here in Ubuntu. Do not press Start Game yet. Type stop if the launcher is unavailable. No mod-picker or ROM-import test is required.', flush=True)
    answers = []
    while True:
        answer = prompt('Launcher assessment (ready / stop): ')
        answers.append(answer)
        choice = answer.casefold()
        if choice in ('ready', 'stop'):
            break
        print('Type ready or stop here in Ubuntu after checking the launcher.', flush=True)
    save('User-Launcher-Assessment', '\n'.join(answers) + '\n')
    if choice == 'stop':
        save('Start-Game-Skipped', 'User selected stop after launcher check; no manual Start Game invitation.\n')
        return
    require_original_process('Before-Start-Game-PIDs')
    health()
    clock('Start-Game-Instructions-Device-Time')
    save('Start-Game-Instructions', 'One manual Start Game attempt from the original responsive launcher. No new launch, ROM import or retry.\n')
    save('Gameplay-Capture-Plan', ('Intro/demo-only profile: five-minute untouched windows and optional extensions; no save loading or gameplay.\n' if DEMO_ONLY else '300-second untouched intro/demo windows after watching confirmation; extend until automatic demos are seen. Optional gameplay starts 600 seconds after playing confirmation, then optional 300-second extensions in the SAME process. Prompt/loading waits excluded from timed windows; raw logging remains continuous. Foreground-only host screenshots at approximately 60/180 seconds in five-minute windows and 60/180/300/420/540 seconds in the initial gameplay window. No automatic game input or audio/video recording.\n'))
    print('STEP 1: Press Start Game ONCE on Odin now. Leave the intro and automatic demos running untouched; do not press through them or load your save.', flush=True)
    print('STEP 2: Immediately after pressing Start Game, type watching HERE in Ubuntu and press Enter. Logging is already running. Type stop if the game is unavailable.', flush=True)
    answers = []
    while True:
        answer = prompt('Intro/demo readiness (watching / stop): ')
        answers.append(answer)
        choice = answer.casefold()
        if choice in ('watching', 'stop'):
            break
        print('Type watching after pressing Start Game once, or stop to retain evidence.', flush=True)
    save('User-Demo-Readiness', '\n'.join(answers) + '\n')
    if choice == 'stop':
        save('Timed-Demo-Skipped', 'User selected stop before the intro/demo timed window.\n')
        return
    segment = 1
    while True:
        label = f'Demo-{segment:02d}'
        require_original_process(label + '-Before-Window-PIDs')
        health()
        clock(label + '-Start-Device-Time')
        completed = observe(label)
        clock(label + '-End-Device-Time')
        if not completed:
            if finish_requested:
                save('User-Demo-Observation', prompt('Briefly describe the intro/demo speed, scenes or closure (or press Enter): ') + '\n')
            return
        print(('Demo window finished. Keep Odin untouched. Type continue for FIVE MORE MINUTES or save to finish.' if DEMO_ONLY else 'Demo window finished. Type continue for five more minutes, gameplay to load your save, or save to finish.'), flush=True)
        answers = []
        while True:
            answer = prompt('Demo decision (continue / save): ' if DEMO_ONLY else 'Demo decision (continue / gameplay / save): ')
            answers.append(answer)
            decision = answer.casefold()
            if decision in (('continue', 'save') if DEMO_ONLY else ('continue', 'gameplay', 'save')):
                break
            print('Type continue or save here in Ubuntu.' if DEMO_ONLY else 'Type continue, gameplay or save here in Ubuntu.', flush=True)
        save(label + '-Capture-Decision', '\n'.join(answers) + '\n')
        if decision != 'continue':
            break
        segment += 1
    save('User-Demo-Observation', prompt('Briefly describe which intro/demo scenes ran, visible speed and any Odin buttons pressed (or press Enter): ') + '\n')
    if DEMO_ONLY or decision == 'save':
        return
    print('NOW use the running game to load your existing save. Once you can move Banjo, type playing HERE in Ubuntu. Type stop if unavailable. The TEN-MINUTE gameplay timer starts only after playing; logging is already running. Do not reopen or relaunch the app.', flush=True)
    answers = []
    while True:
        answer = prompt('Gameplay readiness (playing / stop): ')
        answers.append(answer)
        choice = answer.casefold()
        if choice in ('playing', 'stop'):
            break
        print('Type playing once you can move Banjo, or stop to save the evidence.', flush=True)
    save('User-Gameplay-Readiness', '\n'.join(answers) + '\n')
    if choice == 'stop':
        save('Timed-Gameplay-Skipped', 'User selected stop before the timed playable-area window.\n')
        return
    clock('Gameplay-Ready-Device-Time')
    segment = 1
    while True:
        label = f'Gameplay-{segment:02d}'
        require_original_process(label + '-Before-Window-PIDs')
        health()
        clock(label + '-Start-Device-Time')
        completed = observe(label)
        clock(label + '-End-Device-Time')
        if not completed:
            break
        print('Timed window finished. Type continue for FIVE MORE MINUTES in this same session, or save to finish. Logging stays open while you decide.', flush=True)
        answers = []
        while True:
            answer = prompt('Capture decision (continue / save): ')
            answers.append(answer)
            decision = answer.casefold()
            if decision in ('continue', 'save'):
                break
            print('Type continue for five more minutes, or save to finish.', flush=True)
        save(label + '-Capture-Decision', '\n'.join(answers) + '\n')
        if decision == 'save':
            break
        segment += 1
    clock('After-Gameplay-Window-Device-Time')
    print('Timed capture finished. Leave the app as it is while evidence is saved.', flush=True)
    save('User-Gameplay-Observation', prompt('Briefly describe gameplay areas/actions, input delay, freezes, stretching and sound, or any closure (or press Enter): ') + '\n')

def collect():
    delta = None
    if attempted:
        reads = [
            ('Final-PIDs', ('shell', '-T', f'pidof {PKG}')),
            ('End-Device-Time', ('shell', '-T', 'date')),
            ('FullLog', ('shell', '-T', f'run-as {PKG} cat files/data/banjotooi-debug.log')),
            ('Display-State', ('shell', '-T', 'dumpsys display')),
            ('Window-State', ('shell', '-T', 'dumpsys window displays')),
            ('Audio-State', ('shell', '-T', 'dumpsys audio')),
            ('Audio-Flinger', ('shell', '-T', 'dumpsys media.audio_flinger')),
        ]
        if capture_start is not None:
            reads.append(('Logcat-Dump', ('logcat', '-b', 'all', '-d', '-v', 'threadtime,year,zone,usec', '-T', capture_start + '.000')))
        for name, args in reads:
            print('Saving evidence: '+name+' (bounded read; do not restart the app).',flush=True)
            try:
                result = adb(*args, name=name, strict=False)
                if name == 'FullLog':
                    if result.returncode == 0 and baseline is not None and result.stdout.startswith(baseline):
                        delta = result.stdout[len(baseline):]
                        save('NewLog', delta)
                        note = 'NewLog is the exact appended byte delta; use PID/TID tags and Logcat to correlate processes.\n'
                    else:
                        note = 'Reliable byte delta unavailable: read failed, baseline unavailable or file changed. FullLog retained; no NewLog manufactured.\n'
                    save('Delta-status', note)
                    print(note.strip(), flush=True)
            except Exception as error:
                save(name + '-read-error', str(error) + '\n')
                print(f'Evidence read failed ({name}); error retained.', flush=True)
    if reader is not None and reader.poll() is not None:
        print('Logcat stream ended early; inspect reader status and the separate final dump.', flush=True)
    stop_capture()
    if prefix is None:
        return
    lines = []
    for suffix in ('Logcat', 'Logcat-Dump'):
        path = Path(str(prefix) + '-' + suffix + '.txt')
        if path.exists():
            lines.extend(path.read_text(errors='replace').splitlines())
    # Preserve both raw files. De-duplicate only the derived combined summary.
    lines = list(dict.fromkeys(lines))
    stamped = [line for line in lines if re.match(r'\d{4}-\d\d-\d\d ', line)]
    stamped.sort()
    lines = stamped + [line for line in lines if not re.match(r'\d{4}-\d\d-\d\d ', line)]
    ids = set(re.findall(r'Start proc (\d+):' + re.escape(PKG), '\n'.join(lines)))
    if delta is not None:
        ids.update(re.findall(r'\bpid=(\d+)\b', delta.decode(errors='replace')))
    history = [line for line in lines if re.search(r'Start proc|has died|am_proc_start|am_proc_died|Process .*exited|Killing', line) and
               (PKG in line or any(re.search(r'\b' + pid + r'\b', line) for pid in ids))]
    focused = [line for line in lines if re.search(re.escape(PKG) + r'|Runtime (?:audio|vi|present|config|window|workload-stages|render-stages|present-stages|vulkan-build|shader-build|interpolation|present-queue|matching-stages|guest-fatal|lookup-failure|lookup-registers|lookup-neighbors|lookup-words|lookup-map|lookup-section|lookup-guest-table|lookup-guest-overlay|lookup-event|skeletal-source|skeletal-rejected|skeletal|actor-source|actor-draw|guest-frame-sync|present-input|full-sync|gfx-submit|gfx-dispatch|display-list|correction-rom|correction-clock|correction-clock-event|correction-clock-bucket):|TooieUserMark|Android mod picker|ModImporter|Document|shutdown|Rml|first.person|AudioTrack|OpenSLES|IBufferQueue|Descriptor|GPU tile buffer|gpu-tile-buffer|Clear record:|Clear draw:|Partial color clear|Transfer clear rejected|framebuffer-shader-views|Fatal signal|FATAL EXCEPTION|SIGSEGV|tombstone|BuildId|AndroidRuntime', line, re.I)]
    diagnostics = [] if delta is None else [line for line in delta.decode(errors='replace').splitlines()
        if re.search(r'Descriptor pool|Descriptor indexing discovery|Descriptor layout|Descriptor allocation|Descriptor set allocated|Descriptor.*rejected|GPU tile buffer|gpu-tile-buffer|Clear record:|Clear draw:|Partial color clear|Transfer clear rejected|framebuffer-shader-views', line)]
    ui_crash = [line for line in lines if re.search(r'Fatal signal|FATAL EXCEPTION|SIGSEGV|tombstone|\bDEBUG\s*:|guest-fatal|Runtime lookup-[a-z-]+:|AndroidRuntime|Rml|layout|document|shutdown|first.person|mod picker|ModImporter|openModPicker|nativeModFileDialogResult', line, re.I)]
    save('Crash-UI-Mod-Context', '\n'.join(ui_crash) + '\n')
    save('Process-History', '\n'.join(history) + '\n')
    save('Focused-Logcat', '\n'.join(focused) + '\n')
    save('Allocation-Diagnostics', '\n'.join(diagnostics) + '\n')
    gpu_tiles = [line for line in diagnostics if re.search(r'GPU tile buffer|gpu-tile-buffer', line)]
    save('GPU-Tile-Diagnostics', '\n'.join(gpu_tiles) + '\n')
    clears = [line for line in diagnostics if re.search(r'Clear record:|Clear draw:|Partial color clear|Transfer clear rejected', line)]
    save('Clear-Diagnostics', '\n'.join(clears) + '\n')
    summary = ('Recorded process history (each PID is a separate process):\n' + ('\n'.join(history) or '(none captured)') +
               '\n\nFresh private-log descriptor, GPU-tile and clear records (correlate processes using raw logs):\n' + ('\n'.join(diagnostics) or '(no reliable delta / no matching records)') +
               '\n\nRecent focused Logcat:\n' + ('\n'.join(focused[-60:]) or '(none captured)') + '\n')
    runtime = [] if delta is None else [line for line in delta.decode(errors='replace').splitlines()
        if RUNTIME_RE.match(line)]
    correlated = [line for line in runtime if original_pid is not None and re.search(r'\bpid=' + str(original_pid) + r'\b', line)]
    save('Runtime-All-Appended', '\n'.join(runtime) + '\n')
    for domain in RUNTIME_DOMAINS:
        selected = [line for line in correlated if line.startswith('Runtime ' + domain + ':')]
        save('Runtime-' + domain, '\n'.join(selected) + '\n')
    render = [line for line in correlated if re.match(r'Runtime (?:workload-stages|render-stages|present-stages|vulkan-build|shader-build|present-queue|matching-stages|full-sync|gfx-submit|gfx-dispatch|display-list):', line)]
    skeletal = [line for line in correlated if re.match(r'Runtime skeletal(?:-source|-rejected)?:', line)]
    save('Skeletal-Diagnostics', '\n'.join(skeletal) + '\n')
    actors = [line for line in correlated if re.match(r'Runtime actor-(?:source|draw):', line)]
    save('Actor-Diagnostics', '\n'.join(actors) + '\n')
    save('Actor-Capture-Scope', 'Fresh original-PID standard-actor ownership/admission and draw records. Bounded counts are operations, not unique actors or proof of corrected appearance. User markers identify receipt times and descriptions, not automatic model IDs or exact bug onset.\n')

    lookup = [line for line in correlated if re.match(r'Runtime (?:guest-fatal|lookup-[a-z-]+):', line)]
    save('Guest-Lookup-Diagnostics', '\n'.join(lookup) + '\n')
    save('Lookup-Capture-Scope', 'Only exact appended original-PID records are extracted here, preserving primary guest-fatal and lookup record order. Failure-only lookup records may be absent if no lookup failed or reliable fresh evidence is unavailable. Counts alone do not prove successful first-person gameplay. Raw caller/source line are native generated C provenance; raw guest RA is not a reliable guest callsite/backtrace. Native/guest tables and nearest entries are bounded observations, not an inferred owner or repair. Reported omission/readable counts must be retained. FullLog/NewLog and Logcat remain complete, including other processes and events outside the timed window.\n')


    save('Skeletal-Capture-Scope', 'Only fresh, original-launch-PID skeletal/source/rejected records are extracted. Their bounded counters describe policy activity and current-pose fallback, not a verified visual fix, persistent actor identity or complete coverage. Zero records do not prove that no eligible skeleton was seen. Coverage includes raw prompt/launcher/gameplay time; exact timed-window boundaries are saved separately.\n')
    save('Render-Timings', '\n'.join(render) + '\n')
    timing = [line for line in correlated if re.match(r'Runtime (?:guest-frame-sync|present-input|present-queue|full-sync|gfx-submit|gfx-dispatch|display-list|correction-rom|correction-clock|correction-clock-event|correction-clock-bucket):', line)]
    save('Timing-Diagnostics', '\n'.join(timing) + '\n')
    delivery = [line for line in correlated if re.match(r'Runtime (?:gfx-submit|gfx-dispatch|display-list|full-sync):', line)]
    save('Task-Delivery-Diagnostics', '\n'.join(delivery) + '\n')
    device_marks = []
    for line in lines:
        match = re.search(r'\bTooieUserMark\s*:\s*(\{.*\})', line)
        if match is None:
            continue
        try:
            record = json.loads(match[1])
        except (ValueError, TypeError):
            continue
        if record.get('capture') == prefix.name and record.get('original_game_pid') == original_pid:
            device_marks.append(line)
    save('User-Marker-Logcat', '\n'.join(device_marks) + '\n')

    save('Timing-Capture-Scope', 'Exact appended original-PID lines only. Native mono_us and window bounds are microseconds; guest values retain explicit VI/counts/guest_seconds units. Sparse clock-event details are bounded samples, not a complete event stream; aggregate buckets describe their windows. guest-frame-sync max_start_us/max_end_us and before/after snapshots belong to that maximum-duration call, not the last call; its full elapsed span includes swap acknowledgement as well as divisor/retrace work. full-sync reports preparation, advancement and publication/reset CPU durations, cumulative eligibility counters, separate lifetime and reporting-window entry-gap maxima, and copied task correlation. Window gaps reset only after a report and are assigned to the window observing the ending entry; they include the preceding call/logging and may start before the window. gfx-submit covers existing task/skeletal preparation and enqueue. Its packet timestamp precedes enqueue. gfx-dispatch submit_age includes enqueue and observer setup, not pure queue sleep or exclusive dispatch-body time. Prefix/send_dl/completion durations remain separate. display-list interpretation includes nested full-sync work. Task IDs are correlation keys, not an ownership/synchronization or global multi-producer order guarantee. Zero/invalid correlation can occur for dummy/non-task calls. No observed gap alone identifies its cause. present-input describes CPU State::updateScreen decisions/publication, not GPU completion or display scanout; result counters are cumulative. present-input entry/publication gap maxima remain lifetime maxima with their own end timestamps; the new full-sync and task fields label lifetime and reporting-window maxima separately. cursor_wait includes mutex/condition work; cursor_lock and cursor_condition are nested parts, and condition time includes mutex reacquisition. Maxima and peak contexts can belong to different events; do not add nested/independent maxima or attribute them directly to a GPU fence. ROM header/configured hash is not a full ROM dump/hash verification. User markers timestamp host receipt, not exact visual onset. No calibrated original/working-PC landmark baseline is supplied; these records alone do not establish an intended speed multiplier. Timing observation includes copying/logging overhead; geometry/correction-source capture remains OFF.\n')
    window = [line for line in lines if 'Runtime window:' in line and original_pid is not None and re.search(r'\bpid=' + str(original_pid) + r'\b', line)]
    save('Runtime-window', '\n'.join(window) + '\n')
    counts = {domain: sum(line.startswith('Runtime ' + domain + ':') for line in correlated)
              for domain in RUNTIME_DOMAINS}
    scope = ('Runtime summaries contain exact appended private-log records from the original launch PID only. Raw FullLog/NewLog/Logcat preserve all captured processes. '
             'No reliable delta means no native runtime summary is manufactured. Native mono_us and Java mono_uptime_ms have different explicit units. '
             'VI iterations/message requests, native presents, display Hz, SDL queue zero polls and AudioTrack stop messages are distinct observations. '
             'Stage fields are n/sum_us/max_us/end_mono_us; maxima may be different events and nested timings are not additive. gpu_ns is a GPU duration, not a CPU timestamp. previous_log_us records prior diagnostic emission cost. '
             'present-queue current totals describe all outer decisions at report time; peak contexts and stage maxima may be different decisions. Matching velocity preparation is nested in other phases. '
             'logical_call_pairs counts the old implied call product; expansion_visits, tile_lookups and input_calls count separate work dimensions. call_pairs remains the candidate-construction duration stage. '
             'Peak matching context ends before stage_units=; phase maxima can belong to different events. '
             'Initial untouched intro/automatic-demo windows are 300 seconds after watching confirmation, optionally extended in the same process. Optional gameplay is 600 seconds after user declares Banjo movable, followed by optional 300-second windows. A user save/stop during timed capture ends observation and is explicitly recorded. Exact per-window metadata records completion or interruption. Loading/prompt waits and final evidence reads are additional raw-log coverage. Task-delivery/display-list, separate lifetime/report-window full-sync gaps and independent guest-clock/frame-sync/input/split-wait timing is ON, including bounded observer/logging overhead; geometry/correction-source observers are OFF. Clock records keep their historical correction-clock names despite geometry capture being disabled. Timing-Capture-Scope defines attribution/units and limits. Authored player and validated standard-actor hierarchy are ON. Counters describe application/fallback, not appearance. Accepted prerecorded spans extend the existing guest VI wait; global VI rate and ordinary title/gameplay waits are unchanged. Failure-only lookup diagnostics are included. All five corrected USA first-person switches are present (47 retained plus 23 further slots). Entrance/boss success was reported for prior b3401dd hardware; this timing build requires its own observations. This replay-wait correction requires Odin verification; ordinary-title motion and presentation stalls remain separate unresolved issues. Actual mod execution/texture replacement are not tested here. '
             'No audio/video content was recorded. Screenshots alone cannot establish motion, sound quality or simulation speed.\n')
    if DEMO_ONLY:
        scope = 'INTRO/DEMO ONLY: no save loading or playable-area test is requested. ' + scope
    save('Capture-Scope', scope)
    summary += '\nRuntime records correlated with original PID ' + str(original_pid) + ': ' + str(counts) + '; window=' + str(len(window)) + '\n' + scope
    if correlated:
        summary += '\nRecent original-process runtime records:\n' + '\n'.join(correlated[-24:]) + '\n'
    save('Evidence', summary)
    print('Evidence saved for original PID ' + str(original_pid) + '; runtime counts: ' + str(counts), flush=True)
    bundles, index = archive_evidence()
    print('CAPTURE COMPLETE. Demo-cadence APK; replay-wait correction requires hardware verification; no intentional probe exit. Observer cost is included. Process presence, frame counts and screenshots alone do not establish correct speed or gameplay.', flush=True)
    print('Saved ' + ('raw evidence ZIP:' if len(bundles) == 1 else 'independent evidence ZIP parts AND the upload index:') + '\n' + '\n'.join(str(p) for p in bundles), flush=True)
    if index is not None:
        print(index, flush=True)
    print('Capture is saved. You may close the app normally. For another separately recorded session, use the workflow test command again.', flush=True)

def archive_evidence():
    # Retain every raw file. If the complete ZIP exceeds the per-upload limit,
    # also emit independent, bounded ZIPs with byte offsets and hashes. No log
    # truncation, device files, or reassembly-dependent split-ZIP format is used.
    sources = [p for p in sorted(DOWNLOADS.glob(prefix.name + '-*'))
               if p.is_file() and p.suffix in ('.txt', '.png')]
    bundle = Path(str(prefix) + '-Evidence.zip')
    with zipfile.ZipFile(bundle, 'x', zipfile.ZIP_DEFLATED) as archive:
        for path in sources:
            archive.write(path, path.name)
    if bundle.stat().st_size <= MAX_UPLOAD_BYTES:
        return [bundle], None
    groups = [[]]
    group_bytes = 0
    manifest = {'complete_local_zip': bundle.name, 'upload_limit_bytes': MAX_UPLOAD_BYTES,
                'scope': 'Upload every listed independent ZIP plus this index. Chunk entries are exact raw byte ranges, with zero-based offsets. Original files and full ZIP remain locally intact.',
                'sources': [], 'parts': []}
    for path in sources:
        size = path.stat().st_size
        source = {'name': path.name, 'size_bytes': size, 'chunks': []}
        digest = hashlib.sha256()
        with path.open('rb') as stream:
            offset = chunk_number = 0
            while True:
                data = stream.read(CHUNK_BYTES)
                if not data and (size or chunk_number):
                    break
                digest.update(data)
                if groups[-1] and group_bytes + len(data) > PART_RAW_BYTES:
                    groups.append([])
                    group_bytes = 0
                member = path.name if size <= CHUNK_BYTES else path.name + f'.chunk{chunk_number:04d}'
                entry = {'member': member, 'source_offset': offset, 'length': len(data),
                         'sha256': hashlib.sha256(data).hexdigest(), 'part_number': len(groups)}
                groups[-1].append((path, entry))
                source['chunks'].append(entry)
                group_bytes += len(data)
                offset += len(data)
                chunk_number += 1
                if not data:
                    break
        source['sha256'] = digest.hexdigest()
        manifest['sources'].append(source)
    bundles = []
    for number, group in enumerate(groups, 1):
        part = Path(str(prefix) + f'-Evidence-Part{number:03d}.zip')
        with zipfile.ZipFile(part, 'x', zipfile.ZIP_DEFLATED) as archive:
            for path, entry in group:
                with path.open('rb') as stream:
                    stream.seek(entry['source_offset'])
                    data = stream.read(entry['length'])
                require(len(data) == entry['length'] and hashlib.sha256(data).hexdigest() == entry['sha256'], 'A retained source changed while packaging; all raw files/full ZIP retained.')
                archive.writestr(entry['member'], data)
        require(part.stat().st_size <= MAX_UPLOAD_BYTES, 'An upload part exceeds its size bound; all raw files/full ZIP retained.')
        digest = hashlib.sha256()
        with part.open('rb') as stream:
            for data in iter(lambda: stream.read(1024 * 1024), b''):
                digest.update(data)
        manifest['parts'].append({'part_number': number, 'name': part.name,
                                  'size_bytes': part.stat().st_size, 'sha256': digest.hexdigest()})
        bundles.append(part)
    save('Upload-Index', json.dumps(manifest, indent=2) + '\n')
    print('The full ZIP exceeds 30 MiB. It is retained locally; upload the smaller parts below instead.', flush=True)
    return bundles, Path(str(prefix) + '-Upload-Index.txt')

def run():
    signal.signal(signal.SIGTERM, lambda *_: (_ for _ in ()).throw(KeyboardInterrupt()))
    code = 0
    try:
        main()
    except (Exception, KeyboardInterrupt) as error:
        code = 1
        print('\nSTOPPED: '+(str(error) or 'Interrupted'),flush=True)
        print('This session has ended. Its evidence is retained. A later workflow test starts a separate session after fresh checks.' if attempted else 'No install or launch attempt was consumed; no device update or forced stop was performed.',flush=True)
    finally:
        try:
            try:
                close_live_input()
            except Exception as input_error:
                print('Live-note cleanup failed; collecting retained evidence: '+str(input_error), flush=True)
                try:
                    if prefix is not None: save('Live-Input-Cleanup-Error', str(input_error)+'\n')
                except Exception:
                    pass
            collect()
        finally:
            stop_capture()
    return code

if __name__ == '__main__':
    raise SystemExit(run())
