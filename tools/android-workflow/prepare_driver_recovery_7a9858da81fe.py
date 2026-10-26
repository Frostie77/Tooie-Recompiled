"""Prepare a backed-up, incomplete-startup fixture for the exact 7a Odin APK.

The test game must already be closed. This changes only its diagnostic
menu_ready flag; it does not induce a Vulkan failure or change drivers.
"""

from pathlib import Path
import base64, hashlib, json, re, shlex, subprocess, uuid

PACKAGE = 'io.github.banjotooierecompiled.gputest.g7a9858da81fe'
APK_SHA = '81af5c400b4676b678acea1b0e83e583e1c4b402ed949f2e08d65b8d29ea73d9'
SOURCE = '7a9858da81fee55e4b1610396db7920b00f547ac'

def require(ok, message):
    if not ok:
        raise SystemExit('STOP: ' + message)

def prepare(folder, serial, adb_path='adb', runner=subprocess.run):
    def adb(*args, allowed=(0,)):
        result = runner([str(adb_path), '-s', serial, *args], capture_output=True, timeout=60)
        require(result.returncode in allowed,
                'ADB exited ' + str(result.returncode) + ': ' +
                result.stderr.decode(errors='replace').strip())
        return result

    def read(name):
        return adb('shell', 'run-as', PACKAGE, 'cat',
                   'files/vulkan-driver/' + name).stdout

    def closed():
        result = adb('shell', 'pidof', PACKAGE + ':game', allowed=(0, 1))
        require(result.returncode == 1 and not result.stdout.strip() and not result.stderr.strip(),
                'The test game is still running or its state is unknown. Exit it first.')

    report = json.loads((folder / 'build-report.json').read_text())
    require(report['source_commit'] == SOURCE and report['package'] == PACKAGE
            and report['sha256'] == APK_SHA and report.get('driver_picker') is True,
            'Unexpected test build report.')
    require(adb('get-state').stdout.strip() == b'device', 'Odin is unavailable.')
    paths = adb('shell', 'pm', 'path', PACKAGE).stdout.decode().splitlines()
    require(len(paths) == 1 and paths[0].startswith('package:'), 'Unexpected installed APK paths.')
    installed = paths[0][8:].strip()
    require(re.fullmatch(r'/data/app/[A-Za-z0-9_~+./=-]+/base\.apk', installed) is not None
            and '/../' not in installed, 'Unexpected installed APK path.')
    actual_apk = adb('exec-out', 'run-as', PACKAGE, 'cat', installed).stdout
    require(hashlib.sha256(actual_apk).hexdigest() == APK_SHA, 'Installed test APK differs.')
    closed()
    # Validate quoting and a non-zero remote status before changing any file.
    # exec-out escapes its own arguments and does not carry the remote status;
    # the manually quoted sh -c payload belongs to adb shell instead.
    probe = adb('shell', 'run-as', PACKAGE, 'sh', '-c',
                shlex.quote('printf %s TOOIE_RECOVERY_SHELL\nexit 73\n'), allowed=(0, 73))
    require(probe.returncode == 73 and probe.stdout == b'TOOIE_RECOVERY_SHELL'
            and not probe.stderr.strip(), 'ADB shell quoting/exit-status control failed.')
    original = {name: read(name) for name in ('boot.json', 'runtime.json', 'setup-seen.json')}
    require(all(0 < len(data) <= 16384 for data in original.values()), 'Unexpected startup-state size.')
    boot, runtime, setup = (json.loads(original[name]) for name in original)
    require(isinstance(boot.get('attempt'), str) and boot['attempt']
            and runtime.get('attempt') == boot['attempt'] and runtime.get('menu_ready') is True,
            'A matching, successfully completed startup is required.')
    require(setup == {'schema': 1, 'seen': True}, 'First-launch setup was not retained.')
    fixture = dict(runtime)
    fixture['menu_ready'] = False
    replacement = json.dumps(fixture, separators=(',', ':'), ensure_ascii=True).encode()
    nonce = uuid.uuid4().hex[:12]
    backup = folder / ('recovery-check-' + nonce)
    backup.mkdir(mode=0o700)
    for name, data in original.items():
        target = backup / name
        target.write_bytes(data)
        target.chmod(0o600)
    old_sha = hashlib.sha256(original['runtime.json']).hexdigest()
    new_sha = hashlib.sha256(replacement).hexdigest()
    remote_backup = 'runtime-before-recovery-' + nonce + '.json'
    temporary = '.runtime-recovery-' + nonce + '.tmp'
    encoded = base64.b64encode(replacement).decode()
    marker = 'TOOIE_RECOVERY_PREPARED_' + nonce
    script = f'''set -eu
umask 077
cd files/vulkan-driver
test ! -L runtime.json
test -f runtime.json
test ! -e {remote_backup}
test ! -e {temporary}
set +e
game_pids=$(pidof {PACKAGE}:game 2>&1)
pid_status=$?
set -e
test "$pid_status" = 1
test -z "$game_pids"
current_sum=$(sha256sum runtime.json)
test "${{current_sum%% *}}" = {old_sha}
cp -p runtime.json {remote_backup}
backup_sum=$(sha256sum {remote_backup})
test "${{backup_sum%% *}}" = {old_sha}
printf %s {encoded} | base64 -d > {temporary}
chmod 400 {temporary}
fixture_sum=$(sha256sum {temporary})
test "${{fixture_sum%% *}}" = {new_sha}
current_sum=$(sha256sum runtime.json)
test "${{current_sum%% *}}" = {old_sha}
set +e
game_pids=$(pidof {PACKAGE}:game 2>&1)
pid_status=$?
set -e
test "$pid_status" = 1
test -z "$game_pids"
mv {temporary} runtime.json
printf %s {marker}
'''
    print('Startup-status backup:', backup, flush=True)
    closed()
    changed = adb('shell', 'run-as', PACKAGE, 'sh', '-c', shlex.quote(script))
    require(changed.stdout == marker.encode() and not changed.stderr.strip(),
            'Recovery command did not confirm completion.')
    require(read('runtime.json') == replacement and read(remote_backup) == original['runtime.json'],
            'Startup-status or backup readback mismatch.')
    require(read('boot.json') == original['boot.json']
            and read('setup-seen.json') == original['setup-seen.json'],
            'Other startup state changed during the check.')
    print('PASS: exact installed test APK verified; startup status backed up and marked incomplete.')
    print('This is a simulated incomplete startup, not a graphics-driver crash.')
    print('Leave the test app closed and paste this output.')

if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--folder', type=Path, required=True)
    parser.add_argument('--serial', required=True)
    parser.add_argument('--adb', default='adb')
    args = parser.parse_args()
    prepare(args.folder, args.serial, args.adb)
