from pathlib import Path
import contextlib, hashlib, io, json, os, runpy, shlex, subprocess, tempfile


def check(source, reference_apk=None):
    ns = runpy.run_path(str(source))
    apk = reference_apk.read_bytes() if reference_apk else b'APK fixture for transport guards'
    if reference_apk:
        assert hashlib.sha256(apk).hexdigest() == ns['APK_SHA']
    else:
        ns['prepare'].__globals__['APK_SHA'] = hashlib.sha256(apk).hexdigest()
        ns['APK_SHA'] = hashlib.sha256(apk).hexdigest()
    compile(source.read_text(), str(source), 'exec')
    cases = ['success', 'wrong-apk', 'running-game', 'unknown-process',
             'wrong-attempt', 'already-incomplete', 'bad-report', 'partial-copy',
             'partial-transfer', 'rename-failure', 'runtime-race', 'process-race']
    for case in cases:
        with tempfile.TemporaryDirectory(prefix='tooie-recovery-check-') as tmp:
            root = Path(tmp)
            device = root / 'device'
            state = device / 'files/vulkan-driver'
            state.mkdir(parents=True)
            folder = root / 'candidate'
            folder.mkdir()
            report = {'source_commit': ns['SOURCE'], 'package': ns['PACKAGE'],
                      'sha256': ns['APK_SHA'], 'driver_picker': True}
            if case == 'bad-report':
                report['package'] = 'io.github.banjotooierecompiled.release'
            (folder / 'build-report.json').write_text(json.dumps(report))
            initial = {'pid': 123, 'attempt': 'known-good',
                       'menu_ready': case != 'already-incomplete',
                       'accepted': True, 'driver_name': 'Known system driver'}
            if case == 'wrong-attempt':
                initial['attempt'] = 'other-attempt'
            values = {'boot.json': {'attempt': 'known-good', 'elapsed_ms': 10},
                      'runtime.json': initial,
                      'setup-seen.json': {'schema': 1, 'seen': True}}
            for name, value in values.items():
                (state / name).write_text(json.dumps(value, separators=(',', ':')))
                (state / name).chmod(0o400)
            original = {name: (state / name).read_bytes() for name in values}
            bin_dir = root / 'bin'
            bin_dir.mkdir()

            def tool(name, body):
                file = bin_dir / name
                file.write_text('#!/bin/sh\n' + body + '\n')
                file.chmod(0o700)

            tool('pidof', 'exit 1')
            if case == 'partial-copy':
                tool('cp', 'printf broken > "$3"; exit 0')
            if case == 'partial-transfer':
                tool('base64', 'printf broken; exit 0')
            if case == 'rename-failure':
                tool('mv', 'exit 9')
            mutation_attempts = []

            def runner(argv, **kwargs):
                assert argv[:3] == ['adb-fixture', '-s', 'odin-fixture']
                args = argv[3:]
                result, output, error = 0, b'', b''
                if args == ['get-state']:
                    output = b'device\n'
                elif args == ['shell', 'pm', 'path', ns['PACKAGE']]:
                    output = b'package:/data/app/test/base.apk\n'
                elif args == ['shell', 'pidof', ns['PACKAGE'] + ':game']:
                    result = 0 if case == 'running-game' else 1
                    output = b'123\n' if case == 'running-game' else b''
                    error = b'cannot query process' if case == 'unknown-process' else b''
                elif args[:3] == ['exec-out', 'run-as', ns['PACKAGE']] and args[3] == 'cat':
                    name = args[4]
                    if name == '/data/app/test/base.apk':
                        output = b'wrong' if case == 'wrong-apk' else apk
                    else:
                        output = (device / name).read_bytes()
                elif args[:5] == ['exec-out', 'run-as', ns['PACKAGE'], 'sh', '-c']:
                    mutation_attempts.append(args)
                    if case == 'runtime-race':
                        race = state / '.other-writer'
                        race.write_bytes(original['runtime.json'] + b' ')
                        race.chmod(0o400)
                        race.replace(state / 'runtime.json')
                    if case == 'process-race':
                        tool('pidof', 'echo 456; exit 0')
                    script, = shlex.split(args[5])
                    env = dict(os.environ, PATH=str(bin_dir) + ':' + os.environ['PATH'])
                    return subprocess.run(['/bin/sh', '-c', script], cwd=device,
                                          env=env, **kwargs)
                else:
                    raise AssertionError('Unexpected ADB operation: ' + repr(args))
                return subprocess.CompletedProcess(argv, result, output, error)

            failed = False
            with contextlib.redirect_stdout(io.StringIO()):
                try:
                    ns['prepare'](folder=folder, serial='odin-fixture', adb_path='adb-fixture', runner=runner)
                except SystemExit:
                    failed = True
            if case == 'success':
                assert not failed
                assert json.loads((state / 'runtime.json').read_bytes()) == dict(initial, menu_ready=False)
                assert (state / 'runtime.json').stat().st_mode & 0o777 == 0o400
                backups = list(state.glob('runtime-before-recovery-*.json'))
                assert len(backups) == 1 and backups[0].read_bytes() == original['runtime.json']
            else:
                assert failed, case
                expected = original['runtime.json'] + (b' ' if case == 'runtime-race' else b'')
                assert (state / 'runtime.json').read_bytes() == expected, case
            assert (state / 'boot.json').read_bytes() == original['boot.json']
            assert (state / 'setup-seen.json').read_bytes() == original['setup-seen.json']
            if case in ['wrong-apk', 'running-game', 'unknown-process',
                        'wrong-attempt', 'already-incomplete', 'bad-report']:
                assert not mutation_attempts, case
            print('PASS:', case)
    print('PASS: actual atomic shell publication and 11 rejection controls; no Android-device success claim.')


if __name__ == '__main__':
    import sys
    source = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parents[1] / 'prepare_driver_recovery_7a9858da81fe.py'
    reference = Path(sys.argv[2]) if len(sys.argv) > 2 else None
    check(source, reference)
