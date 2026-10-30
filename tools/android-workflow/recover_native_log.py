#!/usr/bin/env python3
"""Recover an existing Debug capture's appended log with bounded read-only ADB calls."""
import argparse
import base64
import binascii
import contextlib
import fcntl
import hashlib
import json
from pathlib import Path
import re
import shlex
import subprocess
import tempfile
import zipfile

PACKAGE = 'io.github.banjotooierecompiled'
LOG = 'files/data/banjotooi-debug.log'
CHUNK = 4 * 1024 * 1024
PART_TARGET = 25_000_000
MAX_UPLOAD = 30_000_000
MAX_APPENDED = 2 * 1024 * 1024 * 1024


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def digest(data):
    return hashlib.sha256(data).hexdigest()


def file_digest(path):
    result = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(CHUNK), b''):
            result.update(block)
    return result.hexdigest()


def capture_identity(share, expected):
    require(re.fullmatch('[0-9a-f]{64}', expected), 'Invalid pinned Share digest')
    require(file_digest(share) == expected, 'Share ZIP digest differs')
    with zipfile.ZipFile(share) as bundle:
        names = bundle.namelist()
        require(len(names) == len(set(names)), 'Duplicate Share entries')
        manifest = json.loads(bundle.read('share-manifest.json'))
        original = manifest['original_archive']
        require(Path(original).name == original and original.endswith('-Evidence.zip'), 'Invalid capture name')
        prefix = original.removesuffix('-Evidence.zip') + '-'
        identity = dict(line.split('=', 1) for line in bundle.read(prefix + 'Identity.txt').decode().splitlines() if '=' in line)
        require(identity['commit'] == manifest['source_commit'], 'Source identity differs')
        require(re.fullmatch('[0-9a-f]{40}', identity['commit']), 'Invalid source')
        require(re.fullmatch('[0-9a-f]{64}', identity['hash']), 'Invalid installed APK digest')
        baseline = next(row for row in manifest['sources'] if row['name'] == prefix + 'Baseline.txt')
        require(type(baseline['size_bytes']) is int and baseline['size_bytes'] >= 0, 'Invalid baseline length')
        require(re.fullmatch('[0-9a-f]{64}', baseline['sha256']), 'Invalid baseline digest')
        return dict(source_commit=identity['commit'], apk_sha256=identity['hash'], serial=identity['serial'],
                    original_pid=manifest['original_pid'], baseline_bytes=baseline['size_bytes'],
                    baseline_sha256=baseline['sha256'], capture_share_sha256=expected,
                    baseline_entry=baseline['name'], original_archive=original)


@contextlib.contextmanager
def archived_baseline(share, identity):
    """Only consume the original member whose length/hash the pinned Share names."""
    name = identity['baseline_entry']
    require(Path(name).name == name, 'Invalid baseline member name')
    path = share.parent / name
    if path.is_file():
        with path.open('rb') as stream:
            yield stream
    else:
        path = share.parent / identity['original_archive']
        require(path.is_file(), 'Keep the original Evidence ZIP beside the Share ZIP in Downloads')
        with zipfile.ZipFile(path) as archive:
            require(archive.namelist().count(name) == 1, 'Original baseline member missing or duplicated')
            require(archive.getinfo(name).file_size == identity['baseline_bytes'], 'Archived baseline length differs')
            with archive.open(name) as stream:
                yield stream


def baseline_candidates(share, identity):
    """Derive native offsets from verified bytes, including Windows LF expansion."""
    archived_hash, native_hash = hashlib.sha256(), hashlib.sha256()
    archived_size = native_size = 0
    pending = b''
    with archived_baseline(share, identity) as stream:
        for data in iter(lambda: stream.read(CHUNK), b''):
            archived_hash.update(data)
            archived_size += len(data)
            require(archived_size <= identity['baseline_bytes'], 'Archived baseline length differs')
            block = pending + data
            pending = b'\r' if block.endswith(b'\r') else b''
            if pending:
                block = block[:-1]
            block = block.replace(b'\r\n', b'\n')
            native_hash.update(block)
            native_size += len(block)
    native_hash.update(pending)
    native_size += len(pending)
    require(archived_size == identity['baseline_bytes'], 'Archived baseline length differs')
    require(archived_hash.hexdigest() == identity['baseline_sha256'], 'Archived baseline digest differs')
    candidates = []
    if native_size != archived_size:
        candidates.append(dict(mapping='windows-crlf-to-lf', bytes=native_size, sha256=native_hash.hexdigest()))
    candidates.append(dict(mapping='as-archived', bytes=archived_size, sha256=archived_hash.hexdigest()))
    return candidates


