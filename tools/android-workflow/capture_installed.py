#!/usr/bin/env python3
"""Attach the existing marker collector to a verified installed Debug APK."""
import argparse
import datetime as dt
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import shlex
import sys
import time
import uuid
import zipfile

HERE = Path(__file__).resolve().parent
LEGACY_LIBRARIES = {'libmain.so', 'libSDL2.so'}
DRIVER_LIBRARIES = LEGACY_LIBRARIES | {'libmain_hook.so', 'libhook_impl.so',
    'libfile_redirect_hook.so', 'libgsl_alloc_hook.so', 'libc++_shared.so'}


def load_module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def require(ok, message):
    if not ok:
        raise RuntimeError(message)


def digest(data):
    return hashlib.sha256(data).hexdigest()


HUD_REQUEST = 'files/data/hud-trace-v1'
HUD_CONTENTS = b'tooie-hud-trace-v1\n'
HUD_HASH = hashlib.sha256(HUD_CONTENTS).hexdigest()
RENDER_REQUEST = 'files/data/render-trace-v1'
RENDER_CONTENTS = b'tooie-render-trace-v1\n'
RENDER_HASH = hashlib.sha256(RENDER_CONTENTS).hexdigest()
CAMERA_REQUEST = 'files/data/camera-trace-v1'
CAMERA_HEADER = b'tooie-camera-trace-v1\n'


def hud_request_script(enable):
    common = 'set -eu\np=' + shlex.quote(HUD_REQUEST) + '\n'
    if enable:
        return common + r'''
[ -d files/data ] || exit 44
if [ -e "$p" ] || [ -L "$p" ]; then
    printf '%s\n' 'HUD trace request already exists; nothing overwritten'
    exit 45
fi
made=0
trap 'if [ "$made" = 1 ]; then rm -- "$p"; fi' 0
(umask 077; set -C; printf '%s\n' tooie-hud-trace-v1 > "$p")
made=1
set -- $(sha256sum "$p")
[ "$1" = ''' + shlex.quote(HUD_HASH) + r''' ]
made=0
trap - 0
printf '%s\n' TOOIE_HUD_TRACE_ENABLED
'''
    return common + r'''
[ ! -L "$p" ] || { printf '%s\n' 'HUD request is a link; nothing removed'; exit 46; }
if [ -e "$p" ]; then
    [ -f "$p" ] || exit 47
    set -- $(sha256sum "$p")
    [ "$1" = ''' + shlex.quote(HUD_HASH) + r''' ] || {
        printf '%s\n' 'HUD request contents differ; nothing removed'; exit 48;
    }
    rm -- "$p"
fi
printf '%s\n' TOOIE_HUD_TRACE_REMOVED
'''


def render_request_script(enable):
    # This is a second closed request, not a general private-file writer.
    return (hud_request_script(enable).replace(HUD_REQUEST, RENDER_REQUEST)
            .replace(HUD_HASH, RENDER_HASH).replace('tooie-hud-trace-v1', 'tooie-render-trace-v1')
            .replace('HUD trace', 'Render trace').replace('HUD request', 'Render request')
            .replace('TOOIE_HUD_TRACE_', 'TOOIE_RENDER_TRACE_'))


def camera_request_script(enable, token):
    require(re.fullmatch('[0-9a-f]{32}', token), 'Invalid camera request token.')
    content = CAMERA_HEADER + token.encode('ascii') + b'\n'
    return (hud_request_script(enable).replace(HUD_REQUEST, CAMERA_REQUEST)
            .replace(HUD_HASH, digest(content))
            .replace('tooie-hud-trace-v1 >', 'tooie-camera-trace-v1 ' + token + ' >')
            .replace('HUD trace', 'Camera trace').replace('HUD request', 'Camera request')
            .replace('TOOIE_HUD_TRACE_', 'TOOIE_CAMERA_TRACE_'))


