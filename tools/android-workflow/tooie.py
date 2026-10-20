#!/usr/bin/env python3
"""Pinned Android build, repeatable Odin sessions and lossless evidence handoffs."""
from __future__ import annotations

import argparse
import ast
import contextlib
import datetime as dt
import fcntl
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import sys
import uuid
import zipfile

HERE = Path(__file__).resolve().parent
REPO = 'https://github.com/Frostie77/Tooie-Recompiled.git'
TOOL_BRANCH = 'tools/android-workflow-20261020'
UPLOAD_LIMIT = 30 * 1024 * 1024


def require(ok, message):
    if not ok:
        raise RuntimeError(message)


def sha256(path):
    result = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            result.update(block)
    return result.hexdigest()


def unique_id():
    return dt.datetime.now().strftime('%Y%m%d-%H%M%S') + '-' + uuid.uuid4().hex[:8]


def write_new(path, text):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open('x') as stream:
        stream.write(text)


def json_new(path, value):
    write_new(path, json.dumps(value, indent=2, ensure_ascii=False) + '\n')


def run(args, **kwargs):
    return subprocess.run([str(x) for x in args], check=True, **kwargs)


def git(source, *args):
    return run(['git', '-C', source, *args], stdin=subprocess.DEVNULL,
               capture_output=True, text=True, timeout=180).stdout.strip()


def load_profile(path):
    profile = json.loads(Path(path).read_text())
    require(profile.get('schema') == 1, 'Unsupported version profile.')
    identity = profile['identity']
    for key in ('commit', 'tree', 'engineering', 'branding'):
        require(re.fullmatch('[0-9a-f]{40}', identity[key]), 'Invalid source identity: ' + key)
    require(identity['parents'] == [identity['engineering'], identity['branding']],
            'The reviewed engineering and artwork parents must match.')
    require(profile['device']['package'] == 'io.github.banjotooierecompiled',
            'Unexpected Android package.')
    # This first template supports the reviewed task-delivery configuration.
    # New flags/toolchains require a reviewed template and updated digest.
    require(profile['configuration'] == {
        'ndk': '28.2.13676358', 'api': 24, 'abi': 'arm64-v8a',
        'sdl_prefix': 'SDL2-2.32.10-android-arm64',
        'freetype_prefix': 'freetype-2.13.3-android-arm64',
        'c_flags': '-DTOOIE_ANDROID_TIMING_TRACE=1 -DTOOIE_ANDROID_CORRECTION_TRACE=0 -DTOOIE_ANDROID_GEOMETRY_TRACE=0 -DTOOIE_VULKAN_QUEUE_ISOLATION_TEST=0',
        'cmake_build_type': 'Release', 'descriptor_probe': False},
        'This template does not support the requested build configuration.')
    require(re.fullmatch('[a-z0-9-]+', profile['name']), 'Unsafe profile name.')
    require(set(profile['templates']) == {'build.sh', 'collector.py'}, 'Unexpected templates.')
    for name, digest in profile['templates'].items():
        require(sha256(HERE / 'templates' / name) == digest, 'Template digest differs: ' + name)
    for key in ('apk', 'report'):
        require(Path(profile['artifact'][key]).name == profile['artifact'][key],
                'Artifact names must be simple filenames.')
    return profile


