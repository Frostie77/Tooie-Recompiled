"""Read-only backup of a closed, debuggable Android app. Never share its output."""
from __future__ import annotations

import datetime as dt
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shlex
import stat
import subprocess
import tarfile
import tempfile
import uuid
import zipfile

MANIFEST = 'private-backup.json'
KIND = 'tooie-private-app-data-backup'
MODEL = 'Odin2 Portal'
ROOTS = ('files', 'shared_prefs', 'databases', 'no_backup')
MAX_BYTES = 8 * 1024 ** 3
MAX_MEMBERS = 100000


def require(ok, message):
    if not ok:
        raise RuntimeError(message)


def digest(stream):
    value = hashlib.sha256()
    for block in iter(lambda: stream.read(1024 * 1024), b''):
        value.update(block)
    return value.hexdigest()


def read(adb, serial, command, label, *, output=None, timeout=30):
    try:
        result = subprocess.run([str(adb), '-s', serial, *command],
                                stdin=subprocess.DEVNULL,
                                stdout=output if output is not None else subprocess.PIPE,
                                stderr=subprocess.PIPE, timeout=timeout)
    except subprocess.TimeoutExpired:
        raise RuntimeError(label + ' timed out; result unknown. No retry or app change.') from None
    require(result.returncode == 0 and not result.stderr.strip(),
            label + ' failed; no verified backup exported.')
    return result.stdout


def stopped(adb, serial, package):
    try:
        result = subprocess.run([str(adb), '-s', serial, 'shell', '-T', 'pidof ' + package],
                                stdin=subprocess.DEVNULL, capture_output=True, timeout=15)
    except subprocess.TimeoutExpired:
        raise RuntimeError('Process check timed out; app state unknown. No retry or app change.') from None
    if result.returncode == 0 and re.fullmatch(rb'[1-9][0-9]*(?:\s+[1-9][0-9]*)*\s*', result.stdout):
        raise RuntimeError('App is running. Close it normally with Exit, then run backup-debug-data again.')
    require(result.returncode == 1 and not result.stdout.strip() and not result.stderr.strip(),
            'Process check is inconclusive; app state unknown. No app change.')


def app_shell(package, script):
    return ['shell', '-T', 'run-as ' + package + ' sh -c ' + shlex.quote(script)]


def roots(adb, serial, package):
    script = ('for d in ' + ' '.join(ROOTS) + '; do '
              'if [ -d "$d" ] && [ ! -L "$d" ]; then printf "%s\\n" "$d"; '
              'elif [ -e "$d" ] || [ -L "$d" ]; then exit 71; fi; done')
    output = read(adb, serial, app_shell(package, script), 'App-directory read')
    names = output.decode('ascii').splitlines()
    require('files' in names and names == [name for name in ROOTS if name in names],
            'Unexpected or missing app-data directories.')
    return names


def safe_name(name, selected):
    if name.startswith('./'):
        name = name[2:]
    name = name.rstrip('/')
    require(name and len(name) <= 4096 and not name.startswith('/') and
            '\\' not in name and not any(ord(c) < 32 or ord(c) == 127 for c in name) and
            all(part not in ('', '.', '..') for part in name.split('/')) and
            PurePosixPath(name).parts[0] in selected,
            'Unsupported app-data archive path; no verified backup exported.')
    return name


def inventory(adb, serial, package, selected):
    command = 'find ' + ' '.join(selected) + ' -type f -exec sha256sum {} +'
    output = read(adb, serial, app_shell(package, command), 'Device file-hash read', timeout=180)
    require(not output or output.endswith(b'\n'), 'Incomplete device file-hash list.')
    files = {}
    for line in output.decode('utf-8').splitlines():
        match = re.fullmatch(r'([0-9a-f]{64})  (.+)', line)
        require(match is not None, 'Unsupported device file-hash list.')
        name = safe_name(match[2], selected)
        require(name not in files and '/' in name, 'Duplicate or invalid device file-hash entry.')
        files[name] = match[1]
        require(len(files) <= MAX_MEMBERS, 'App-data file limit exceeded.')
    return files