class HudTraceRequest:
    label, description, request, request_hash = 'HUD', 'HUD layout', HUD_REQUEST, HUD_HASH
    script = staticmethod(hud_request_script)
    def __init__(self):
        self.enabled = self.was_enabled = self.removed = self.closed = False
        self.error = None

    def operation(self, c, enable):
        remote = shlex.join(['run-as', c.PKG, 'sh', '-c', self.script(enable)])
        return c.adb('shell', '-T', remote, name=self.label + '-Trace-' + ('Enable' if enable else 'Disable'), strict=False)

    def enable(self, c):
        result = self.operation(c, True)
        require(result.returncode == 0 and c.text(result) == 'TOOIE_' + self.label.upper() + '_TRACE_ENABLED',
                self.label + ' trace request was not acknowledged; inspect its retained status.')
        self.enabled = self.was_enabled = True
        print('Optional ' + self.description + ' trace enabled for this capture.', flush=True)

    def finish(self, c):
        if self.closed:
            return
        self.closed = True
        if self.enabled:
            try:
                result = self.operation(c, False)
                require(result.returncode == 0 and c.text(result) == 'TOOIE_' + self.label.upper() + '_TRACE_REMOVED',
                        self.label + ' trace request removal was not verified; inspect its retained status.')
                self.enabled = False
                self.removed = True
                print('PASS: ' + self.label + ' trace request removed.', flush=True)
            except (OSError, RuntimeError) as error:
                self.error = str(error)
        if c.prefix is not None:
            c.save(self.label + '-Trace-Request', json.dumps({'requested': True,
                'path': self.request, 'sha256': self.request_hash,
                'enable_acknowledged': self.was_enabled,
                'removal_acknowledged': self.removed, 'error': self.error,
                'scope': 'Only this diagnostic request; no game settings, saves, installation or input.'}, indent=2) + '\n')


class RenderTraceRequest(HudTraceRequest):
    label, description, request, request_hash = 'Render', 'renderer input', RENDER_REQUEST, RENDER_HASH
    script = staticmethod(render_request_script)


class CameraTraceRequest(HudTraceRequest):
    label, description, request = 'Camera', '12-second consecutive camera', CAMERA_REQUEST
    def __init__(self):
        super().__init__()
        self.token = uuid.uuid4().hex
        self.request_hash = digest(CAMERA_HEADER + self.token.encode('ascii') + b'\n')

    def script(self, enable):
        return camera_request_script(enable, self.token)


def finish_before_collection(c, requests):
    # Final log reads and processing can be large. Remove our temporary
    # requests as soon as recording ends, while the device is still connected.
    # Archive/finally remain idempotent fallbacks and retain failed receipts.
    original_collect = c.collect
    def collect():
        for request in requests:
            request.finish(c)
        return original_collect()
    c.collect = collect


def guard_observer_command(args, adb, serial, hud_trace=False, render_trace=False, camera=None):
    values = list(map(str, args))
    if not values or values[0] != str(adb):
        return
    require(values[1:3] == ['-s', serial], 'Observer device scope changed.')
    operation = values[3:]
    require(operation, 'Missing observer command.')
    if operation[0] in ('get-state', 'get-serialno'):
        require(len(operation) == 1, 'Unexpected device query arguments.')
        return
    if operation[0] == 'exec-out':
        require(operation[1:] == ['screencap', '-p'], 'Only foreground screenshot reads are allowed.')
        return
    if operation[0] == 'logcat':
        require('-d' in operation and '-c' not in operation, 'Only saved Logcat reads are allowed.')
        return
    require(operation[0] == 'shell', 'Attach mode cannot install, launch or change app data.')
    shell = operation[1:]
    if shell and shell[0] == '-T':
        shell = shell[1:]
    require(len(shell) == 1, 'Unexpected observer shell arguments.')
    words = shlex.split(shell[0])
    require(words, 'Missing observer shell command.')
    if hud_trace and any(words == ['run-as', 'io.github.banjotooierecompiled', 'sh', '-c', hud_request_script(enable)]
                         for enable in (False, True)):
        return
    if render_trace and any(words == ['run-as', 'io.github.banjotooierecompiled', 'sh', '-c', render_request_script(enable)]
                            for enable in (False, True)):
        return
    if camera is not None and any(words == ['run-as', 'io.github.banjotooierecompiled', 'sh', '-c', camera.script(enable)]
                                  for enable in (False, True)):
        return
    simple = {'date', 'pidof', 'getprop', 'dumpsys', 'sha256sum'}
    if words[0] in simple:
        return
    if words[:2] == ['pm', 'path'] and len(words) == 3:
        require(words[2] == 'io.github.banjotooierecompiled', 'Unexpected package query.')
        return
    if words[:3] == ['run-as', 'io.github.banjotooierecompiled', 'cat']:
        require(words[3:] == ['files/data/banjotooi-debug.log'], 'Unexpected private log path.')
        return
    if words[:5] == ['log', '-p', 'i', '-t', 'TooieUserMark']:
        require(len(words) == 6, 'Unexpected diagnostic marker arguments.')
        return
    if words[0] == 'logcat' and '-d' in words and '-c' not in words:
        return
    raise RuntimeError('Attach mode blocked a command that could change the app or device.')


