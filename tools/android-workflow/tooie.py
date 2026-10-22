#!/usr/bin/env python3
"""Pinned Android build, repeatable Odin sessions and one compact evidence upload."""
from __future__ import annotations

import argparse
import ast
import contextlib
import datetime as dt
import fcntl
from functools import lru_cache
import hashlib
import json
import os
from pathlib import Path, PureWindowsPath
import re
import shlex
import shutil
import subprocess
import sys
import tempfile
import uuid
import zipfile
import zlib

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


@lru_cache(maxsize=1)
def windows_profile():
    """Read the current Windows account path without embedding a personal name."""
    if not shutil.which('cmd.exe') or not shutil.which('wslpath'):
        return None
    try:
        output = run(['cmd.exe', '/d', '/u', '/c', 'set', 'USERPROFILE'],
                     stdin=subprocess.DEVNULL, capture_output=True, text=True,
                     encoding='utf-16-le', timeout=8).stdout
        values = [line.partition('=')[2] for line in output.splitlines()
                  if line.partition('=')[0].upper() == 'USERPROFILE']
        if len(values) != 1:
            return None
        path = PureWindowsPath(values[0])
        if not path.is_absolute() or not re.fullmatch('[A-Za-z]:', path.drive):
            return None
        value = run(['wslpath', '-u', str(path)], stdin=subprocess.DEVNULL,
                    capture_output=True, text=True, timeout=8).stdout.strip()
        return Path(value) if value and Path(value).is_absolute() else None
    except (OSError, UnicodeError, subprocess.SubprocessError):
        return None


def default_downloads():
    if os.environ.get('TOOIE_DOWNLOADS'):
        return Path(os.environ['TOOIE_DOWNLOADS']).expanduser()
    profile = windows_profile()
    return (profile if profile is not None else Path.home()) / 'Downloads'


def default_adb():
    if os.environ.get('TOOIE_ADB'):
        return os.environ['TOOIE_ADB']
    profile = windows_profile()
    if profile is not None:
        candidate = profile / 'AppData/Local/Android/Sdk/platform-tools/adb.exe'
        if candidate.is_file():
            return str(candidate)
    return shutil.which('adb.exe') or shutil.which('adb') or 'adb'


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
    for key in ('apk', 'report', 'mod', 'mod_report'):
        if key not in profile['artifact']:
            continue
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


def mod_artifact(profile, artifact):
    names = profile['artifact']
    require('mod' in names and 'mod_report' in names, 'This profile has no verified example mod. Build the mod-check profile first.')
    path = Path(artifact['apk']).parent / names['mod']
    report = path.parent / names['mod_report']
    data = json.loads(report.read_text())
    digest = sha256(path)
    fields = report_fields(artifact['report'], profile)
    require(data.get('source_commit') == profile['identity']['commit'] and
            data.get('archive_sha256') == digest == fields.get('Example mod SHA-256'),
            'Example mod does not match its source and APK build report.')
    require(data.get('source_files') == profile['mod_check']['source_files'], 'Example mod source hashes differ.')
    with zipfile.ZipFile(path) as archive:
        require(archive.testzip() is None, 'Example mod ZIP CRC failed.')
        manifest = json.loads(archive.read('mod.json'))
        require(manifest.get('id') == 'tooie_mod_check' and manifest.get('game_id') == 'bt', 'Wrong example mod identity.')
        require(archive.read('mod_syms.bin').startswith(b'N64RSYMS') and archive.read('mod_binary.bin'),
                'Example mod has no executable payload.')
    return {'path': str(path), 'sha256': digest}


def validate_mod_phase(phase):
    require(phase != 'removed',
            'This APK has no Remove option in Mods. The removal test is retired; '
            'use the on/off toggle. A Remove button is not required. No install or launch was started.')
    require(phase in (None, '', 'gold', 'blue', 'persist', 'disabled'), 'Unsupported mod-check phase.')


