from pathlib import Path
import ast
import base64
import contextlib
import hashlib
import importlib.util
import io
import json
import queue
import os
import subprocess as sp
import tempfile
import types
import zipfile

root = Path(__file__).resolve().parents[1]
import sys
sys.path.insert(0, str(root))
import tooie
fixture_area = tempfile.TemporaryDirectory(prefix='tooie-rendered-tests-')
fixture_root = Path(fixture_area.name)
profile = tooie.load_profile(root / 'profiles/current.json')
artifact = {**profile['artifact'], 'sha256':'a'*64, 'build_id':'b'*40, 'apk':str(fixture_root / profile['artifact']['apk']),
            'report':str(fixture_root / profile['artifact']['report'])}
source = fixture_root / 'collector.py'
source.write_text(tooie.render_collector(profile, artifact, fixture_root, fixture_root / 'session'))
builder = fixture_root / 'build.sh'
builder.write_text(tooie.render_builder(profile))
tree = ast.parse(source.read_text())
main_node = next(n for n in tree.body if isinstance(n, ast.FunctionDef) and n.name == 'main')
# This fixture supplies diagnostic strings only. These tests do not claim to
# validate a real ELF, Java bytecode, hardware, or a compatible executable mod.
positive = {'native': [], 'dex': []}
negative = []
for node in ast.walk(main_node):
    if not isinstance(node, ast.For) or not isinstance(node.target, ast.Name) or node.target.id != 'marker':
        continue
    if not isinstance(node.iter, ast.Tuple):
        continue
    condition = node.body[0].value.args[0]
    if isinstance(condition, ast.Compare) and isinstance(condition.comparators[0], ast.Name):
        target = condition.comparators[0].id
        values = [n.value for n in node.iter.elts]
        if isinstance(condition.ops[0], ast.In):
            positive[target].extend(values)
        else:
            negative.extend(values)

checks = 0
def module():
    spec = importlib.util.spec_from_file_location('step91', source)
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value

def response(payload=b'', rc=0, stderr=b''):
    return sp.CompletedProcess([], rc, payload.encode() if isinstance(payload, str) else payload, stderr)

def file(m, suffix):
    return Path(str(m.prefix) + '-' + suffix + '.txt')

class Terminal(io.StringIO):
    def isatty(self): return True

def report_fixture(m,sdl):
    header=profile['report_header']
    report=builder.read_text().split(header+'\n',1)[1].split('\nEOF\n',1)[0]
    replacements={'$FULL_SHA':m.SHA,'$BRANCH':profile['identity']['branch'],
      '$ACTUAL_PARENT':' '.join(profile['identity']['parents']),
      '$TIMING_CXX_FLAGS':'-DTOOIE_ANDROID_TIMING_TRACE=1 -DTOOIE_ANDROID_CORRECTION_TRACE=0 -DTOOIE_ANDROID_GEOMETRY_TRACE=0 -DTOOIE_VULKAN_QUEUE_ISOLATION_TEST=0',
      '$PACKAGED_BUILD_ID':m.BUILD_ID,'$APK_SHA':m.HASH,'$OUTPUT':str(m.APK),
      '$PACKAGED_SDL_SHA':hashlib.sha256(sdl).hexdigest()}
    for key,value in replacements.items():report=report.replace(key,value)
    return header+'\n'+report+'\n'