@contextlib.contextmanager
def exclusive_lock(root, name):
    root.mkdir(parents=True, exist_ok=True)
    with (root / (name + '.lock')).open('a+') as stream:
        try:
            fcntl.flock(stream, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            raise RuntimeError('Another workflow command holds the ' + name + ' lock.') from None
        yield


def report_fields(path, profile):
    lines = Path(path).read_text().splitlines()
    require(lines and lines[0] == profile['report_header'], 'Unexpected build-report header.')
    fields = {}
    for line in lines[1:]:
        key, sep, value = line.partition(': ')
        if sep:
            require(key not in fields, 'Duplicate build-report field: ' + key)
            fields[key] = value
    for key, value in profile['report_fields'].items():
        require(fields.get(key) == value, 'Unexpected build-report field: ' + key)
    return fields


def artifact_from_files(apk, report, profile, expected_hash=None, expected_id=None):
    apk, report = Path(apk).resolve(), Path(report).resolve()
    fields = report_fields(report, profile)
    require(fields.get('APK') == str(apk), 'Build report points to a different APK.')
    digest = sha256(apk)
    require(digest == fields.get('APK SHA-256'), 'APK bytes differ from the verified report.')
    build_id = fields.get('ELF Build ID', '')
    require(re.fullmatch('[0-9a-f]{40}', build_id), 'Invalid ELF Build ID.')
    if expected_hash is not None:
        require(digest == expected_hash, 'APK differs from the pinned artifact.')
    if expected_id is not None:
        require(build_id == expected_id, 'Build ID differs from the pinned artifact.')
    return {'schema': 1, 'profile': profile['name'], 'source': profile['identity']['commit'],
            'apk': str(apk), 'report': str(report), 'sha256': digest, 'build_id': build_id}


def default_receipt(args, profile):
    return args.state / 'artifacts' / (profile['identity']['commit'] + '-' + profile['name'] + '.json')


def select_artifact(args, profile):
    receipt_path = getattr(args, 'artifact', None)
    if receipt_path is None and profile['artifact']['sha256'] is None:
        receipt_path = default_receipt(args, profile)
        require(receipt_path.is_file(), 'This revision needs a verified PC build first. Run ~/projects/tooie-tools/tooie build')
    if receipt_path is not None:
        receipt = json.loads(Path(receipt_path).read_text())
        require(receipt.get('schema') == 1 and receipt.get('profile') == profile['name'] and
                receipt.get('source') == profile['identity']['commit'], 'Artifact receipt has a different source/profile.')
        return artifact_from_files(receipt['apk'], receipt['report'], profile,
                                   receipt['sha256'], receipt['build_id'])
    known = profile['artifact']
    return artifact_from_files(args.downloads / known['apk'], args.downloads / known['report'],
                               profile, known['sha256'], known['build_id'])


def replace_assignments(source, values):
    lines = source.splitlines(keepends=True)
    edits = []
    found = set()
    for node in ast.parse(source).body:
        if isinstance(node, ast.Assign) and len(node.targets) == 1 and isinstance(node.targets[0], ast.Name):
            name = node.targets[0].id
            if name in values:
                edits.append((node.lineno - 1, node.end_lineno, name + ' = ' + values[name] + '\n'))
                found.add(name)
    require(found == set(values), 'Collector assignment layout changed.')
    for first, last, text in reversed(edits):
        lines[first:last] = [text]
    return ''.join(lines)


def render_collector(profile, artifact, downloads, session):
    source = (HERE / 'templates/collector.py').read_text()
    identity = profile['identity']
    replacements = {
        'SHA': repr(identity['commit']), 'HASH': repr(artifact['sha256']),
        'BUILD_ID': repr(artifact['build_id']), 'ENGINEERING': repr(identity['engineering']),
        'NATIVE_SOURCE': repr(identity['commit']), 'DOWNLOADS': 'Path(' + repr(str(downloads)) + ')',
        'APK': 'Path(' + repr(artifact['apk']) + ')', 'REPORT': 'Path(' + repr(artifact['report']) + ')',
        'ADB': repr(profile['device']['adb']), 'SERIAL': repr(profile['device']['serial']),
        'READELF': "Path.home() / " + repr('Android/Sdk/ndk/' + profile['configuration']['ndk'] +
                                             '/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-readelf'),
        'MARKER': 'Path(' + repr(str(session / 'attempt.started')) + ')',
        'DEMO_ONLY': repr(profile.get('demo_only', False)),
    }
    source = replace_assignments(source, replacements)
    node = next(n for n in ast.parse(source).body if isinstance(n, ast.FunctionDef) and n.name == 'expected_report')
    lines = source.splitlines(keepends=True)
    fields = dict(profile['report_fields'])
    lines[node.lineno - 1:node.end_lineno] = ['def expected_report():\n    return ' + repr(fields) +
        " | {'APK': str(APK), 'ELF Build ID': BUILD_ID, 'APK SHA-256': HASH}\n"]
    source = ''.join(lines)
    source = source.replace('Tooie-3a8ef82eafc0-Task-Delivery-Timing-Odin-',
                            'Tooie-' + identity['commit'][:12] + '-' + profile['label'] + '-Odin-')
    ast.parse(source)
    return source


def render_builder(profile):
    source = (HERE / 'templates/build.sh').read_text()
    identity = profile['identity']
    tokens = {'IDENTITY': repr(identity), 'MANIFEST': repr(profile['source_manifest']),
              'COMMIT': identity['commit'], 'SHORT': identity['commit'][:12], 'TREE': identity['tree'],
              'ENGINEERING': identity['engineering'], 'BRANDING': identity['branding'], 'BRANCH': identity['branch']}
    for name, value in tokens.items():
        source = source.replace('@@' + name + '@@', value)
    require(not re.search(r'@@[A-Z_]+@@', source), 'Unknown builder token.')
    return source


def fingerprint(paths, extra):
    digest = hashlib.sha256(json.dumps(extra, sort_keys=True).encode())
    records = []
    for root in paths:
        root = Path(root)
        require(root.exists(), 'Missing cache prerequisite: ' + str(root))
        files = sorted(p for p in root.rglob('*') if p.is_file()) if root.is_dir() else [root]
        require(files, 'Empty cache prerequisite: ' + str(root))
        for path in files:
            record = (str(path), sha256(path))
            digest.update(json.dumps(record).encode())
            records.append(record)
    return digest.hexdigest()[:24], records


def cache_configuration(profile, home, tool_root):
    config = profile['configuration']
    ndk = home / 'Android/Sdk/ndk' / config['ndk']
    llvm = ndk / 'toolchains/llvm/prebuilt/linux-x86_64/bin'
    paths = [ndk / 'source.properties', ndk / 'build/cmake', llvm / 'clang', llvm / 'ld.lld',
             home / 'Android/prefixes' / config['sdl_prefix'],
             home / 'Android/prefixes' / config['freetype_prefix'], tool_root / 'dependency-cache']
    versions = {}
    for command in ('cmake', 'ninja', 'gradle', 'java', 'gcc', 'g++', 'clang', 'clang++',
                    'clang-19', 'clang++-19'):
        executable = shutil.which(command)
        if executable is None:
            require(command not in ('cmake', 'ninja', 'gradle'), 'Missing build prerequisite: ' + command)
            versions[command] = None
            continue
        flag = '-version' if command == 'java' else '--version'
        result = run([executable, flag], stdin=subprocess.DEVNULL, capture_output=True, text=True, timeout=30)
        versions[command] = {'path': str(Path(executable).resolve()), 'sha256': sha256(executable),
                             'output': result.stdout + result.stderr}
    extra = {'configuration': config, 'versions': versions,
             'environment': {k: os.environ.get(k) for k in ('CC', 'CXX', 'JAVA_HOME', 'CFLAGS', 'CXXFLAGS', 'LDFLAGS',
                 'AUDIO_CXX', 'SIMD_CXX', 'TOOIE_FIRST_PERSON_CLANG', 'TOOIE_JDK', 'TOOIE_ANDROID_JAR',
                 'TOOIE_AAPT2', 'ANDROID_HOME', 'ANDROID_SDK_ROOT')},
             'workspace_layout': 1}
    key, records = fingerprint(paths, extra)
    return key, {'inputs': records, **extra}


def prepare_source(workspace, profile):
    source = workspace / 'source'
    commit = profile['identity']['commit']
    created = not source.exists()
    if created:
        run(['git', 'clone', '--no-checkout', '--no-tags', '--filter=blob:none', REPO, source],
            stdin=subprocess.DEVNULL, env={**os.environ, 'GIT_TERMINAL_PROMPT': '0'})
    require(source.is_dir() and not source.is_symlink(), 'Unexpected source-cache path.')
    require(git(source, 'remote', 'get-url', 'origin') == REPO, 'Source-cache remote differs.')
    if not created:
        require(not git(source, 'status', '--porcelain', '--untracked-files=no'),
                'Tracked source-cache changes found. They are preserved; use build --fresh.')
    run(['git', '-C', source, 'fetch', '--no-tags', 'origin', commit],
        stdin=subprocess.DEVNULL, env={**os.environ, 'GIT_TERMINAL_PROMPT': '0'})
    run(['git', '-C', source, 'checkout', '--detach', commit], stdin=subprocess.DEVNULL)
    require(git(source, 'rev-parse', 'HEAD') == commit, 'Source-cache pin failed.')
    require(git(source, 'rev-parse', 'HEAD^{tree}') == profile['identity']['tree'], 'Source tree differs.')
    return source


def build(args, profile):
    known = profile['artifact']
    if not args.rebuild and not args.fresh and ((args.downloads / known['apk']).exists() or default_receipt(args, profile).is_file()):
        artifact = select_artifact(args, profile)
        print('Existing APK/report hash and source identity verified. Ready for: ~/projects/tooie-tools/tooie test')
        return artifact
    with exclusive_lock(args.state, 'build'):
        key, inputs = cache_configuration(profile, Path.home(), args.tool_root)
        if args.fresh:
            key += '-' + unique_id()
        workspace = args.state / 'cache' / key
        workspace.mkdir(parents=True, exist_ok=True)
        ownership = workspace / 'workspace.json'
        if ownership.exists():
            require(json.loads(ownership.read_text())['cache_key'] == key, 'Cache ownership differs.')
            print('Reusing compatible native and Gradle build outputs. All host and package checks still run.', flush=True)
        else:
            require(not any(workspace.iterdir()), 'Unrecognized nonempty cache directory.')
            json_new(ownership, {'schema': 1, 'cache_key': key, 'configuration': inputs})
        source = prepare_source(workspace, profile)
        run_id = profile['identity']['commit'][:12] + '-' + unique_id()
        record = args.state / 'build-runs' / run_id
        record.mkdir(parents=True)
        script = record / 'build.sh'
        write_new(script, render_builder(profile))
        run(['bash', '-n', script])
        export = args.downloads / 'Tooie-Builds' / run_id
        export.mkdir(parents=True)
        env = {**os.environ, 'TOOIE_WORKFLOW_EXPORT': str(export),
               'TOOIE_WORKFLOW_RUN': str(record / 'build'), 'TOOIE_WORKFLOW_SOURCE': str(source),
               'TOOIE_WORKFLOW_BUILD': str(workspace / 'build-android'),
               'TOOIE_WORKFLOW_PACKAGE': str(workspace / 'android-package')}
        json_new(record / 'version.json', profile)
        run(['bash', script], env=env)
        artifact = artifact_from_files(export / known['apk'], export / known['report'], profile)
        receipt = record / 'artifact.json'
        json_new(receipt, artifact)
        latest = default_receipt(args, profile)
        pending = latest.with_name(latest.name + '.' + unique_id() + '.tmp')
        json_new(pending, artifact)
        os.replace(pending, latest)
        print('Build complete. Verified artifact saved for: ~/projects/tooie-tools/tooie test')
        return artifact


def archive_prefix(archive):
    require(archive.name.endswith('-Evidence.zip'), 'Select the original *-Evidence.zip.')
    return archive.name[:-len('-Evidence.zip')]


def summarize_archive(archive):
    """Derived counts only; never manufacture a missing byte delta or infer game speed."""
    prefix = archive_prefix(archive)
    result = {'archive': archive.name, 'bytes': archive.stat().st_size, 'sha256': sha256(archive), 'source_commit': None,
              'original_pid': None, 'runtime_counts': {}, 'user_markers': [], 'statuses': {},
              'windows': [], 'scope': 'Derived from retained evidence. No pacing or crash-cause conclusion.'}
    with zipfile.ZipFile(archive) as bundle:
        names = bundle.namelist()
        require(len(names) == len(set(names)), 'Duplicate evidence ZIP member names.')
        require(all('/' not in n and '\\' not in n for n in names), 'Unexpected evidence member path.')
        # The archive is never extracted. Exact member names prevent suffix confusion.
        def read(suffix, limit=4 * 1024 * 1024):
            name = prefix + '-' + suffix + '.txt'
            if name not in names:
                return None
            require(bundle.getinfo(name).file_size <= limit, 'Unexpectedly large evidence metadata: ' + suffix)
            return bundle.read(name).decode(errors='replace')
        for suffix in ('Identity', 'Original-Launch-PID', 'Capture-Scope', 'Timing-Capture-Scope', 'Delta-status'):
            value = read(suffix)
            if value is not None:
                result[suffix] = value
        source = re.search(r'^commit=([0-9a-f]{40})$', result.get('Identity', ''), re.M)
        if source:
            require(prefix.startswith('Tooie-' + source[1][:12] + '-'), 'Capture prefix and retained source identity differ.')
            result['source_commit'] = source[1]
        summary = read('Evidence') or ''
        original = (read('Original-Launch-PID') or '').strip()
        found = re.search(r'original PID\s+(\d+)', summary, re.I)
        if re.fullmatch(r'\d+', original):
            result['original_pid'] = original
        elif found:
            result['original_pid'] = found[1]
        raw = read('User-Markers') or ''
        for line in raw.splitlines():
            try:
                result['user_markers'].append(json.loads(line))
            except ValueError:
                result['user_markers'].append({'unparsed': line})
        for name in names:
            suffix = name[len(prefix) + 1:-4] if name.startswith(prefix + '-') and name.endswith('.txt') else ''
            if suffix.endswith('-status') or suffix.endswith('-error') or suffix.endswith('-Assessment'):
                value = read(suffix, 1024 * 1024)
                if value and ('returncode=0' not in value or 'TIMED OUT' in value or 'unknown' in value):
                    result['statuses'][suffix] = value
            if re.search(r'(?:Demo|Gameplay)-\d+.*(?:Window|Timing|Metadata)', suffix):
                result['windows'].append({'name': suffix, 'text': read(suffix)})
        name = prefix + '-NewLog.txt'
        if name in names:
            # Stream the exact delta. Count only anchored original-process records.
            digest = hashlib.sha256()
            with bundle.open(name) as stream:
                for line in stream:
                    digest.update(line)
                    text = line.decode(errors='replace')
                    match = re.match(r'^Runtime ([a-z-]+):', text)
                    pid = re.search(r'\bpid=(\d+)\b', text)
                    if match and pid and result['original_pid'] and pid[1] == result['original_pid']:
                        domain = match[1]
                        result['runtime_counts'][domain] = result['runtime_counts'].get(domain, 0) + 1
            result['delta_sha256'] = digest.hexdigest()
        result['members'] = len(names)
    return result


def package_evidence(archive, profile):
    archive = Path(archive).resolve()
    require(archive.is_file(), 'Evidence ZIP is missing.')
    summary = summarize_archive(archive)
    prefix = archive_prefix(archive)
    output = archive.with_name(prefix + '-Handoff-' + unique_id() + '.zip')
    uploads = []
    if archive.stat().st_size <= UPLOAD_LIMIT - 1024 * 1024:
        uploads = [archive]
    else:
        index = archive.with_name(prefix + '-Upload-Index.txt')
        require(index.is_file(), 'Large evidence: keep the full ZIP; collector upload index is missing.')
        manifest = json.loads(index.read_text())
        uploads = []
        for part in manifest['parts']:
            require(Path(part['name']).name == part['name'], 'Unsafe upload-part name.')
            path = archive.with_name(part['name'])
            require(path.stat().st_size == part['size_bytes'] and sha256(path) == part['sha256'],
                    'Upload part differs: ' + path.name)
            require(path.stat().st_size <= UPLOAD_LIMIT, 'Upload part is too large.')
            uploads.append(path)
        require(uploads, 'No upload parts found.')
        uploads.append(index)
    summary['upload_files'] = [p.name for p in uploads]
    text = ('Tooie Recompiled evidence handoff\n\n'
            'Captured source: ' + str(summary['source_commit'] or 'unavailable; inspect original evidence') + '\n'
            'Workflow profile at packaging: ' + profile['identity']['commit'] + '\n'
            'Original PID: ' + str(summary['original_pid']) + '\n'
            'Rendering and pacing conclusions require the raw records and user observations.\n'
            'Markers record host receipt, which can follow the visible issue.\n'
            'An ADB timeout is an unknown result, not proof that the app crashed.\n'
            'Task IDs correlate records; nested stage maxima must not be added together.\n'
            'No original/working-PC demo landmark baseline is supplied.\n\n'
            'Runtime record counts: ' + json.dumps(summary['runtime_counts'], sort_keys=True) + '\n'
            'Marker journal records: ' + str(len(summary['user_markers'])) + '\n\n'
            'Share the handoff ZIP' + (' only; it contains the original evidence ZIP.\n' if uploads == [archive]
                                      else ' AND every listed upload part/index:\n' + '\n'.join(p.name for p in uploads) + '\n'))
    with zipfile.ZipFile(output, 'x', zipfile.ZIP_DEFLATED) as bundle:
        bundle.writestr('README.txt', text)
        bundle.writestr('summary.json', json.dumps(summary, indent=2, ensure_ascii=False) + '\n')
        bundle.writestr('workflow-profile.json', json.dumps(profile, indent=2) + '\n')
        if uploads == [archive]:
            # Store the already-compressed ZIP verbatim and leave all original files intact.
            bundle.write(archive, archive.name, compress_type=zipfile.ZIP_STORED)
    require(output.stat().st_size <= UPLOAD_LIMIT, 'Handoff exceeds the upload limit; original evidence retained.')
    print('Share this handoff:\n' + str(output), flush=True)
    if uploads != [archive]:
        print('Also share ALL these evidence parts and index:\n' + '\n'.join(str(p) for p in uploads), flush=True)
    return output, summary


def capture(args, profile):
    require(sys.stdin.isatty(), 'Run test in one interactive Ubuntu terminal.')
    artifact = select_artifact(args, profile)
    with exclusive_lock(args.state, 'device'):
        print('Each session performs a replacement install and launch, retaining app data.\n'
              'Keep using this terminal for prompts and numbered bug notes.\n'
              'Repeat sessions are allowed; an uncertain or failed session stops the batch.', flush=True)
        for number in range(1, args.sessions + 1):
            session = args.state / 'sessions' / unique_id()
            session.mkdir(parents=True)
            script = session / 'collector.py'
            write_new(script, render_collector(profile, artifact, args.downloads, session))
            json_new(session / 'session.json', {'session': session.name, 'number': number,
                     'requested_sessions': args.sessions, 'artifact': artifact,
                     'authorization': 'User invoked repeat-capable workflow. One install/launch per session. Saves retained.'})
            print('\nSESSION ' + str(number) + '/' + str(args.sessions) + ': ' + session.name, flush=True)
            process = subprocess.Popen([sys.executable, '-u', str(script)])
            while True:
                try:
                    code = process.wait()
                    break
                except KeyboardInterrupt:
                    print('Waiting for the collector to finish saving this session.', flush=True)
            json_new(session / 'result.json', {'returncode': code})
            marker = session / 'attempt.started/evidence-prefix.txt'
            summary = None
            if marker.is_file():
                prefix = Path(marker.read_text().strip())
                require(prefix.parent.resolve() == args.downloads.resolve() and
                        prefix.name.startswith('Tooie-' + profile['identity']['commit'][:12] + '-'),
                        'Unexpected capture prefix; all files preserved.')
                archive = Path(str(prefix) + '-Evidence.zip')
                if archive.is_file():
                    _, summary = package_evidence(archive, profile)
            if code:
                print('Session stopped. No automatic retry. Run the test command again after resolving its reported problem.', flush=True)
                return code
            if summary is None:
                print('A complete evidence ZIP was not found. No further session was started; retained files are in ' + str(session), flush=True)
                return 1
            if summary and any('TIMED OUT' in value or 'state=unknown' in value for value in summary['statuses'].values()):
                print('An ADB result was uncertain. Evidence is saved; remaining sessions were not started.', flush=True)
                return 1
    return 0


def update_runner():
    repository = HERE.parents[1]
    require(git(repository, 'remote', 'get-url', 'origin') == REPO, 'Runner remote differs.')
    require(not git(repository, 'status', '--porcelain'), 'Runner checkout has local changes; preserved.')
    run(['git', '-C', repository, 'fetch', '--no-tags', 'origin', TOOL_BRANCH], stdin=subprocess.DEVNULL)
    # Only accept a fast-forward on the isolated tools branch. Never reset user changes.
    run(['git', '-C', repository, 'merge', '--ff-only', 'FETCH_HEAD'], stdin=subprocess.DEVNULL)
    print('Workflow updated. Run the same build/test/package commands as before.')


def parser():
    result = argparse.ArgumentParser(description=__doc__)
    result.add_argument('--profile', type=Path, default=HERE / 'profiles/current.json')
    result.add_argument('--downloads', type=Path, default=Path('/mnt/c/Users/Trent/Downloads'))
    result.add_argument('--tool-root', type=Path, default=Path.home() / 'projects/tooie-tools')
    sub = result.add_subparsers(dest='command', required=True)
    build_parser = sub.add_parser('build', help='Reuse a verified APK, or incrementally build and verify a pinned version.')
    build_parser.add_argument('--rebuild', action='store_true', help='Build again into a new immutable export folder.')
    build_parser.add_argument('--fresh', action='store_true', help='Build with a new cache; existing caches are retained.')
    build_parser.add_argument('--artifact', type=Path)
    test_parser = sub.add_parser('test', help='Install, launch, capture notes and automatically prepare a handoff.')
    test_parser.add_argument('--sessions', type=int, default=1)
    test_parser.add_argument('--artifact', type=Path, help='Receipt from a new workflow build.')
    package_parser = sub.add_parser('package', help='Prepare a handoff from an existing evidence ZIP; no device commands.')
    package_parser.add_argument('evidence', type=Path)
    sub.add_parser('update', help='Explicitly fast-forward this isolated tooling checkout.')
    return result


def main(argv=None):
    args = parser().parse_args(argv)
    args.downloads = args.downloads.expanduser().resolve()
    args.tool_root = args.tool_root.expanduser().resolve()
    args.state = args.tool_root / 'workflow'
    if args.command == 'update':
        with exclusive_lock(args.state, 'device'), exclusive_lock(args.state, 'build'):
            update_runner()
        return 0
    profile = load_profile(args.profile)
    if args.command == 'build':
        build(args, profile)
    elif args.command == 'test':
        require(1 <= args.sessions <= 10, 'Choose 1–10 separately recorded sessions.')
        return capture(args, profile)
    elif args.command == 'package':
        package_evidence(args.evidence, profile)
    return 0


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, ValueError, subprocess.SubprocessError, zipfile.BadZipFile) as error:
        print('STOP: ' + str(error), file=sys.stderr, flush=True)
        raise SystemExit(1)