def render_collector(profile, artifact, downloads, session, presentation_hz=None, gameplay_only=False, mod_check=None, mod=None):
    validate_mod_phase(mod_check)
    require(presentation_hz in (None, 60), 'Unsupported presentation comparison.')
    source = (HERE / 'templates/collector.py').read_text()
    identity = profile['identity']
    replacements = {
        'SHA': repr(identity['commit']), 'HASH': repr(artifact['sha256']),
        'BUILD_ID': repr(artifact['build_id']), 'ENGINEERING': repr(identity['engineering']),
        'NATIVE_SOURCE': repr(identity['commit']), 'DOWNLOADS': 'Path(' + repr(str(downloads)) + ')',
        'ARTWORK_HASH': repr(profile['launcher_assets']['background']),
        'WORDMARK_HASH': repr(profile['launcher_assets']['wordmark']),
        'LAUNCHER_LAYOUT_HASH': repr(profile['launcher_assets']['layout']),
        'APK': 'Path(' + repr(artifact['apk']) + ')', 'REPORT': 'Path(' + repr(artifact['report']) + ')',
        'REPORT_HEADER': repr(profile['report_header']),
        'ADB': repr(default_adb() if profile['device']['adb'] == 'auto' else profile['device']['adb']),
        'SERIAL': repr(profile['device']['serial']),
        'READELF': "Path.home() / " + repr('Android/Sdk/ndk/' + profile['configuration']['ndk'] +
                                             '/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-readelf'),
        'MARKER': 'Path(' + repr(str(session / 'attempt.started')) + ')',
        'DEMO_ONLY': repr(profile.get('demo_only', False)),
        'GAMEPLAY_ONLY': repr(bool(gameplay_only or mod_check)),
        'PRESENTATION_TEST_HZ': repr(presentation_hz or 0),
        'MOD_CHECK': repr(mod_check or ''),
        'MOD_FILE': 'Path(' + repr(mod['path'] if mod else '') + ')',
        'MOD_HASH': repr(mod['sha256'] if mod else ''),
        'MOD_REMOTE': repr('/sdcard/Download/Tooie-Mod-Check-' + identity['commit'][:12] + '-' + session.name + '.nrm' if mod_check == 'gold' else ''),
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
    """Keep the complete capture locally; share exact fresh data within one ZIP."""
    archive = Path(archive).resolve()
    require(archive.is_file(), 'Evidence ZIP is missing.')
    summary = summarize_archive(archive)
    prefix = archive_prefix(archive)
    output = archive.with_name(prefix + '-Share-' + unique_id() + '.zip')
    summary['upload_files'] = [output.name]
    manifest = {'schema': 1, 'original_archive': archive.name,
                'original_archive_bytes': summary['bytes'],
                'original_archive_sha256': summary['sha256'],
                'source_commit': summary['source_commit'],
                'original_pid': summary['original_pid'], 'sources': [],
                'scope': 'Exact included files or explicitly named byte excerpts. '
                         'Full capture and omitted originals remain on the PC. '
                         'This compact upload is not the complete raw archive.'}
    local_only = {prefix + '-Baseline.txt', prefix + '-FullLog.txt'}
    primary = {prefix + '-' + s + '.txt' for s in ('NewLog', 'Logcat', 'Logcat-Dump')}
    with zipfile.ZipFile(archive) as source:
        if prefix + '-NewLog.txt' not in source.namelist():
            # An unreliable/missing delta stays missing. Retain complete or
            # clearly excerpted FullLog as fallback evidence under its own name.
            local_only.remove(prefix + '-FullLog.txt')
            primary.add(prefix + '-FullLog.txt')
    text = ('Tooie Recompiled compact evidence upload\n\n'
            'UPLOAD THIS ZIP ONLY. No other parts, index or handoff are needed.\n'
            'Captured source: ' + str(summary['source_commit'] or 'unavailable; inspect original evidence') + '\n'
            'Workflow profile at packaging: ' + profile['identity']['commit'] + '\n'
            'Original PID: ' + str(summary['original_pid']) + '\n'
            'Complete local archive: ' + archive.name + '\n'
            'Baseline and accumulated FullLog normally stay local; FullLog can provide a missing-delta fallback.\n'
            'Read share-manifest.json for every inclusion, omission and byte excerpt.\n'
            'Summary counts describe the local capture; excerpts may contain fewer records.\n'
            'Rendering and pacing conclusions require the raw records and user observations.\n'
            'Markers record host receipt, which can follow the visible issue.\n'
            'An ADB timeout is an unknown result, not proof that the app crashed.\n'
            'Task IDs correlate records; nested stage maxima must not be added together.\n'
            'No original/working-PC demo landmark baseline is supplied.\n')

    def priority(info):
        name = info.filename
        if name in local_only:
            return (9, name)
        if name.endswith('.txt') and info.file_size <= 256 * 1024 and name not in primary:
            return (0, name)
        if name in primary:
            return (1, name != prefix + '-NewLog.txt', name)
        if name.endswith('.png'):
            # Keep mod acceptance and numbered bug pictures before routine views.
            return (2, 'Mod-Check-Ready-' not in name,
                    'Screenshot-Marker-' not in name, name)
        return (3, name)

    # Reserve space for the source inventory, summary and ZIP directory. Measure
    # compressed bytes with the same raw DEFLATE settings used by ZipFile.
    reserve = min(1024 * 1024, UPLOAD_LIMIT // 4)
    with tempfile.TemporaryDirectory(prefix='.tooie-share-', dir=archive.parent) as temporary:
        staged = Path(temporary) / 'share.zip'
        with zipfile.ZipFile(archive) as original, \
             zipfile.ZipFile(staged, 'x', zipfile.ZIP_DEFLATED, compresslevel=6) as bundle:
            require(all(n.startswith(prefix + '-') for n in original.namelist()),
                    'Mixed capture member prefixes; original evidence retained.')
            bundle.writestr('README.txt', text)
            bundle.writestr('workflow-profile.json', json.dumps(profile, indent=2) + '\n')
            remaining = UPLOAD_LIMIT - bundle.fp.tell() - reserve
            for info in sorted(original.infolist(), key=priority):
                digest = hashlib.sha256()
                compressor = zlib.compressobj(6, zlib.DEFLATED, -15)
                compressed = 0
                with original.open(info) as stream:
                    for data in iter(lambda: stream.read(1024 * 1024), b''):
                        digest.update(data)
                        if info.filename not in local_only:
                            compressed += len(compressor.compress(data))
                compressed += len(compressor.flush())
                entry = {'name': info.filename, 'size_bytes': info.file_size,
                         'sha256': digest.hexdigest(), 'mode': 'local-only', 'stored_files': []}
                cost = compressed + 100 + 2 * len(info.filename.encode('utf-8'))
                if info.filename in local_only:
                    entry['reason'] = 'Accumulated private log/baseline; fresh delta is preferred.'
                elif cost <= remaining:
                    written = hashlib.sha256()
                    with original.open(info) as source, bundle.open(info.filename, 'w') as target:
                        for data in iter(lambda: source.read(1024 * 1024), b''):
                            target.write(data)
                            written.update(data)
                    require(written.hexdigest() == entry['sha256'], 'Source changed during packaging.')
                    remaining -= cost
                    entry['mode'] = 'verbatim'
                    entry['stored_files'].append({'name': info.filename, 'offset': 0,
                        'length': info.file_size, 'sha256': entry['sha256']})
                else:
                    entry['reason'] = 'Single-upload size budget; original retained locally.'
                    # Never label a partial log NewLog. Byte offsets and source
                    # hashes make oversized start/end excerpts explicit.
                    edge = min(128 * 1024, max(0, (remaining - 4096) // 3))
                    if info.filename in primary and edge > 0:
                        entry['mode'] = 'excerpt'
                        for label, offset in [('Start', 0), ('End', max(edge, info.file_size - edge))]:
                            with original.open(info) as source:
                                source.seek(offset)
                                data = source.read(edge)
                            name = info.filename[:-4] + '-' + label + '-Excerpt.txt'
                            bundle.writestr(name, data)
                            remaining -= len(data) + 100 + 2 * len(name.encode('utf-8'))
                            entry['stored_files'].append({'name': name, 'offset': offset,
                                'length': len(data), 'sha256': hashlib.sha256(data).hexdigest()})
                manifest['sources'].append(entry)
            summary['compact_upload'] = {
                'complete_delta_included': any(e['name'] == prefix + '-NewLog.txt' and
                    e['mode'] == 'verbatim' for e in manifest['sources']),
                'verbatim_files': sum(e['mode'] == 'verbatim' for e in manifest['sources']),
                'excerpted_files': sum(e['mode'] == 'excerpt' for e in manifest['sources']),
                'local_only_files': sum(e['mode'] == 'local-only' for e in manifest['sources'])}
            bundle.writestr('summary.json', json.dumps(summary, indent=2, ensure_ascii=False) + '\n')
            bundle.writestr('share-manifest.json', json.dumps(manifest, indent=2) + '\n')
        require(staged.stat().st_size <= UPLOAD_LIMIT,
                'Compact metadata exceeds upload limit; all original evidence retained.')
        with staged.open('rb') as source, output.open('xb') as target:
            shutil.copyfileobj(source, target)
    print('UPLOAD THIS ONE FILE:\n' + str(output), flush=True)
    print('Full raw evidence stays on your PC. Any size-limited omissions/excerpts are listed inside.', flush=True)
    return output, summary


def latest_evidence(downloads):
    candidates = [p for p in downloads.glob('Tooie-*-Evidence.zip') if p.is_file()]
    require(candidates, 'No original evidence ZIP found in Downloads.')
    return max(candidates, key=lambda p: (p.stat().st_mtime_ns, p.name))


def capture(args, profile):
    validate_mod_phase(args.mod_check)
    require(sys.stdin.isatty(), 'Run test in one interactive Ubuntu terminal.')
    artifact = select_artifact(args, profile)
    mod = mod_artifact(profile, artifact) if args.mod_check else None
    with exclusive_lock(args.state, 'device'):
        print('Each session performs a replacement install and launch, retaining app data.\n'
              'Keep using this terminal for prompts and numbered bug notes.\n'
              'Repeat sessions are allowed; an uncertain or failed session stops the batch.', flush=True)
        for number in range(1, args.sessions + 1):
            session = args.state / 'sessions' / unique_id()
            session.mkdir(parents=True)
            script = session / 'collector.py'
            write_new(script, render_collector(profile, artifact, args.downloads, session, args.presentation_hz, args.gameplay_only, args.mod_check, mod))
            json_new(session / 'session.json', {'session': session.name, 'number': number,
                     'requested_sessions': args.sessions, 'artifact': artifact,
                     'requested_presentation_hz': args.presentation_hz,
                     'gameplay_only': args.gameplay_only,
                     'mod_check': args.mod_check,
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
    result.add_argument('--downloads', type=Path, default=default_downloads())
    result.add_argument('--tool-root', type=Path, default=Path.home() / 'projects/tooie-tools')
    sub = result.add_subparsers(dest='command', required=True)
    build_parser = sub.add_parser('build', help='Reuse a verified APK, or incrementally build and verify a pinned version.')
    build_parser.add_argument('--rebuild', action='store_true', help='Build again into a new immutable export folder.')
    build_parser.add_argument('--fresh', action='store_true', help='Build with a new cache; existing caches are retained.')
    build_parser.add_argument('--artifact', type=Path)
    test_parser = sub.add_parser('test', help='Install, launch, capture notes and prepare one compact upload.')
    test_parser.add_argument('--sessions', type=int, default=1)
    test_parser.add_argument('--artifact', type=Path, help='Receipt from a new workflow build.')
    test_parser.add_argument('--presentation-hz', type=int, choices=[60],
                             help='Guide a manual 60 Hz output comparison before Start Game; no automatic setting change.')
    test_parser.add_argument('--gameplay-only', action='store_true',
                             help='Load an existing save and capture ten minutes after playing confirmation, without a timed demo window.')
    test_parser.add_argument('--mod-check', choices=['gold', 'blue', 'persist', 'disabled', 'removed'],
                             help='Guided code-mod check; gold copies the example. Removed is blocked: this APK has no removal UI.')
    package_parser = sub.add_parser('package', help='Prepare one compact upload from existing evidence; no device commands.')
    package_parser.add_argument('evidence', type=Path, nargs='?')
    package_parser.add_argument('--latest', action='store_true', help='Package the newest original evidence ZIP in Downloads.')
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
        require(bool(args.evidence) != args.latest, 'Choose an evidence path or --latest.')
        package_evidence(latest_evidence(args.downloads) if args.latest else args.evidence, profile)
    return 0


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, ValueError, subprocess.SubprocessError, zipfile.BadZipFile) as error:
        print('STOP: ' + str(error), file=sys.stderr, flush=True)
        raise SystemExit(1)