def verify_local(c, args):
    for value, length in ((args.expect_source, 40), (args.expect_sha256, 64), (args.expect_cert, 64)):
        require(re.fullmatch('[0-9a-f]{' + str(length) + '}', value), 'Invalid expected identity.')
    receipt = json.loads(args.verification.read_text())
    report = json.loads(args.build_report.read_text())
    driver_picker = report.get('driver_picker', False)
    require(type(driver_picker) is bool, 'Invalid driver-picker profile.')
    expected_libraries = DRIVER_LIBRARIES if driver_picker else LEGACY_LIBRARIES
    require(receipt['source_commit'] == report['source_commit'] == args.expect_source,
            'Source identity differs from the selected build.')
    require(receipt['apk_sha256'] == args.expect_sha256 and
            receipt['certificate_sha256'] == args.expect_cert and
            receipt['all_unsigned_payload_matches'] is True,
            'The local signing verification does not match this capture.')
    data = args.apk.read_bytes()
    require(digest(data) == args.expect_sha256, 'Local APK differs from the verified build.')
    with zipfile.ZipFile(args.apk) as archive:
        require(archive.testzip() is None and len(archive.namelist()) == len(set(archive.namelist())),
                'Invalid APK archive.')
        libraries = report['libraries']
        require(set(libraries) == expected_libraries, 'Incomplete CI native-library inventory.')
        packaged = {name for name in archive.namelist() if name.startswith('lib/') and name.endswith('.so')}
        require(packaged == {'lib/arm64-v8a/' + name for name in expected_libraries},
                'Packaged native-library inventory differs from the selected profile.')
        for name, identity in libraries.items():
            require(name == Path(name).name and name.endswith('.so'), 'Invalid library name.')
            require(digest(archive.read('lib/arm64-v8a/' + name)) == identity['sha256'],
                    'Packaged native library differs from the CI report: ' + name)
        native = archive.read('lib/arm64-v8a/libmain.so')
        require(native[:6] == b'\x7fELF\x02\x01' and native[18:20] == b'\xb7\x00', 'Expected ARM64 ELF.')
        require(report['native_version'].encode() in native, 'Native version stamp differs.')
        if getattr(args, 'hud_trace', False):
            require(all(value in native for value in (b'hud-trace-v1', b'tooie_hud_component_begin',
                    b'tooie_hud_component_callback', b'tooie_hud_component_end')),
                    'This APK does not contain the optional HUD layout observer.')
        if getattr(args, 'render_trace', False):
            require(all(value in native for value in (b'render-trace-v1',
                    b'call_first=%u call_pages=%u', b'z_compare=%u z_write=%u')),
                    'This APK does not contain the paged renderer input observer.')
        if getattr(args, 'camera_trace', False):
            require(all(value in native for value in (b'camera-trace-v1', b'camera-step',
                    b'camera-matrix', b'cpu-camera-inputs-not-gpu-readback')),
                    'This APK does not contain the consecutive camera observer.')
    build_id = libraries['libmain.so']['build_id']
    require(re.fullmatch('[0-9a-f]{40}', build_id), 'Invalid CI Build ID.')
    signature = c.text(c.command([args.build_tools / 'apksigner', 'verify', '--verbose',
                                  '--print-certs', args.apk]))
    certificates = re.findall(r'^Signer #\d+ certificate SHA-256 digest: ([0-9a-fA-F:]+)\s*$', signature, re.M)
    require([x.replace(':', '').lower() for x in certificates] == [args.expect_cert],
            'APK signing certificate differs.')
    manifest = c.text(c.command([args.build_tools / 'aapt2', 'dump', 'badging', args.apk]))
    require("package: name='io.github.banjotooierecompiled'" in manifest and
            'application-debuggable' in manifest, 'Expected the existing Debug app.')
    return receipt, report, build_id, signature, manifest