def inspect_tar(path, selected):
    require(1024 <= path.stat().st_size <= MAX_BYTES and path.stat().st_size % 512 == 0,
            'Incomplete or oversized app-data TAR.')
    files, seen, end, total = {}, set(), 0, 0
    with tarfile.open(path, mode='r:') as archive:
        for item in archive:
            name = safe_name(item.name, selected)
            require(name not in seen, 'Duplicate app-data TAR member.')
            seen.add(name)
            require(len(seen) <= MAX_MEMBERS, 'App-data member limit exceeded.')
            require(item.isdir() or item.isreg(), 'App data contains an unsupported link or special file.')
            require(not item.sparse and item.size >= 0 and (item.isreg() or item.size == 0),
                    'Unsupported app-data TAR record.')
            end = max(end, item.offset_data + ((item.size + 511) // 512) * 512)
            if item.isreg():
                require('/' in name, 'App-data root is not a directory.')
                total += item.size
                require(total <= MAX_BYTES, 'App-data byte limit exceeded.')
                with archive.extractfile(item) as stream:
                    files[name] = {'sha256': digest(stream), 'bytes': item.size,
                                   'mode': item.mode & 0o777}
    require(set(selected).issubset(seen), 'App-data TAR is missing a selected directory.')
    # tarfile otherwise accepts a missing end marker, so check it explicitly.
    with path.open('rb') as stream:
        stream.seek(end)
        tail_bytes = 0
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            require(not any(block), 'Unexpected data after the app-data TAR end.')
            tail_bytes += len(block)
    require(tail_bytes >= 1024, 'App-data TAR end marker is incomplete.')
    return files


def private_directory(path):
    path = Path(path).expanduser().resolve()
    require(not any((parent / '.git').exists() for parent in (path, *path.parents)),
            'Private app-data backups must stay outside Git checkouts.')
    path.mkdir(mode=0o700, parents=True, exist_ok=True)
    require(path.is_dir() and stat.S_IMODE(path.stat().st_mode) & 0o077 == 0,
            'Private backup directory must have mode 700; no permissions were changed.')
    return path


def create_backup(adb, device, directory):
    serial, package = device['serial'], device['package']
    require(re.fullmatch(r'[A-Za-z0-9._:-]+', serial) and
            package == 'io.github.banjotooierecompiled', 'Unexpected backup device or package.')
    directory = private_directory(directory)
    require(read(adb, serial, ['get-state'], 'Connection read').strip() == b'device',
            'Device connection is not ready.')
    model = read(adb, serial, ['shell', '-T', 'getprop ro.product.model'], 'Model read').decode().strip()
    require(model == MODEL, 'Unexpected Android device model.')
    sdk = read(adb, serial, ['shell', '-T', 'getprop ro.build.version.sdk'], 'Android version read').strip()
    require(re.fullmatch(rb'[0-9]{2,3}', sdk) and int(sdk) >= 24, 'Unexpected Android API level.')
    stopped(adb, serial, package)
    selected = roots(adb, serial, package)
    print('Reading private app-data hashes. Keep the app closed; Odin is unchanged.', flush=True)
    before = inventory(adb, serial, package, selected)
    stopped(adb, serial, package)
    with tempfile.TemporaryDirectory(prefix='.private-copy-', dir=directory) as work:
        tar_path = Path(work) / 'app-data.tar'
        print('Copying private app data, then checking every file. This can take a few minutes.', flush=True)
        with tar_path.open('xb') as stream:
            read(adb, serial, ['shell', '-T', 'run-as ' + package + ' tar -cf - ' + ' '.join(selected)],
                 'Private app-data copy', output=stream, timeout=300)
        stopped(adb, serial, package)
        try:
            files = inspect_tar(tar_path, selected)
        except (tarfile.TarError, EOFError):
            raise RuntimeError('Incomplete or unsupported app-data TAR; no verified backup exported.') from None
        require(selected == roots(adb, serial, package), 'App-data directories changed during backup.')
        after = inventory(adb, serial, package, selected)
        stopped(adb, serial, package)
        require(before == after == {name: item['sha256'] for name, item in files.items()},
                'App-data files changed or copied bytes differ; no verified backup exported.')
        with tar_path.open('rb') as stream:
            tar_sha = digest(stream)
        manifest = {'schema': 1, 'kind': KIND, 'share_allowed': False,
                    'created_utc': dt.datetime.now(dt.timezone.utc).isoformat(),
                    'device': {'serial': serial, 'model': model, 'api': int(sdk)},
                    'package': package, 'roots': selected, 'files': files,
                    'tar_bytes': tar_path.stat().st_size, 'tar_sha256': tar_sha,
                    'consistency': 'App absent at each process check; device file hashes before/after equal copied bytes. '
                                   'This is not an atomic filesystem snapshot.',
                    'excluded': ['cache', 'code_cache', 'native libraries', 'external/shared storage', 'signing keys'],
                    'restoration_tested': False, 'device_modified': False}
        note = ('PRIVATE APP-DATA BACKUP — DO NOT UPLOAD\n\n'
                'May contain your ROM, saves, settings, mods and private logs.\n'
                'Keep this ZIP outside GitHub, chat attachments and evidence uploads.\n'
                'Only the named app-private directories are included; external/shared storage and keys are excluded.\n'
                'Source-file hashes and the saved TAR were checked. Restoration and signing-key migration were not tested.\n'
                'Do not uninstall or reset the existing app based on this backup alone.\n')
        output = directory / ('Tooie-PRIVATE-App-Data-' + dt.datetime.now().strftime('%Y%m%d-%H%M%S') +
                              '-' + uuid.uuid4().hex[:8] + '.zip')
        created = False
        try:
            descriptor = os.open(output, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
            created = True
            with os.fdopen(descriptor, 'wb') as stream, zipfile.ZipFile(stream, 'w', zipfile.ZIP_DEFLATED) as bundle:
                bundle.write(tar_path, 'app-data.tar')
                bundle.writestr(MANIFEST, json.dumps(manifest, indent=2, ensure_ascii=False) + '\n')
                bundle.writestr('KEEP-PRIVATE.txt', note)
            with zipfile.ZipFile(output) as bundle:
                with bundle.open('app-data.tar') as stream:
                    require(digest(stream) == tar_sha, 'Saved backup TAR readback differs.')
                require(json.loads(bundle.read(MANIFEST)) == manifest, 'Saved backup manifest readback differs.')
        except BaseException:
            if created:
                output.unlink()
            raise
    print('PRIVATE BACKUP SAVED — DO NOT UPLOAD:\n' + str(output), flush=True)
    print(str(len(files)) + ' files verified against device hashes. Keep this ZIP private. '
          'No app change or restoration test was performed.', flush=True)
    return output