def native_script(operation, start=0, length=0):
    require(type(start) is int and type(length) is int and start >= 0 and length >= 0, 'Invalid byte range')
    if operation == 'size':
        return 'stat -c %s ' + LOG
    if operation == 'prefix':
        return f'head -c {length} {LOG} | sha256sum'
    require(operation in ('bytes', 'hash') and length > 0 and length <= MAX_APPENDED, 'Invalid native read')
    # Seek by aligned blocks, then trim at most one block from the pipe.
    # Every range reads only its own bytes rather than rescanning the prefix.
    block = 1024 * 1024
    skip, trim = divmod(start, block)
    count = (trim + length + block - 1) // block
    body = f'dd if={LOG} bs={block} skip={skip} count={count} 2>/dev/null | tail -c +{trim + 1} | head -c {length}'
    return body + (' | sha256sum' if operation == 'hash' else '')


class Device:
    def __init__(self, adb, serial):
        self.adb, self.serial = str(adb), serial

    def command(self, *arguments):
        result = subprocess.run([self.adb, '-s', self.serial, *arguments],
                                stdin=subprocess.DEVNULL, capture_output=True, timeout=30)
        require(result.returncode == 0, 'ADB read failed: ' + result.stderr.decode(errors='replace')[:1000])
        return result.stdout

    def verify(self, expected):
        require(self.command('get-state').strip() == b'device', 'Device unavailable')
        require(self.command('get-serialno').decode().strip() == self.serial == expected['serial'], 'Device serial differs')
        lines = self.command('shell', '-T', 'pm path ' + PACKAGE).decode().splitlines()
        require(len(lines) == 1 and re.fullmatch(r'package:/data/app/[A-Za-z0-9._~+=/-]+/base\.apk', lines[0]), 'Unexpected installed APK path')
        path = lines[0].removeprefix('package:')
        actual = self.command('shell', '-T', 'sha256sum ' + shlex.quote(path)).decode().split()
        require(actual and actual[0] == expected['apk_sha256'], 'Installed APK differs from this capture')

    def native(self, operation, start=0, length=0):
        script = native_script(operation, start, length)
        # Encode before ADB handles stdout: Windows shell transport can expand
        # LF to CRLF even with -T. Never apply text normalization to log bytes.
        if operation == 'bytes':
            script += ' | base64'
        data = self.command('shell', '-T', shlex.join(['run-as', PACKAGE, 'sh', '-c', script]))
        if operation == 'bytes':
            encoded = data.translate(None, b' \t\r\n')
            require(len(encoded) <= 4 * ((length + 2) // 3), 'Oversized encoded native-log read')
            try:
                return base64.b64decode(encoded, validate=True)
            except binascii.Error as error:
                raise RuntimeError('Invalid encoded native-log read') from error
        return data


def recover(device, identity, output_root, candidates=None):
    device.verify(identity)
    end_text = device.native('size').strip()
    require(re.fullmatch(rb'[0-9]+', end_text), 'Invalid log size')
    end = int(end_text)
    if candidates is None:
        candidates = [dict(mapping='as-archived', bytes=identity['baseline_bytes'], sha256=identity['baseline_sha256'])]
    selected = None
    for candidate in candidates:
        if not 0 <= candidate['bytes'] < end:
            continue
        prefix = device.native('prefix', length=candidate['bytes']).decode().split()
        if prefix and prefix[0] == candidate['sha256']:
            selected = candidate
            break
    require(selected is not None, 'Existing log prefix differs from the verified original baseline')
    start = selected['bytes']
    length = end - start
    require(0 < length <= MAX_APPENDED, 'No bounded appended range available; log may have changed')
    print(f'Verified native baseline: {start} bytes ({selected["mapping"]}).', flush=True)
    expected_fields = device.native('hash', start, length).decode().split()
    require(expected_fields, 'Missing appended-log digest')
    expected = expected_fields[0]
    require(re.fullmatch('[0-9a-f]{64}', expected), 'Invalid appended-log digest')
    folder = Path(tempfile.mkdtemp(prefix='Tooie-' + identity['source_commit'][:12] + '-Native-Recovery-', dir=output_root))
    common = {**identity, 'native_baseline_bytes': start, 'native_baseline_sha256': selected['sha256'],
              'baseline_mapping': selected['mapping'], 'transfer_encoding': 'base64-decoded-to-raw-bytes',
              'snapshot_end_bytes': end, 'appended_bytes': length, 'appended_sha256': expected,
              'scope': 'Read-only post-capture snapshot of the original append-only log. May include records after save; original journal/windows remain authoritative. No trace enabling, installation, launch, stop, input or app-data writes.'}
    whole = hashlib.sha256()
    position = 0
    pending = []
    try:
        while position < length:
            index = len(pending) + 1
            path = folder / f'Native-Recovery-{index:03d}.partial'
            pending.append(path)
            part_start = position
            part_hash = hashlib.sha256()
            with zipfile.ZipFile(path, 'x', zipfile.ZIP_DEFLATED) as archive:
                entry_name = f'Native-Appended-Part-{index:03d}.log'
                with archive.open(entry_name, 'w', force_zip64=True) as entry:
                    while position < length:
                        size = min(CHUNK, length - position)
                        data = device.native('bytes', start + position, size)
                        require(len(data) == size, 'Short native-log read; no retry')
                        whole.update(data); part_hash.update(data); entry.write(data)
                        position += size
                        if position % (16 * CHUNK) == 0 or position == length:
                            print(f'Read {position // (1024 * 1024)} / {length // (1024 * 1024)} MiB', flush=True)
                        if archive.fp.tell() >= PART_TARGET:
                            break
                record = {**common, 'part': index, 'entry': entry_name, 'offset': start + part_start,
                          'length': position - part_start, 'sha256': part_hash.hexdigest()}
                archive.writestr('recovery.json', json.dumps(record, indent=2) + '\n')
            require(path.stat().st_size <= MAX_UPLOAD, 'Recovery part exceeds upload budget')
        require(whole.hexdigest() == expected, 'Recovered bytes differ from the verified device snapshot')
        device.verify(identity)
        require(int(device.native('size').strip()) >= end, 'Log truncated during recovery')
        receipt = {**common, 'verified': True, 'parts': len(pending)}
        (folder / 'recovery-complete.json').write_text(json.dumps(receipt, indent=2) + '\n')
        outputs = []
        for path in pending:
            target = path.with_suffix('.zip')
            path.rename(target); outputs.append(target)
        return outputs
    except Exception as error:
        (folder / 'recovery-failed.json').write_text(json.dumps({**common, 'verified': False, 'bytes_read': position, 'error': str(error)}, indent=2) + '\n')
        raise


@contextlib.contextmanager
def device_lock(state):
    require(state.is_dir(), 'Original capture-state folder is missing')
    with (state / 'device.lock').open('a+') as stream:
        try:
            fcntl.flock(stream, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            raise RuntimeError('The capture helper still holds the device lock') from None
        yield


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--share', type=Path, required=True)
    parser.add_argument('--expect-share-sha256', required=True)
    parser.add_argument('--adb', required=True)
    parser.add_argument('--serial', required=True)
    parser.add_argument('--state', type=Path, required=True)
    args = parser.parse_args()
    identity = capture_identity(args.share, args.expect_share_sha256)
    with device_lock(args.state):
        print('Verifying the original local baseline and native byte offsets...', flush=True)
        candidates = baseline_candidates(args.share, identity)
        outputs = recover(Device(args.adb, args.serial), identity, args.share.parent, candidates)
    print('PASS: existing log prefix, installed APK and complete appended snapshot verified.')
    for path in outputs:
        print('UPLOAD:', path)


if __name__ == '__main__':
    try:
        main()
    except (OSError, RuntimeError, ValueError, KeyError, StopIteration, subprocess.SubprocessError, zipfile.BadZipFile) as error:
        raise SystemExit('STOP: ' + str(error))
