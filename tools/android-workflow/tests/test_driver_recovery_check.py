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
             'wrong-attempt', 'already-incomplete', 'bad-report', 'private-read-error',
             'shell-loses-exit-status', 'shell-quote-error', 'shell-false-success',
             'partial-copy', 'partial-transfer', 'rename-failure', 'runtime-race', 'process-race']
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
            if case == 'private-read-error':
                (state / 'runtime.json').chmod(0)
            bin_dir = root / 'bin'
            bin_dir.mkdir()

            def tool(name, body):
                file = bin_dir / name
                file.write_text('#!/bin/sh\n' + body + '\n')
                file.chmod(0o700)

            tool('pidof', 'exit 1')
            tool('run-as', 'test "$1" = ' + shlex.quote(ns['PACKAGE']) +
                 ' || exit 63\nshift\nexec "$@"')
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
                elif args[0] in ('exec-out', 'shell') and args[1:3] == ['run-as', ns['PACKAGE']]:
                    if args[3:] == ['cat', '/data/app/test/base.apk']:
                        assert args[0] == 'exec-out'
                        output = b'wrong' if case == 'wrong-apk' else apk
                    else:
                        writing = False
                        if args[3:5] == ['sh', '-c']:
                            script, = shlex.split(args[5])
                            writing = script.startswith('set -eu\n')
                            if writing:
                                mutation_attempts.append(args)
                                if case == 'runtime-race':
                                    race = state / '.other-writer'
                                    race.write_bytes(original['runtime.json'] + b' ')
                                    race.chmod(0o400)
                                    race.replace(state / 'runtime.json')
                                if case == 'process-race':
                                    tool('pidof', 'echo 456; exit 0')
                                if case == 'shell-false-success':
                                    return subprocess.CompletedProcess(argv, 0, b'sh: command inaccessible\n', b'')
                        else:
                            assert args[3] == 'cat' and args[4].startswith('files/vulkan-driver/')
                        # ADB's exec-out quotes arguments itself; shell joins them.
                        # Run the resulting outer shell too, rather than parsing
                        # away a caller's quote and missing the observed bug.
                        # See Android adb/client/commandline.cpp (adb_shell and exec-out).
                        if args[0] == 'exec-out' or (case == 'shell-quote-error' and args[3:5] == ['sh', '-c']):
                            command = args[1] + ' ' + ' '.join(shlex.quote(x) for x in args[2:])
                        else:
                            command = ' '.join(args[1:])
                        env = dict(os.environ, PATH=str(bin_dir) + ':' + os.environ['PATH'])
                        executed = subprocess.run(['/bin/sh', '-c', command], cwd=device, env=env, **kwargs)
                        if args[0] == 'exec-out':
                            return subprocess.CompletedProcess(argv, 0, executed.stdout + executed.stderr, b'')
                        if case == 'shell-loses-exit-status' and args[3:5] == ['sh', '-c'] and not writing:
                            return subprocess.CompletedProcess(argv, 0, executed.stdout, executed.stderr)
                        return executed
                else:
                    raise AssertionError('Unexpected ADB operation: ' + repr(args))
                return subprocess.CompletedProcess(argv, result, output, error)

            failed = False
            failure = ''
            with contextlib.redirect_stdout(io.StringIO()):
                try:
                    ns['prepare'](folder=folder, serial='odin-fixture', adb_path='adb-fixture', runner=runner)
                except SystemExit as error:
                    failed = True
                    failure = str(error)
            if case == 'private-read-error':
                (state / 'runtime.json').chmod(0o400)
            if case == 'success':
                assert not failed, (case, failure)
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
                        'wrong-attempt', 'already-incomplete', 'bad-report',
                        'private-read-error', 'shell-loses-exit-status', 'shell-quote-error']:
                assert not mutation_attempts, case
            print('PASS:', case)
    print('PASS: ADB escaping/exit-status model, actual outer/inner shell publication and 15 rejection controls; no Android-device success claim.')


if __name__ == '__main__':
    import sys
    source = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parents[1] / 'prepare_driver_recovery_7a9858da81fe.py'
    reference = Path(sys.argv[2]) if len(sys.argv) > 2 else None
    check(source, reference)