def follow_up_records(events):
    result = []
    targets = {}
    orphan_notes = []
    for event in events:
        if event['kind'] == 'event':
            item = {'event_id': event['id'], 'sequence': event['sequence'], 'marker': event['marker'],
                    'host_read_wall': event['host_read_wall'], 'description': event['description'],
                    'notes': [], 'status': 'needs-review', 'classification': 'unclassified'}
            targets[event['id']] = item
            result.append(item)
        else:
            target = targets.get(event.get('target_event_id'))
            if target is None:
                orphan_notes.append(dict(event))
            else:
                target['notes'].append({'sequence': event['sequence'], 'host_read_wall': event['host_read_wall'],
                                        'description': event['description']})
    return {'schema': 1, 'events_recorded': len(result),
            'notes_recorded': sum(event['kind'] == 'note' for event in events),
            'items': result, 'orphan_notes': orphan_notes,
            'scope': 'Review every event and follow-up note when evidence is uploaded. '
                     'Marker numbers can repeat; source session, sequence and event ID distinguish observations. '
                     'No issue is resolved or classified automatically.'}


def observe_installed(c):
    hub = c.input_hub()
    hub.phase = 'Gameplay-Installed'
    started = time.monotonic()
    started_wall = dt.datetime.now().astimezone().isoformat()
    index = 0
    next_progress = 300
    try:
        while not c.finish_requested:
            c.require_original_process('Gameplay-Installed-PIDs-' + str(index).zfill(5))
            c.health()
            index += 1
            c.wait_with_live_notes(5, 'Gameplay-Installed')
            elapsed = time.monotonic() - started
            if elapsed >= next_progress:
                print('Capture continues: ' + str(int(elapsed // 60)) + ' minutes. Type save to finish.', flush=True)
                next_progress = (int(elapsed) // 300 + 1) * 300
    finally:
        c.save('Gameplay-01-Window', json.dumps({
            'host_start': started_wall, 'host_end': dt.datetime.now().astimezone().isoformat(),
            'host_start_monotonic_seconds': started, 'host_end_monotonic_seconds': time.monotonic(),
            'requested_seconds': None, 'user_requested_finish': c.finish_requested,
            'scope': 'Continuous capture after the user declared playable gameplay. '
                     'No automatic time limit, install, launch or game input. Elapsed time alone does not prove active play.'
        }, indent=2) + '\n')


def installed_main(c, args, hud=None, render=None, camera=None):
    require(sys.stdin.isatty(), 'Run in the interactive Ubuntu terminal.')
    receipt, report, build_id, signature, manifest = verify_local(c, args)
    c.PROCESS_NAME = c.PKG + (':game' if report.get('driver_picker', False) else '')
    c.SHA = c.NATIVE_SOURCE = args.expect_source
    c.HASH, c.BUILD_ID = args.expect_sha256, build_id
    c.APK, c.REPORT = args.apk, args.build_report
    c.DOWNLOADS = args.downloads
    require(c.DOWNLOADS.is_dir(), 'Downloads directory does not exist.')
    stamp = dt.datetime.now().strftime('%Y%m%d-%H%M%S') + '-' + uuid.uuid4().hex[:8]
    c.prefix = c.DOWNLOADS / ('Tooie-' + c.SHA[:12] + '-Installed-Debug-Odin-' + stamp)
    c.save('Identity', 'commit=' + c.SHA + '\nhash=' + c.HASH + '\nbuild_id=' + build_id +
           '\nserial=' + c.SERIAL + '\ncapture_mode=attach-to-installed-debug\n' +
           'process_name=' + c.PROCESS_NAME + '\n' +
           'capture_tool_revision=' + args.tool_revision + '\n')
    c.save('Build-Report', json.dumps(report, indent=2) + '\n')
    c.save('Signing-Verification', json.dumps(receipt, indent=2) + '\n')
    c.save('SDK-Signature', signature + '\n')
    c.save('SDK-Manifest', manifest + '\n')
    c.save('Authorization', 'User requested a marker-enabled playtest of the existing verified Debug app. '
           'Only observation, local evidence, foreground screenshot attempts and diagnostic markers. '
           'No installation, launch, forced stop, data reset, settings change or automated game input. '
           + ('Explicit diagnostic options also permit creating/removing only their temporary private requests: '
              + ', '.join(request.label for request in (hud, render, camera) if request is not None) + '.\n'
              if any(request is not None for request in (hud, render, camera)) else '\n'))
    require(c.text(c.adb('get-state')) == 'device', 'Expected Odin is unavailable.')
    require(c.text(c.adb('get-serialno')) == c.SERIAL, 'Device serial differs.')
    installed = c.text(c.adb('shell', '-T', 'pm path ' + c.PKG, name='Installed-APK-Path')).splitlines()
    require(len(installed) == 1 and installed[0].startswith('package:/data/app/') and
            installed[0].endswith('/base.apk'), 'Unexpected installed APK path.')
    path = installed[0][len('package:'):]
    actual_hash = c.text(c.adb('shell', '-T', 'sha256sum ' + shlex.quote(path), name='Installed-APK-SHA256'))
    require(re.match(r'^[0-9a-fA-F]{64}(?:\s|$)', actual_hash) and
            actual_hash.split()[0].lower() == c.HASH, 'Installed APK differs; capture stopped without changing it.')
    baseline = c.adb('shell', '-T', 'run-as ' + c.PKG + ' cat files/data/banjotooi-debug.log',
                     name='Baseline', strict=False)
    c.baseline = baseline.stdout if baseline.returncode == 0 else None
    c.capture_start = c.clock('Capture-Start-Device-Time')
    c.attempted = True  # Reused collector's observation/final-read flag, not an installation attempt.
    c.start_capture(c.capture_start)
    if hud is not None:
        hud.enable(c)
    if render is not None:
        render.enable(c)
    print('Verified the installed Debug build. Open Debug and load your existing save if needed.', flush=True)
    if camera is not None:
        print('Reach the narrow passage before typing playing. A 12-second camera burst starts after gameplay verification.', flush=True)
    while True:
        answer = c.prompt('When Banjo is controllable, type playing (or save to finish): ').casefold()
        if answer in ('save', 'stop'):
            c.finish_requested = True
            return
        if answer == 'playing':
            break
        print('Use playing when ready. Numbered notes can also be entered in this terminal.', flush=True)
    c.save('User-Playing-Confirmation', 'The user typed playing; exact input receipt is in the input journal.\n')
    active = c.pids('Attached-Process-PIDs')
    require(len(active) == 1, 'A unique Debug game process could not be confirmed.')
    c.original_pid = active[0]
    foreground = c.text(c.adb('shell', '-T', 'dumpsys activity activities', name='Attached-Process-Foreground'))
    resumed = [line for line in foreground.splitlines()
               if re.search(r'\b(?:topResumedActivity|mResumedActivity)\s*[:=]\s*ActivityRecord\{', line)]
    require(resumed and all(re.search(r'\s' + re.escape(c.PKG) + r'/', line) for line in resumed),
            'Debug foreground could not be confirmed; no automatic launch was attempted.')
    c.save('Original-Launch-PID', str(c.original_pid) + '\n')
    c.save('Attached-Process', 'process_name=' + c.PROCESS_NAME + '\npid=' + str(c.original_pid) + '\n' +
           'Existing unique game-process PID plus package foreground after manual playing confirmation. '
           'No launch command was sent.\n')
    if camera is not None:
        camera.enable(c)
        print('Camera burst armed. Walk through the narrow passage now; logging stops automatically after 12 seconds or 1440 steps.', flush=True)
    print('Recording until save. Use 1–30, optionally followed by a short description.', flush=True)
    print('Reuse numbers freely. Use note 30 DETAILS to annotate the latest marker 30. '
          'The first 30 fresh events attempt screenshots; every note is retained.', flush=True)
    observe_installed(c)
    print('Saving evidence. Leave Debug as it is until the upload filename appears.', flush=True)


def parser():
    result = argparse.ArgumentParser(description=__doc__)
    for name in ('apk', 'verification', 'build-report', 'build-tools', 'downloads'):
        result.add_argument('--' + name, required=True, type=Path)
    for name in ('expect-source', 'expect-sha256', 'expect-cert', 'adb', 'serial'):
        result.add_argument('--' + name, required=True)
    result.add_argument('--state', type=Path, default=Path.home() / 'projects/tooie-tools/workflow')
    result.add_argument('--tool-revision', default='working-copy')
    result.add_argument('--hud-trace', action='store_true', help='Record HUD layout using a temporary private request, removed before archiving.')
    result.add_argument('--render-trace', action='store_true', help='Record paged renderer draw inputs using a temporary private request, removed before archiving.')
    result.add_argument('--camera-trace', action='store_true', help='Arm a bounded consecutive camera burst after verified playing confirmation.')
    return result


def main(argv=None):
    args = parser().parse_args(argv)
    c = load_module('installed_debug_collector', HERE / 'templates/collector.py')
    workflow = load_module('installed_debug_packager', HERE / 'tooie.py')
    c.ADB, c.SERIAL = args.adb, args.serial
    c.GAMEPLAY_ONLY = True
    hud = HudTraceRequest() if args.hud_trace else None
    render = RenderTraceRequest() if args.render_trace else None
    camera = CameraTraceRequest() if args.camera_trace else None
    requests = [request for request in (hud, render, camera) if request is not None]
    c.CAPTURE_SCOPE_OVERRIDE = (
        'ATTACH TO INSTALLED DEBUG: exact local APK hash, expected certificate and CI native-library '
        'hashes checked, then installed base.apk hash matched. User manually opens the app and declares '
        'playing; gameplay observation requires a unique existing game-process PID and package foreground. '
        'Inspect User-Playing-Confirmation and Attached-Process to see whether those steps were reached. No installation, automatic '
        'launch, forced stop, game input or settings change. Capture continues until save, interruption '
        'or loss of the original process. Every marker and follow-up note is retained; the first 30 fresh '
        'events attempt foreground screenshots. Native runtime records are included only when emitted '
        'by this build; historical timing-trace claims are not inherited. Host logging/ADB overhead is '
        'included. Screenshots and elapsed time alone do not establish correct speed or sound.\n')
    if hud is not None:
        c.CAPTURE_SCOPE_OVERRIDE += ('Explicit HUD trace: a temporary hud-trace-v1 request enables bounded read-only '
            'HUD component/command records. Its creation/removal receipts are retained; it changes no game settings.\n')
    if render is not None:
        c.CAPTURE_SCOPE_OVERRIDE += ('Explicit renderer trace: a temporary render-trace-v1 request enables bounded '
            'read-only draw/depth/texture/vertex records with rotating call pages. Creation/removal receipts are retained; '
            'it changes no game settings. Diagnostic logging cost is included in timings.\n')
    if camera is not None:
        c.CAPTURE_SCOPE_OVERRIDE += ('Explicit camera trace: after exact installed identity, game PID and foreground checks, '
            'a unique temporary camera-trace-v1 request arms at most 12 seconds/1440 consecutive processed gameplay-camera '
            'steps. Records copy CPU camera inputs and selected outputs, not GPU readback. Logging overhead is included; '
            'no camera, input, rendering, game setting or save policy changes. The owned request is removed before collection.\n')
    c.TIMING_SCOPE_OVERRIDE = ('Use exact appended records from the attached PID and their stated units. '
        'This capture does not enable or infer compile-time timing/geometry/correction observers. '
        'Absent records are not a pass. Marker times are host receipt times, not exact visual onset.\n')
    original_command = c.command
    def command(args, *positional, **keywords):
        guard_observer_command(args, c.ADB, c.SERIAL, hud is not None, render is not None, camera)
        return original_command(args, *positional, **keywords)
    c.command = command
    c.main = lambda: installed_main(c, args, hud, render, camera)
    finish_before_collection(c, requests)
    original_archive = c.archive_evidence
    def archive():
        c.save('User-Follow-Up', json.dumps(follow_up_records(c.live_events), indent=2, ensure_ascii=False) + '\n')
        for request in requests:
            request.finish(c)
        return original_archive()
    c.archive_evidence = archive
    with workflow.exclusive_lock(args.state, 'device'):
        try:
            code = c.run()
            if c.prefix is not None:
                evidence = Path(str(c.prefix) + '-Evidence.zip')
                if evidence.is_file():
                    workflow.package_evidence(evidence, {'name': 'installed-debug', 'identity': {'commit': c.SHA},
                        'capture_mode': 'attach-only', 'apk_sha256': c.HASH, 'marker_range': [1, 30],
                        'marker_screenshot_limit': 30, 'time_limit_seconds': None})
            errors = [request.error for request in requests if request.error]
            if errors:
                print('STOP: ' + '; '.join(errors), file=sys.stderr, flush=True)
                return 1
            return code
        finally:
            for request in requests:
                request.finish(c)


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, ValueError, KeyError, zipfile.BadZipFile) as error:
        print('STOP: ' + str(error), file=sys.stderr, flush=True)
        raise SystemExit(1)