with tempfile.TemporaryDirectory(prefix='step100-simulated-') as temporary:
    area = Path(temporary)

    def scenario(name, installs=0, launches=0, code=1, windows=0):
        global checks
        work = area / name
        work.mkdir()
        m = module()
        m.GAMEPLAY_ONLY = name.startswith('gameplay-only-')
        m.DEMO_ONLY = m.GAMEPLAY_ONLY or name in ('demo-only-profile', 'demo-only-rejects-gameplay')
        m.DOWNLOADS = work
        m.APK = work / 'fixture.apk'
        m.REPORT = work / 'report.txt'
        m.ADB = str(work / 'fake-adb')
        Path(m.ADB).touch()
        m.READELF = work / 'fake-readelf'
        m.READELF.touch()
        m.MARKER = work / 'consumed'
        native = b'\n'.join(positive['native'])
        dex = b'\n'.join(positive['dex'])
        if name.startswith('missing-native-'):
            native = native.replace(positive['native'][int(name.rsplit('-', 1)[1])], b'')
        if name.startswith('missing-dex-'):
            dex = dex.replace(positive['dex'][int(name.rsplit('-', 1)[1])], b'')
        if name.startswith('forbidden-'):
            native += b'\n' + negative[int(name.rsplit('-', 1)[1])]
        sdl = b'packaged-sdl-fixture'
        art = b'fixture-artwork-not-a-hardware-or-pixel-check'
        m.ARTWORK_HASH = hashlib.sha256(art).hexdigest() # Fixture bytes, not an actual packaged-artwork claim.
        wordmark=b'fixture-wordmark-not-a-visual-check'
        layout=b'fixture-layout-not-a-native-layout-check'
        m.WORDMARK_HASH=hashlib.sha256(wordmark).hexdigest()
        m.LAUNCHER_LAYOUT_HASH=hashlib.sha256(layout).hexdigest()
        with zipfile.ZipFile(m.APK, 'x') as archive:
            for entry, data in {
                'lib/arm64-v8a/libmain.so': native,
                'lib/arm64-v8a/libSDL2.so': sdl, 'classes.dex': dex,
                'assets/program/assets/menu.png': b'wrong' if name == 'bad-art' else art,
                'assets/program/assets/menu-recompiled.png': b'wrong' if name == 'bad-wordmark' else wordmark,
                'assets/program/assets/launcher-layout.json': b'wrong' if name == 'bad-launcher-layout' else layout,
                'AndroidManifest.xml': b'fixture manifest', 'resources.arsc': b'fixture resources',
            }.items(): archive.writestr(entry, data)
        m.HASH = hashlib.sha256(m.APK.read_bytes()).hexdigest()
        report = report_fixture(m, sdl)
        if name == 'bad-report-header':
            report = report.replace(profile['report_header'], 'Unexpected report identity', 1)
        if name == 'legacy-report-header':
            report = report.replace(profile['report_header'], 'Tooie Recompiled Android verified task-delivery-timing build report', 1)
        if name == 'duplicate-report': report += 'Source commit: ' + m.SHA + '\n'
        if name.startswith('report-'):
            key = list(m.expected_report())[int(name.rsplit('-', 1)[1])]
            report = report.replace(key + ': ' + m.expected_report()[key], key + ': WRONG')
        m.REPORT.write_text(report)
        if name == 'bad-hash': m.HASH = '0' * 64
        if name == 'consumed': m.MARKER.mkdir()
        if name == 'consumed-symlink': m.MARKER.symlink_to(work / 'missing-target')
        now = [0.0]
        m.time = types.SimpleNamespace(monotonic=lambda: now[0], sleep=lambda n: now.__setitem__(0, now[0] + n))
        full = name in ('live-notes','gameplay','extension','reprompt','game-crash','game-restart','interrupt-game','dead-reader','demo-only','demo-extension','timed-pid-timeout','save-during-demo','save-during-game','device-marker-filter','demo-only-profile','demo-only-rejects-gameplay','reused-process')
        answers=('raedy\nReAdY' if name=='reprompt' else 'ready')+'\n'
        if full:
            answers+='watching\n'
            if name=='demo-extension':answers+='continue\n'
            if name=='save-during-demo':answers+='demo observed\n'
            else:
                answers+=('save' if name in ('demo-only','demo-extension','reused-process') else 'gameplay')+'\ndemos observed without input\n'
                if name not in ('demo-only','demo-extension','reused-process'):
                    answers+='playing\n'
                    if name!='save-during-game':answers+=('continue\nsave' if name in ('extension','dead-reader') else 'save')+'\n'
                    answers+='areas and input observed\n'
        elif name=='readiness-stop':answers+='stop\n'
        elif name in ('before-game-died','before-game-timeout'):answers='ready\n'
        else:answers='stop\n'
        if name=='live-notes':answers=answers.replace('watching\n','1 demo looks fast\nwatching\n')
        if m.DEMO_ONLY and not m.GAMEPLAY_ONLY:
            answers='ready\nwatching\n'+('gameplay\n' if name=='demo-only-rejects-gameplay' else '')+'save\ndemo observation\n'
        if m.GAMEPLAY_ONLY:
            answers = 'stop\n' if name == 'gameplay-only-launcher-stop' else 'ready\n'
            if name != 'gameplay-only-launcher-stop':
                answers += 'stop\n' if name == 'gameplay-only-loading-stop' else 'playing\n'
                if name != 'gameplay-only-loading-stop':
                    answers += ('continue\nsave\n' if name == 'gameplay-only-extension' else 'save\n') + 'walking and controls observed\n'
        stdin = Terminal(answers)
        if name == 'noninteractive': stdin = io.StringIO(answers)
        m.sys = types.SimpleNamespace(argv=[str(source)], stdin=stdin)
        class FixtureInput:
            def __init__(self):
                self.items=queue.Queue();self.phase='fixture';self.error=None
                for line in answers.splitlines():
                    self.items.put({'text':line,'host_read_wall':'2026-10-19T13:00:00+11:00',
                                    'host_read_mono_seconds':now[0],'phase_at_read':'prompt-fixture'})
            def close(self):pass
        hub=FixtureInput()
        def input_hub():
            m.live_input=hub
            return hub
        m.input_hub=input_hub
        note_sent=[False]
        def wait_with_live_notes(seconds,phase):
            now[0]+=seconds
            if name=='save-during-demo' and phase.startswith('Demo-') and not m.finish_requested:
                m.finish_requested=True;m.save('User-Finish-Request','fixture save during demo\n')
            if name=='save-during-game' and phase.startswith('Gameplay-') and not m.finish_requested:
                m.finish_requested=True;m.save('User-Finish-Request','fixture save during gameplay\n')
            if name=='live-notes' and not note_sent[0]:
                item={'text':'2 green monster arms stretch','host_read_wall':'2026-10-19T13:00:30+11:00',
                      'host_read_mono_seconds':now[0],'phase_at_read':phase}
                m.save('Live-Input-Journal',json.dumps(item)+'\n')
                m.handle_live_note(item,phase)
                note_sent[0]=True
        m.wait_with_live_notes=wait_with_live_notes

        counters = {'install': 0, 'launch': 0}
        phase = {'installed': False, 'launched': False, 'stopped': False, 'game_start': None}
        starts = []
        baseline = b'old retained bytes\n'
        fresh = b''.join((f'Runtime {domain}: mono_us=7000000 pid={pid} tid={pid+1} fixture\n').encode()
                         for domain in m.RUNTIME_DOMAINS for pid in (31000, 32000))
        timeline = (f'2026-10-18 10:00:00.100000 +1100 2000 2001 I ActivityManager: Start proc 31000:{m.PKG}/u0\n'
                    '2026-10-18 10:00:01.100000 +1100 31000 31001 I Tooie: Android mod picker: opening document selection\n'
                    '2026-10-18 10:00:02.100000 +1100 31000 31001 F libc: Fatal signal 11 (SIGSEGV)\n'
                    '2026-10-18 10:00:02.200000 +1100 32001 32001 F DEBUG   : #04 pc 0000 non_repeating_frame\n').encode()

        def command(args, name=None, strict=True, timeout=30):
            args = [str(x) for x in args]
            if args[0] == 'wslpath': return response(r'C:\Users\Frostie\Downloads\fixture.apk')
            if args[0] == str(m.READELF):
                if '-n' in args: return response('Build ID: ' + ('0' * 40 if name_case == 'bad-build-id' else m.BUILD_ID))
                if '-h' in args: return response('Class: ELF64\nMachine: ' + ('X86-64' if name_case == 'bad-abi' else 'AArch64'))
                if '--dyn-syms' in args:
                    suffixes = ('nativeFileDialogResult', 'nativeModFileDialogResult', 'nativePresentationLifecycle')
                    return response('\n'.join('1: 00001 20 FUNC GLOBAL DEFAULT ' + ('UND' if name_case == 'undefined-mod-jni' and 'Mod' in suffix else '7') + ' Java_io_github_banjotooierecompiled_BanjoTooieSDLActivity_' + suffix for suffix in suffixes))
                raise AssertionError(args)
            assert args[:3] == [m.ADB, '-s', m.SERIAL], args
            args = args[3:]
            value = None
            if args == ['get-state']: value = response('device')
            elif args == ['get-serialno']: value = response('wrong' if name_case == 'bad-serial' else m.SERIAL)
            elif args[:2] == ['install', '-r']:
                counters['install'] += 1; phase['installed'] = True
                value = response('unknown completion', -999) if name_case == 'install-timeout' else response('INSTALL_FAILED', 1) if name_case == 'install-fail' else response('Success\r\n')
            elif args[0] == 'logcat': value = response(timeline)
            elif args == ['exec-out', 'screencap', '-p']:
                value = response(base64.b64decode('iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMCAO+aF1QAAAAASUVORK5CYII='))
            else:
                assert args[:2] == ['shell', '-T'], args
                remote = args[2]
                if remote == 'getprop ro.product.model': value = response('wrong' if name_case == 'bad-model' else 'Odin2 Portal')
                elif remote.startswith('pidof '):
                    if name_case=='preflight-pid-timeout' and not phase['installed']:raise RuntimeError('simulated ADB process timeout')
                    if name_case=='before-game-timeout' and name=='Before-Start-Game-PIDs':raise RuntimeError('simulated ADB process timeout')
                    if name_case=='timed-pid-timeout' and name and name.startswith(('Demo-01-PIDs-','Gameplay-01-PIDs-')):raise RuntimeError('simulated ADB process timeout')
                    alive = phase['launched'] and name_case != 'launcher-died'
                    pid = 31000
                    if name_case == 'original-restarted' and alive: pid = 32000
                    if name_case == 'existing-app' and not phase['installed']: alive = True
                    if (name_case == 'auto-started' or name_case.startswith('reused-')) and phase['installed'] and not phase['launched']: alive = True
                    # Exact observed ordering: no PID immediately after install,
                    # then the package process appears before the Activity launch.
                    if name_case == 'reused-process' and name == 'After-Install-PIDs': alive = False
                    if name_case == 'reused-raced' and phase['launched']: pid = 32000
                    if name_case == 'reused-multiple' and name == 'Before-Launch-PIDs':
                        value = response('31000 32000\n\nTOOIE_PIDOF_RC=0\n')
                        if name is not None:
                            m.save(name,value.stdout);m.save(name+'-status','returncode=0\n')
                        return value
                    if name_case == 'reused-check-timeout' and name == 'Launch-Reused-Process-PIDs': raise RuntimeError('simulated reused-PID timeout')
                    if name_case == 'before-game-died' and name == 'Before-Start-Game-PIDs': alive = False
                    if name and name.startswith('Gameplay-01-PIDs-'):
                        if phase['game_start'] is None:
                            phase['game_start'] = now[0]
                            starts.append(now[0])
                        elapsed = now[0] - phase['game_start']
                        if name_case == 'interrupt-game' and elapsed >= 20: raise KeyboardInterrupt()
                        if name_case in ('game-crash', 'game-restart') and elapsed >= 65:
                            alive = name_case == 'game-restart'; pid = 32000
                    payload = (str(pid) + '\n\nTOOIE_PIDOF_RC=0\n') if alive else '\nTOOIE_PIDOF_RC=1\n'
                    value = response(str(pid) + '\n' if alive else b'', 0 if alive else 1) if name == 'Final-PIDs' else response(payload)
                elif remote.startswith('log -p i -t TooieUserMark '): value = response()
                elif remote.startswith('date'): value = response('2026-10-18 10:00:00\r\n')
                elif remote.startswith('pm path '): value = response('package:/data/app/fixture/base.apk\n' if name_case != 'bad-installed-path' else 'package:unexpected\n')
                elif remote.startswith('sha256sum '): value = response(('0' * 64 if name_case == 'bad-installed-hash' else m.HASH) + '  /data/app/fixture/base.apk\n')
                elif remote.startswith('run-as '):
                    data = baseline if name == 'Baseline' else baseline + fresh
                    if name == 'Baseline' and name_case == 'no-baseline': value = response(b'', 1, b'not found')
                    elif name == 'FullLog' and name_case == 'log-read-fail': value = response(b'partial', -999)
                    else: value = response((b'rewritten\n' + fresh) if name_case == 'log-reset' and name == 'FullLog' else data)
                elif remote.startswith('dumpsys activity '):
                    if name == 'Launch-Reused-Process-Foreground':
                        payload = ('mResumedActivity: ActivityRecord{123 u0 '+('other.app' if name_case == 'reused-other-foreground' else m.PKG)+'/'+('.OtherActivity' if name_case == 'reused-wrong-activity' else '.BanjoTooieSDLActivity')+' t7}\n')
                        if name_case == 'reused-empty-foreground': payload = ''
                        value = response(payload, 1 if name_case == 'reused-foreground-fail' else 0)
                    else:
                        value = response('mResumedActivity: ActivityRecord{123 u0 ' + ('other.app' if name_case == 'other-foreground' else m.PKG) + '/.Activity t7}\n')
                elif remote.startswith('dumpsys '): value = response('read-only snapshot\n')
                elif remote.startswith('am start -n '):
                    counters['launch'] += 1; phase['launched'] = True
                    if name_case != 'missing-start-pid' and not name_case.startswith('reused-'):
                        with file(m, 'Logcat').open('ab') as out: out.write(timeline)
                        if name_case=='device-marker-filter':
                            rows=[]
                            for capture,pid in [(m.prefix.name,31000),('other-capture',31000),(m.prefix.name,32000)]:
                                payload={'capture':capture,'original_game_pid':pid,'id':'fixture-'+str(pid),'marker':1,'description':'demo too fast'}
                                rows.append('2026-10-20 19:00:00.100000 +1100 99000 99000 I TooieUserMark: '+json.dumps(payload))
                            with file(m,'Logcat').open('ab') as out:out.write(('\n'.join(rows)+'\n').encode())

                    value = response('Warning: Activity not started') if name_case in ('ambiguous-launch','reused-warning') else response('completion unknown', -999) if name_case == 'launch-timeout' else response('Starting: Intent { cmp=' + m.PKG + '/.BanjoTooieSDLActivity }')
                else: raise AssertionError('Unapproved remote operation: ' + remote)
            assert value is not None, args
            if name is not None:
                m.save(name, value.stdout); m.save(name + '-status', f'returncode={value.returncode}\n'.encode() + value.stderr)
            if strict and value.returncode: raise RuntimeError('simulated command failed')
            return value

        name_case = name
        m.command = command
        original_prompt = m.prompt
        def prompt(message):
            # Simulate slow launcher/picker/loading responses. Timed game
            # windows must still contain the full requested 600/300 seconds.
            now[0] += 137 if message.startswith('Launcher assessment') else 181 if message.startswith('Gameplay readiness') else 0
            return original_prompt(message)
        m.prompt = prompt
        def capture(start):
            m.save('Logcat', b''); m.save('Logcat-reader', 'TOOIE_CAPTURE_READY\n')
            m.reader = types.SimpleNamespace(poll=lambda: 0 if phase['stopped'] or (name_case == 'dead-reader' and now[0] > 1200) else None)
            if name_case == 'capture-fail': raise RuntimeError('capture failed before install')
        m.start_capture = capture
        m.stop_capture = lambda: phase.__setitem__('stopped', True)
        with contextlib.redirect_stdout(io.StringIO()) as out:
            result = m.run()
        assert counters == {'install': installs, 'launch': launches}, (name, counters, out.getvalue())
        assert result == code, (name, result, out.getvalue())
        if m.prefix is not None and result:
            assert file(m, 'Stop-Reason').read_text().strip(), (name, out.getvalue())
        records = sorted(work.glob(m.prefix.name + '-Gameplay-*-Window.txt')) if m.prefix is not None else []
        assert len(records) == windows, (name, len(records), out.getvalue())
        if windows:
            first = json.loads(records[0].read_text())
            if starts:assert starts[0]>=(338 if m.GAMEPLAY_ONLY else 638),(name,starts)
            assert first['requested_seconds'] == 600
            assert first['measured_observer_seconds'] == (70 if name in ('game-crash','game-restart') else 20 if name=='interrupt-game' else 5 if name=='save-during-game' else 600), (name, first)
            assert first['completed_requested_window'] == (name not in ('game-crash','game-restart','interrupt-game','save-during-game'))
            if windows == 2: assert json.loads(records[1].read_text())['measured_observer_seconds'] == 300
        if m.prefix is not None:
            bundles = list(work.glob('*-Evidence.zip'))
            assert len(bundles) == 1, (name, out.getvalue())
            with zipfile.ZipFile(bundles[0]) as archive:
                assert archive.testzip() is None
                for path in work.glob(m.prefix.name + '-*'):
                    if path.suffix in ('.txt', '.png'): assert archive.read(path.name) == path.read_bytes(), (name, path)
            if installs:
                assert m.MARKER.is_dir()
                assert file(m,'FullLog-status').exists()
                if name=='before-game-timeout':assert 'state=unknown' in file(m,'Before-Start-Game-PIDs-Assessment').read_text() and 'Original process ended before Start Game' not in out.getvalue()
                reliable = m.baseline is not None and name not in ('log-reset', 'log-read-fail', 'no-baseline')
                assert file(m, 'NewLog').exists() == reliable
                if reliable:
                    assert file(m, 'NewLog').read_bytes() == fresh
                    for domain in m.RUNTIME_DOMAINS:
                        records = file(m, 'Runtime-' + domain).read_text()
                        if m.original_pid is not None:
                            assert 'pid=31000' in records and 'pid=32000' not in records
                        else: assert not records.strip()
                assert b'non_repeating_frame' in file(m, 'Crash-UI-Mod-Context').read_bytes()
        if name=='live-notes':
            events=[json.loads(x) for x in file(m,'User-Markers').read_text().splitlines()]
            notes=[x for x in events if x.get('kind')=='event']
            assert [x['marker'] for x in notes]==[1,2]
            assert file(m,'User-Marker-Summary').exists()
            assert file(m,'Live-Input-Journal').exists()
            assert 'pid=31000' in file(m,'Actor-Diagnostics').read_text()
            assert 'pid=32000' not in file(m,'Actor-Diagnostics').read_text()
        if m.DEMO_ONLY and not m.GAMEPLAY_ONLY:
            assert 'Gameplay readiness' not in out.getvalue() and not file(m,'User-Gameplay-Readiness').exists()
            assert 'Choose gameplay' not in out.getvalue()
            assert 'no save loading or gameplay' in file(m,'Gameplay-Capture-Plan').read_text()
        if m.GAMEPLAY_ONLY:
            assert 'Intro/demo readiness' not in out.getvalue()
            assert not list(work.glob(m.prefix.name+'-Demo-*-Window.txt'))
            assert not file(m,'User-Demo-Readiness').exists()
            assert 'GAMEPLAY ONLY' in file(m,'Capture-Scope').read_text()
            if name != 'gameplay-only-launcher-stop':
                assert 'no timed demo' in file(m,'Gameplay-Capture-Plan').read_text()
            assert 'no timed demo is required' in out.getvalue().lower()
        if name in ('demo-only','demo-extension','gameplay','save-during-demo','demo-only-profile','demo-only-rejects-gameplay','reused-process'):
            demos=sorted(work.glob(m.prefix.name+'-Demo-*-Window.txt'))
            assert len(demos)==(2 if name=='demo-extension' else 1),(name,demos,out.getvalue())
            for path in demos:
                row=json.loads(path.read_text());assert row['stage']=='intro-automatic-demo' and row['requested_seconds']==300
                assert row['completed_requested_window']==(name!='save-during-demo')
        if name == 'reused-process':
            assessment=json.loads(file(m,'Launch-PID-Assessment').read_text())
            assert assessment['pid']==31000 and not assessment['new_process_start_observed']
            assert file(m,'Original-Launch-PID').read_text().strip()=='31000'
        if name=='device-marker-filter':
            selected=file(m,'User-Marker-Logcat').read_text().splitlines()
            assert len(selected)==1 and '99000 99000' in selected[0]
            assert 'demo too fast' in file(m,'Focused-Logcat').read_text()
            assert 'Runtime gfx-submit:' in file(m,'Task-Delivery-Diagnostics').read_text()
            assert 'pid=32000' not in file(m,'Task-Delivery-Diagnostics').read_text()
        checks += 1

    for case in ['bad-hash', 'bad-art', 'bad-wordmark', 'bad-launcher-layout', 'bad-build-id', 'bad-abi', 'undefined-mod-jni', 'duplicate-report', 'bad-report-header', 'legacy-report-header', 'consumed', 'consumed-symlink', 'noninteractive', 'bad-serial','bad-model','preflight-pid-timeout']:
        scenario(case)
    for index in range(len(module().expected_report())): scenario(f'report-{index}')
    # Every actual native component marker is checked as a rejection case.
    for index, marker in enumerate(positive['native']):
        scenario(f'missing-native-{index}')
    for index, marker in enumerate(positive['dex']):
        if marker in (b'ModImporter', b'openModPicker', b'nativeModFileDialogResult'): scenario(f'missing-dex-{index}')
    for index in range(len(negative)): scenario(f'forbidden-{index}')
    scenario('capture-fail', 0, 0, 1)
    for case in ('install-timeout', 'install-fail', 'bad-installed-path', 'bad-installed-hash','reused-multiple'): scenario(case, 1, 0, 1)
    for case in ('ambiguous-launch', 'launch-timeout', 'launcher-died', 'original-restarted', 'missing-start-pid','before-game-died','before-game-timeout'): scenario(case, 1, 1, 1)
    for case in ('reused-raced','reused-check-timeout','reused-other-foreground','reused-wrong-activity','reused-empty-foreground','reused-foreground-fail','reused-warning'): scenario(case, 1, 1, 1)
    scenario('auto-started',1,1,0)
    scenario('reused-process',1,1,0)
    for case in ('picker-stop', 'readiness-stop', 'existing-app', 'log-reset', 'log-read-fail', 'no-baseline', 'other-foreground'): scenario(case, 1, 1, 0)
    for case in ('gameplay', 'live-notes', 'reprompt', 'game-crash', 'game-restart'): scenario(case, 1, 1, 0, 1)
    scenario('interrupt-game', 1, 1, 1, 1)
    scenario('extension',1,1,0,2)
    for case in ('demo-only','demo-extension','save-during-demo','demo-only-profile','demo-only-rejects-gameplay'):scenario(case,1,1,0)
    for case in ('timed-pid-timeout','save-during-game','device-marker-filter'):scenario(case,1,1,0,1)
    scenario('dead-reader', 1, 1, 1, 1)
    scenario('gameplay-only-normal', 1, 1, 0, 1)
    scenario('gameplay-only-extension', 1, 1, 0, 2)
    scenario('gameplay-only-loading-stop', 1, 1, 0, 0)
    scenario('gameplay-only-launcher-stop', 1, 1, 0, 0)

    # Full evidence remains one local archive even when larger than the sharing
    # limit. The driver, tested separately, creates the compact upload.
    m = module(); m.DOWNLOADS = area; m.prefix = area / 'single-local'
    originals = {'Raw.txt': os.urandom(71003), 'Empty.txt': b'', 'Image.png': os.urandom(11001)}
    for suffix, data in originals.items(): Path(str(m.prefix) + '-' + suffix).write_bytes(data)
    with contextlib.redirect_stdout(io.StringIO()): parts, index = m.archive_evidence()
    assert len(parts) == 1 and index is None
    with zipfile.ZipFile(parts[0]) as archive:
        assert archive.testzip() is None
        for suffix, data in originals.items():
            assert archive.read('single-local-' + suffix) == data
    assert not list(area.glob('single-local-Evidence-Part*.zip'))
    assert not list(area.glob('single-local-Upload-Index.txt'))
    checks += 1

print(f'PASS: {checks} simulated exact report/APK/task/DEX/JNI guards, one-install/launch, demo/gameplay windows, timeout/unknown state, marker selection, original-PID task extraction, exact delta and lossless archives.')
print('No real ADB, device, install, launch, game input, setting or repository operation occurred.')
