"""Host-only workflow tests. Local Git fixtures; no ADB or real game operations."""
from pathlib import Path
import ast
import contextlib
import copy
import datetime as dt
import hashlib
import importlib.util
import io
import json
import os
import re
import subprocess
import sys
import tarfile
import tempfile
import types
import unittest
from unittest.mock import patch
import zipfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
import tooie


class Terminal(io.StringIO):
    def isatty(self):
        return True


class Workflow(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='tooie-workflow-tests-')
        self.root = Path(self.temp.name)
        self.profile = tooie.load_profile(ROOT / 'profiles/current.json')
        self.args = tooie.parser().parse_args(['test'])
        self.args.downloads = self.root / 'downloads'
        self.args.downloads.mkdir()
        self.args.tool_root = self.root / 'tools'
        self.args.state = self.args.tool_root / 'workflow'
        self.output = io.StringIO()
        self.redirect = contextlib.redirect_stdout(self.output)
        self.redirect.__enter__()

    def tearDown(self):
        self.redirect.__exit__(None, None, None)
        self.temp.cleanup()

    def evidence(self, directory=None, uncertain=False, original='31000', extra=None):
        directory = directory or self.args.downloads
        prefix = 'Tooie-' + self.profile['identity']['commit'][:12] + '-Task-Delivery-Timing-Odin-' + tooie.unique_id()
        archive = directory / (prefix + '-Evidence.zip')
        delta = (b'Runtime gfx-submit: pid=31000 tid=31001 n=1\n'
                 b'Runtime gfx-submit: pid=32000 tid=32001 n=7\n'
                 b'Runtime gfx-dispatch: pid=31000 tid=31002 n=1\n')
        with zipfile.ZipFile(archive, 'x') as bundle:
            values = {'Identity': 'commit=' + self.profile['identity']['commit'] + '\n',
                'Original-Launch-PID': original + '\n', 'NewLog': delta,
                'Evidence': 'Runtime records correlated with original PID ' + original + ': {}\n',
                'Demo-01-Window': json.dumps({'requested_seconds': 300, 'completed_requested_window': True}),
                'Focused-Logcat': 'derived text, not a fresh log',
                'User-Markers': json.dumps({'marker': 1, 'description': 'NPC stretches', 'delivery': 'pending'}) + '\n',
                'User-Marker-Logcat': 'device marker', 'Install-status': 'returncode=0\n',
                'FullLog': b'old data\n' + delta}
            if uncertain:
                values['Demo-01-PIDs-000-status'] = 'returncode=-999\nTIMED OUT: completion is unknown'
            values.update(extra or {})
            for suffix, value in values.items():
                bundle.writestr(prefix + '-' + suffix + '.txt', value)
        return archive

    def test_current_profile_and_rendered_shell_parse(self):
        script = self.root / 'build.sh'
        script.write_text(tooie.render_builder(self.profile))
        subprocess.run(['bash', '-n', script], check=True)
        artifact = {**self.profile['artifact'], 'sha256':'a'*64, 'build_id':'b'*40, 'apk': str(self.root / 'game.apk'),
                    'report': str(self.root / 'report.txt')}
        one = tooie.render_collector(self.profile, artifact, self.args.downloads, self.root / 'first')
        two = tooie.render_collector(self.profile, artifact, self.args.downloads, self.root / 'second')
        first = {}; second = {}
        exec(compile(one, '<collector-fixture>', 'exec'), {'__name__': 'fixture', **first}, first)
        # AST verifies distinct consumed-attempt paths without executing any collector function.
        def marker(source):
            return next(n.value for n in ast.parse(source).body if isinstance(n, ast.Assign)
                        and isinstance(n.targets[0], ast.Name) and n.targets[0].id == 'MARKER')
        self.assertNotEqual(ast.dump(marker(one)), ast.dump(marker(two)))
        header = next(n.value.value for n in ast.parse(one).body if isinstance(n, ast.Assign)
                      and isinstance(n.targets[0], ast.Name) and n.targets[0].id == 'REPORT_HEADER')
        self.assertEqual(header, self.profile['report_header'])
        self.assertIn("'install', '-r'", one)
        self.assertIn('gradle --build-cache assembleDebug', script.read_text())
        self.assertNotIn('gradle clean', script.read_text())

    def test_debug_label_overlay_preserves_existing_package_and_inputs(self):
        builder = tooie.render_builder(self.profile)
        package = self.root / 'package'
        main = package / 'app/src/main'
        (main / 'res/values').mkdir(parents=True)
        manifest = main / 'AndroidManifest.xml'
        manifest.write_text('''<manifest xmlns:android="http://schemas.android.com/apk/res/android">
<application android:label="@string/app_name" android:icon="@mipmap/ic_launcher" android:roundIcon="@mipmap/ic_launcher_round">
<activity android:name="io.github.banjotooierecompiled.BanjoTooieSDLActivity" android:label="@string/app_name"/>
</application></manifest>''')
        gradle = package / 'app/build.gradle'
        gradle.write_text("namespace 'io.github.banjotooierecompiled'\napplicationId 'io.github.banjotooierecompiled'\n")
        (main / 'res/values/strings.xml').write_text('<resources><string name="app_name">Tooie Recompiled</string></resources>')
        (main / 'libmain.so').write_bytes(b'unchanged native fixture')
        before = {p: p.read_bytes() for p in package.rglob('*') if p.is_file()}
        overlay = builder.split("<<'SET_DEBUG_APP_LABEL'\n", 1)[1].split('\nSET_DEBUG_APP_LABEL', 1)[0]
        subprocess.run([sys.executable, '-c', overlay, str(package)], check=True)
        after = {p: p.read_bytes() for p in package.rglob('*') if p.is_file()}
        label = package / 'app/src/debug/res/values/strings.xml'
        self.assertEqual(set(after) - set(before), {label})
        self.assertEqual({p: after[p] for p in before}, before)
        check = builder.split("<<'VERIFY_GAMEPLAY_PACKAGE_IDENTITY'\n", 1)[1].split('\nVERIFY_GAMEPLAY_PACKAGE_IDENTITY', 1)[0]
        def verify():
            return subprocess.run([sys.executable, '-c', check, str(manifest), str(gradle)], capture_output=True, text=True)
        self.assertEqual(verify().returncode, 0)
        for resource in ('<resources/>', '<resources><string name="app_name">Tooie Recompiled</string></resources>',
                         '<resources><string name="app_name">Tooie Recompiled Debug</string><string name="app_name">duplicate</string></resources>'):
            label.write_text(resource)
            result = verify()
            self.assertNotEqual(result.returncode, 0)
            self.assertIn('debug packaging label missing or ambiguous', result.stderr)
        old = dict(self.profile, name='texture-aiming-gameplay')
        self.assertNotEqual(tooie.default_receipt(self.args, old), tooie.default_receipt(self.args, self.profile))

    def test_main_tree_provenance_and_corrupt_manifest_controls(self):
        source = self.root / 'source-fixture'
        source.mkdir()
        environment = dict(os.environ, GIT_AUTHOR_NAME='Frostie', GIT_COMMITTER_NAME='Frostie',
                           GIT_AUTHOR_EMAIL='fixture@example.invalid', GIT_COMMITTER_EMAIL='fixture@example.invalid')

        def git(*args):
            return subprocess.run(['git', '-C', str(source), *args], env=environment,
                                  capture_output=True, text=True, check=True).stdout.strip()

        git('init', '-b', 'main')
        (source / 'approved-art.txt').write_text('unchanged artwork fixture\n')
        git('add', '.')
        git('commit', '-m', 'Approved artwork fixture')
        branding = git('rev-parse', 'HEAD')
        (source / 'upload.cpp').write_text('reviewed upload fixture\n')
        git('add', '.')
        git('commit', '-m', 'Reviewed main fixture')
        engineering = git('rev-parse', 'HEAD')
        tree = git('rev-parse', 'HEAD^{tree}')
        commit = git('commit-tree', tree, '-p', engineering, '-p', branding, '-m', 'Provenance fixture')
        git('reset', '--hard', commit)
        mode, kind, sha, path = git('ls-tree', 'HEAD', '--', 'upload.cpp').split()
        entry = {'path': path, 'mode': mode, 'type': kind, 'sha': sha}
        profile = copy.deepcopy(self.profile)
        profile['identity'] = {'commit': commit, 'tree': tree, 'parents': [engineering, branding],
                               'engineering': engineering, 'branding': branding, 'branch': 'main'}
        profile['source_manifest'] = {'mod_entries': [], 'engineering_entries': [entry]}

        def verify(value):
            builder = tooie.render_builder(value)
            body = builder.split("<<'VERIFY_INTEGRATED_SOURCE'\n", 1)[1].split('\nVERIFY_INTEGRATED_SOURCE', 1)[0]
            # Execute the actual tree/parent/delta/blob/cleanliness checks. The
            # unchanged image hash/dimension checks are outside this fixture.
            body = body.split("for root in ('assets'", 1)[0]
            return subprocess.run([sys.executable, '-c', body, str(source)], env=environment,
                                  capture_output=True, text=True)

        self.assertEqual(verify(profile).returncode, 0)
        for mutation, expected in [('unlisted', 'outside the reviewed disjoint parent deltas'),
                                   ('overlap', 'source deltas overlap'),
                                   ('blob', 'exact source blob/mode differs'),
                                   ('parents', 'integration parent order differs')]:
            bad = copy.deepcopy(profile)
            if mutation == 'unlisted':
                bad['source_manifest']['engineering_entries'] = []
            elif mutation == 'overlap':
                bad['source_manifest']['mod_entries'] = [entry]
            elif mutation == 'blob':
                bad['source_manifest']['engineering_entries'][0]['sha'] = '0' * 40
            else:
                bad['identity']['parents'].reverse()
            with self.subTest(mutation=mutation):
                result = verify(bad)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(expected, result.stderr)
        (source / 'upload.cpp').write_text('changed tracked source\n')
        dirty = verify(profile)
        self.assertNotEqual(dirty.returncode, 0)
        self.assertIn('tracked source differs before validation', dirty.stderr)

    def test_template_changes_and_unsupported_flags_rejected(self):
        changed = copy.deepcopy(self.profile)
        changed['templates']['collector.py'] = '0' * 64
        path = self.root / 'profile.json'; path.write_text(json.dumps(changed))
        with self.assertRaisesRegex(RuntimeError, 'digest'):
            tooie.load_profile(path)
        changed = copy.deepcopy(self.profile)
        changed['configuration']['descriptor_probe'] = True
        path.write_text(json.dumps(changed))
        with self.assertRaisesRegex(RuntimeError, 'configuration'):
            tooie.load_profile(path)

    def test_windows_profile_reads_named_variable_without_expansion(self):
        tooie.windows_profile.cache_clear()
        replies = [subprocess.CompletedProcess([], 0, 'USERPROFILE=C:\\Users\\Frostie & Friends\r\n', ''),
                   subprocess.CompletedProcess([], 0, '/mnt/c/Users/Frostie & Friends\n', '')]
        try:
            with patch.object(tooie.shutil, 'which', return_value='/installed/tool'), \
                 patch.object(tooie, 'run', side_effect=replies) as run:
                expected = Path('/mnt/c/Users/Frostie & Friends')
                self.assertEqual(tooie.windows_profile(), expected)
                self.assertEqual(tooie.windows_profile(), expected)
                self.assertEqual(run.call_count, 2)
                self.assertEqual(run.call_args_list[0].args[0],
                                 ['cmd.exe', '/d', '/u', '/c', 'set', 'USERPROFILE'])
                self.assertEqual(run.call_args_list[1].args[0],
                                 ['wslpath', '-u', 'C:\\Users\\Frostie & Friends'])
                self.assertEqual(run.call_args_list[0].kwargs['timeout'], 8)
                self.assertEqual(run.call_args_list[0].kwargs['encoding'], 'utf-16-le')
        finally:
            tooie.windows_profile.cache_clear()

    def test_windows_profile_failure_does_not_guess_an_account(self):
        for reply in ('USERPROFILE=relative\n', 'USERPROFILE=\\\\server\\share\n',
                      'USERPROFILE=C:\\Users\\Frostie\nUSERPROFILE=C:\\Other\n', ''):
            tooie.windows_profile.cache_clear()
            with patch.object(tooie.shutil, 'which', return_value='/installed/tool'), \
                 patch.object(tooie, 'run', return_value=subprocess.CompletedProcess([], 0, reply, '')) as run:
                self.assertIsNone(tooie.windows_profile())
                self.assertEqual(run.call_count, 1)
        tooie.windows_profile.cache_clear()
        try:
            with patch.object(tooie.shutil, 'which', return_value='/installed/tool'), \
                 patch.object(tooie, 'run', side_effect=subprocess.TimeoutExpired('metadata', 8)):
                self.assertIsNone(tooie.windows_profile())
        finally:
            tooie.windows_profile.cache_clear()

    def test_download_and_adb_defaults_use_local_profile_or_explicit_override(self):
        profile = self.root / 'windows-profile'
        adb = profile / 'AppData/Local/Android/Sdk/platform-tools/adb.exe'
        adb.parent.mkdir(parents=True); adb.touch()
        with patch.dict(os.environ, {}, clear=True), patch.object(tooie, 'windows_profile', return_value=profile):
            self.assertEqual(tooie.default_downloads(), profile / 'Downloads')
            self.assertEqual(tooie.default_adb(), str(adb))
        with patch.dict(os.environ, {'TOOIE_DOWNLOADS':str(self.root / 'export'), 'TOOIE_ADB':'/selected/adb'}, clear=True), \
             patch.object(tooie, 'windows_profile', side_effect=AssertionError('unnecessary account read')):
            self.assertEqual(tooie.default_downloads(), self.root / 'export')
            self.assertEqual(tooie.default_adb(), '/selected/adb')
        with patch.dict(os.environ, {}, clear=True), patch.object(tooie, 'windows_profile', return_value=None), \
             patch.object(tooie.Path, 'home', return_value=self.root), \
             patch.object(tooie.shutil, 'which', side_effect=lambda name:'/installed/adb' if name=='adb' else None):
            self.assertEqual(tooie.default_downloads(), self.root / 'Downloads')
            self.assertEqual(tooie.default_adb(), '/installed/adb')

    def test_auto_adb_is_resolved_when_rendering_the_owned_session(self):
        artifact = {**self.profile['artifact'], 'sha256':'a'*64, 'build_id':'b'*40,
                    'apk':str(self.root/'game.apk'), 'report':str(self.root/'report.txt')}
        with patch.object(tooie, 'default_adb', return_value='/selected/sdk/adb.exe'):
            script = tooie.render_collector(self.profile, artifact, self.args.downloads, self.root/'session')
        assignment = next(node for node in ast.parse(script).body if isinstance(node,ast.Assign)
                          and isinstance(node.targets[0],ast.Name) and node.targets[0].id=='ADB')
        self.assertEqual(ast.literal_eval(assignment.value), '/selected/sdk/adb.exe')

    def test_artifact_report_bytes_identity_and_receipt_binding(self):
        apk = self.root / 'game.apk'; apk.write_bytes(b'host-fixture-no-executable')
        report = self.root / 'report.txt'
        fields = dict(self.profile['report_fields'])
        fields.update({'APK': str(apk), 'APK SHA-256': tooie.sha256(apk), 'ELF Build ID': 'b' * 40})
        report.write_text(self.profile['report_header'] + '\n' + '\n'.join(k + ': ' + v for k, v in fields.items()))
        receipt = tooie.artifact_from_files(apk, report, self.profile)
        receipt_path = self.root / 'receipt.json'; receipt_path.write_text(json.dumps(receipt))
        self.args.artifact = receipt_path
        self.assertEqual(tooie.select_artifact(self.args, self.profile), receipt)
        apk.write_bytes(b'changed')
        with self.assertRaisesRegex(RuntimeError, 'bytes differ'):
            tooie.select_artifact(self.args, self.profile)
        apk.write_bytes(b'host-fixture-no-executable')
        receipt['source'] = '0' * 40; receipt_path.write_text(json.dumps(receipt))
        with self.assertRaisesRegex(RuntimeError, 'source/profile'):
            tooie.select_artifact(self.args, self.profile)

    def test_existing_verified_apk_does_not_compile_or_overwrite(self):
        args = tooie.parser().parse_args(['build'])
        args.downloads = self.args.downloads
        (args.downloads / self.profile['artifact']['apk']).write_bytes(b'fixture')
        with patch.object(tooie, 'select_artifact', return_value={'ready': True}), \
             patch.object(tooie, 'cache_configuration', side_effect=AssertionError('unnecessary rebuild')):
            self.assertEqual(tooie.build(args, self.profile), {'ready': True})

    def test_unbuilt_revision_cannot_fall_back_to_an_old_apk(self):
        self.assertIsNone(self.profile['artifact']['sha256'])
        with self.assertRaisesRegex(RuntimeError, 'verified PC build first'):
            tooie.select_artifact(self.args, self.profile)
        apk = self.root / 'built.apk'; apk.write_bytes(b'host-only-artifact-fixture')
        report = self.root / 'built-report.txt'
        fields = dict(self.profile['report_fields'])
        fields.update({'APK': str(apk), 'APK SHA-256': tooie.sha256(apk), 'ELF Build ID': 'b' * 40})
        report.write_text(self.profile['report_header'] + '\n' + '\n'.join(k + ': ' + v for k, v in fields.items()))
        receipt = tooie.artifact_from_files(apk, report, self.profile)
        tooie.json_new(tooie.default_receipt(self.args, self.profile), receipt)
        self.assertEqual(tooie.select_artifact(self.args, self.profile), receipt)
        apk.write_bytes(b'changed-after-build')
        with self.assertRaisesRegex(RuntimeError, 'bytes differ'):
            tooie.select_artifact(self.args, self.profile)

    def test_mod_profile_retains_gameplay_and_production_wait_gates(self):
        self.assertFalse(self.profile['demo_only'])
        engineering = self.profile['source_manifest']['engineering_entries']
        mods = self.profile['source_manifest']['mod_entries']
        self.assertEqual(len(engineering), 175)
        self.assertEqual(mods, [])
        entries = {x['path']: x for x in engineering}
        self.assertEqual(entries['lib/rt64/src/render/rt64_transform_processor.cpp']['sha'],
                         '7223ebab3e3d5892b784d34ec08dba0af3943cf2')
        self.assertIn('tests/transform_upload.cpp', entries)
        self.assertIn('tools/test_transform_upload.py', entries)
        self.assertIn('src/mod_runtime.h', entries)
        self.assertIn('examples/mods/tooie_mod_check/mod.c', entries)
        self.assertIn('src/aiming_controls.cpp', entries)
        self.assertIn('lib/N64ModernRuntime/librecomp/include/librecomp/zip_read.hpp', entries)
        self.assertFalse({x['path'] for x in engineering} & {x['path'] for x in mods})
        builder = tooie.render_builder(self.profile)
        self.assertIn('python3 tools/test_mod_runtime.py', builder)
        self.assertIn('python3 tools/test_aiming_controls.py', builder)
        self.assertIn('--compression-base-root "$COMPRESSION_BASELINE"', builder)
        self.assertIn('pkg-config --exists libzstd', builder)
        self.assertIn('720 actual generated aiming/first-person cases', builder)
        self.assertIn('Example mod SHA-256: $MOD_SHA', builder)
        self.assertIn('python3 tools/test_transform_upload.py', builder)
        self.assertIn('exact-parent fails the independent byte check and grown-source ASan control', builder)
        self.assertIn('bash tools/test_demo_pacing.sh', builder)
        self.assertIn('python3 tools/test_demo_render_cadence.py', builder)
        self.assertIn('python3 tools/test_match_candidate_storage.py', builder)
        self.assertIn('MATCH_OPTIONS=(--scalar)', builder)
        self.assertIn('MATCH_OPTIONS=(--require-simd)', builder)
        self.assertIn('python3 tools/test_launcher_artwork.py', builder)
        self.assertIn('menu-recompiled.png', builder)

    def test_texture_aiming_build_and_collector_require_the_new_features(self):
        self.assertEqual(self.profile['identity']['commit'], 'dcba2d96d8155ba3d8c93460345eeb9c0d23ef8f')
        builder = tooie.render_builder(self.profile)
        artifact = {**self.profile['artifact'], 'sha256':'a'*64, 'build_id':'b'*40,
                    'apk':str(self.root/'game.apk'), 'report':str(self.root/'report.txt')}
        collector = tooie.render_collector(self.profile, artifact, self.args.downloads, self.root/'session')
        for marker in ('Aiming Invert', 'aiming_invert', 'tooie_aiming_invert',
                       'A Zstandard archive entry failed decoding, size validation or its checksum.',
                       'The texture pack has no readable rt64.json database.'):
            self.assertIn(repr(marker.encode()), builder)
            self.assertIn(repr(marker.encode()), collector)
        self.assertIn("'src/aiming_controls.cpp'", builder)
        self.assertIn("'RecompiledFuncs/funcs_39.c'", builder)
        self.assertIn('default OFF', self.profile['report_fields']['Aiming Invert'])
        self.assertIn('no GPU rendering claim', self.profile['report_fields']['Texture host validation'])
        self.assertIn('actual ARM64 begin-frame source-rate command', builder)
        audit = builder.split("<<'VERIFY_GAMEPLAY_CONFIGURATION'\n", 1)[1].split('\nVERIFY_GAMEPLAY_CONFIGURATION', 1)[0]
        ast.parse(audit)
        self.assertIn('tooie::demo::renderInterval(', audit)
        self.assertIn("'tooie_demo_pacing'", builder)
        report = builder.split(self.profile['report_header']+'\n', 1)[1].split('\nEOF\n', 1)[0]
        for key, value in self.profile['report_fields'].items():
            if key not in ('Source commit', 'Source branch', 'Source parent'):
                self.assertIn(key + ': ' + value + '\n', report+'\n')

    def test_cache_fingerprint_stable_then_invalidated_by_dependency(self):
        dependency = self.root / 'deps'; dependency.mkdir()
        (dependency / 'library').write_bytes(b'old')
        config = {'flags': 'trace=1', 'abi': 'arm64-v8a'}
        first, _ = tooie.fingerprint([dependency], config)
        same, _ = tooie.fingerprint([dependency], config)
        self.assertEqual(first, same)
        (dependency / 'library').write_bytes(b'new')
        second, _ = tooie.fingerprint([dependency], config)
        self.assertNotEqual(first, second)
        third, _ = tooie.fingerprint([dependency], {**config, 'flags': 'trace=0'})
        self.assertNotEqual(second, third)

    def test_build_and_device_locks_are_exclusive(self):
        with tooie.exclusive_lock(self.args.state, 'device'):
            with self.assertRaisesRegex(RuntimeError, 'holds'):
                with tooie.exclusive_lock(self.args.state, 'device'):
                    self.fail('concurrent test allowed')
        with tooie.exclusive_lock(self.args.state, 'device'):
            pass

    def test_compact_upload_preserves_fresh_bytes_notes_and_original_archive(self):
        archive = self.evidence()
        before = archive.read_bytes()
        handoff, summary = tooie.package_evidence(archive, self.profile)
        self.assertEqual(summary['runtime_counts'], {'gfx-submit': 1, 'gfx-dispatch': 1})
        self.assertEqual(len(summary['windows']), 1)
        self.assertEqual(summary['user_markers'][0]['description'], 'NPC stretches')
        with zipfile.ZipFile(handoff) as bundle:
            prefix = tooie.archive_prefix(archive)
            with zipfile.ZipFile(archive) as original:
                self.assertEqual(bundle.read(prefix + '-NewLog.txt'), original.read(prefix + '-NewLog.txt'))
                self.assertEqual(bundle.read(prefix + '-User-Markers.txt'), original.read(prefix + '-User-Markers.txt'))
            self.assertNotIn(prefix + '-FullLog.txt', bundle.namelist())
            self.assertNotIn(archive.name, bundle.namelist())
            manifest = json.loads(bundle.read('share-manifest.json'))
            self.assertEqual(manifest['original_archive_sha256'], tooie.sha256(archive))
            self.assertIsNone(bundle.testzip())
        self.assertTrue(summary['compact_upload']['complete_delta_included'])
        self.assertEqual(summary['upload_files'], [handoff.name])
        self.assertEqual(archive.read_bytes(), before)

    def test_no_missing_delta_or_unknown_pid_is_invented(self):
        archive = self.evidence(original='None')
        result = tooie.summarize_archive(archive)
        self.assertIsNone(result['original_pid'])
        self.assertEqual(result['runtime_counts'], {})

    def test_duplicate_and_path_members_rejected(self):
        for member in ('../other.txt', 'sub/other.txt'):
            archive = self.root / (tooie.unique_id() + '-Evidence.zip')
            with zipfile.ZipFile(archive, 'x') as bundle:
                bundle.writestr(member, 'data')
            with self.assertRaisesRegex(RuntimeError, 'member path'):
                tooie.summarize_archive(archive)

    def test_accumulated_logs_stay_local_without_upload_parts_or_index(self):
        archive = self.evidence(extra={'FullLog': os.urandom(1300000), 'Baseline': os.urandom(1300000)})
        before = tooie.sha256(archive)
        prefix = tooie.archive_prefix(archive)
        with patch.object(tooie, 'UPLOAD_LIMIT', 1200000):
            handoff, result = tooie.package_evidence(archive, self.profile)
            self.assertEqual(result['upload_files'], [handoff.name])
            self.assertLess(handoff.stat().st_size, 1200000)
            with zipfile.ZipFile(handoff) as bundle:
                manifest = json.loads(bundle.read('share-manifest.json'))
                local = {s['name'] for s in manifest['sources'] if s['mode'] == 'local-only'}
                self.assertEqual(local, {prefix + '-Baseline.txt', prefix + '-FullLog.txt'})
                self.assertIn(prefix + '-NewLog.txt', bundle.namelist())
        self.assertEqual(tooie.sha256(archive), before)
        self.assertFalse(list(self.args.downloads.glob('*-Evidence-Part*.zip')))
        self.assertFalse(list(self.args.downloads.glob('*-Upload-Index.txt')))

    def test_oversized_fresh_log_has_named_hashed_byte_excerpts(self):
        data = os.urandom(400000)
        archive = self.evidence(extra={'NewLog': data})
        prefix = tooie.archive_prefix(archive)
        with patch.object(tooie, 'UPLOAD_LIMIT', 120000):
            output, summary = tooie.package_evidence(archive, self.profile)
        self.assertLessEqual(output.stat().st_size, 120000)
        self.assertFalse(summary['compact_upload']['complete_delta_included'])
        with zipfile.ZipFile(output) as bundle:
            self.assertNotIn(prefix + '-NewLog.txt', bundle.namelist())
            entry = next(s for s in json.loads(bundle.read('share-manifest.json'))['sources']
                         if s['name'] == prefix + '-NewLog.txt')
            self.assertEqual(entry['mode'], 'excerpt')
            self.assertEqual(entry['sha256'], tooie.hashlib.sha256(data).hexdigest())
            self.assertEqual(len(entry['stored_files']), 2)
            for part in entry['stored_files']:
                raw = bundle.read(part['name'])
                self.assertEqual(raw, data[part['offset']:part['offset'] + part['length']])
                self.assertEqual(tooie.hashlib.sha256(raw).hexdigest(), part['sha256'])

    def test_size_budget_prefers_mod_and_marker_images_and_declares_omissions(self):
        images = {'Mod-Check-Ready-blue.png': os.urandom(12000),
                  'Screenshot-Marker-001.png': os.urandom(12000),
                  'Screenshot-Gameplay-01-01.png': os.urandom(60000)}
        archive = self.evidence()
        prefix = tooie.archive_prefix(archive)
        with zipfile.ZipFile(archive, 'a') as bundle:
            for suffix, data in images.items(): bundle.writestr(prefix + '-' + suffix, data)
        with patch.object(tooie, 'UPLOAD_LIMIT', 65000):
            output, summary = tooie.package_evidence(archive, self.profile)
        self.assertLessEqual(output.stat().st_size, 65000)
        with zipfile.ZipFile(output) as bundle:
            for suffix in list(images)[:2]:
                self.assertEqual(bundle.read(prefix + '-' + suffix), images[suffix])
            omitted = next(s for s in json.loads(bundle.read('share-manifest.json'))['sources']
                           if s['name'].endswith('Screenshot-Gameplay-01-01.png'))
            self.assertEqual(omitted['mode'], 'local-only')
            self.assertIn('size budget', omitted['reason'])

    def test_missing_delta_stays_missing_with_explicit_fulllog_fallback(self):
        archive = self.evidence()
        prefix = tooie.archive_prefix(archive)
        with zipfile.ZipFile(archive) as bundle:
            values = {n: bundle.read(n) for n in bundle.namelist() if not n.endswith('-NewLog.txt')}
        with zipfile.ZipFile(archive, 'w') as bundle:
            for n, data in values.items(): bundle.writestr(n, data)
        output, summary = tooie.package_evidence(archive, self.profile)
        self.assertNotIn('delta_sha256', summary)
        self.assertFalse(summary['compact_upload']['complete_delta_included'])
        with zipfile.ZipFile(output) as bundle:
            self.assertNotIn(prefix + '-NewLog.txt', bundle.namelist())
            self.assertEqual(bundle.read(prefix + '-FullLog.txt'), values[prefix + '-FullLog.txt'])

    def test_package_latest_selects_raw_archive_without_any_device_operation(self):
        first = self.evidence(); second = self.evidence()
        os.utime(first, ns=(1, 1)); os.utime(second, ns=(2, 2))
        misleading = second.with_name('Tooie-newest-Share-file.zip')
        misleading.write_bytes(b'not a raw capture')
        self.assertEqual(tooie.latest_evidence(self.args.downloads), second)
        with patch.object(tooie, 'default_downloads', return_value=self.args.downloads), \
             patch.object(tooie.subprocess, 'Popen', side_effect=AssertionError('No device/subprocess allowed')):
            self.assertEqual(tooie.main(['--downloads', str(self.args.downloads), 'package', '--latest']), 0)
        self.assertEqual(len(list(self.args.downloads.glob('*-Share-*.zip'))), 2)  # Includes the misleading fixture.

    def test_corrupt_excluded_history_is_detected_before_share_export(self):
        archive = self.evidence(extra={'Baseline': b'old baseline'})
        with zipfile.ZipFile(archive) as bundle:
            info = next(i for i in bundle.infolist() if i.filename.endswith('-Baseline.txt'))
        raw = bytearray(archive.read_bytes())
        raw[info.header_offset + 30 + len(info.filename.encode())] ^= 1
        archive.write_bytes(raw)
        with self.assertRaises(zipfile.BadZipFile):
            tooie.package_evidence(archive, self.profile)
        self.assertFalse(list(self.args.downloads.glob('*-Share-*.zip')))

    def fake_capture(self, codes, uncertain_at=None):
        calls = []
        def popen(argv):
            script = Path(argv[-1]); session = script.parent
            # Stand-in for the collector: no subprocess/device command occurs.
            archive = self.evidence(uncertain=len(calls) == uncertain_at)
            prefix = archive.with_name(tooie.archive_prefix(archive))
            marker = session / 'attempt.started'
            marker.mkdir()
            (marker / 'evidence-prefix.txt').write_text(str(prefix) + '\n')
            calls.append(session)
            code = codes[len(calls)-1]
            class Process:
                def wait(self):
                    return code
            return Process()
        return calls, popen

    def test_two_sessions_get_distinct_attempts_baselines_and_handoffs(self):
        self.args.sessions = 2
        calls, popen = self.fake_capture([0, 0])
        with patch.object(tooie.sys, 'stdin', Terminal()), \
             patch.object(tooie, 'select_artifact', return_value=self.profile['artifact']), \
             patch.object(tooie.subprocess, 'Popen', side_effect=popen):
            self.assertEqual(tooie.capture(self.args, self.profile), 0)
        self.assertEqual(len(set(calls)), 2)
        self.assertEqual(len(list(self.args.downloads.glob('*-Share-*.zip'))), 2)
        prefixes = [(p / 'attempt.started/evidence-prefix.txt').read_text() for p in calls]
        self.assertEqual(len(set(prefixes)), 2)

    def test_uncertain_or_failed_session_stops_batch_without_retry(self):
        self.args.sessions = 3
        for codes, uncertain in (([1], None), ([0], 0)):
            calls, popen = self.fake_capture(codes, uncertain)
            with patch.object(tooie.sys, 'stdin', Terminal()), \
                 patch.object(tooie, 'select_artifact', return_value=self.profile['artifact']), \
                 patch.object(tooie.subprocess, 'Popen', side_effect=popen):
                self.assertNotEqual(tooie.capture(self.args, self.profile), 0)
            self.assertEqual(len(calls), 1)
            self.assertTrue((calls[0] / 'result.json').is_file())

    def local_repository(self):
        repo = self.root / 'upstream'; repo.mkdir()
        environment = {**os.environ, 'GIT_AUTHOR_NAME': 'Fixture', 'GIT_AUTHOR_EMAIL': 'fixture@example.invalid',
                       'GIT_COMMITTER_NAME': 'Fixture', 'GIT_COMMITTER_EMAIL': 'fixture@example.invalid'}
        def command(*args):
            return subprocess.check_output(['git', '-C', str(repo), *args], env=environment).decode().strip()
        command('init', '-q')
        (repo / 'tracked.txt').write_text('first')
        command('add', '.'); command('commit', '-qm', 'first')
        first = command('rev-parse', 'HEAD')
        (repo / 'tracked.txt').write_text('second')
        command('commit', '-qam', 'second')
        second = command('rev-parse', 'HEAD')
        return repo, first, second, command

    def test_actual_local_git_source_cache_reuses_checkout_and_preserves_dirty_files(self):
        repo, first, second, command = self.local_repository()
        workspace = self.root / 'owned-cache'; workspace.mkdir()
        profile = copy.deepcopy(self.profile)
        profile['identity']['commit'] = first
        profile['identity']['tree'] = command('rev-parse', first + '^{tree}')
        with patch.object(tooie, 'REPO', str(repo)):
            source = tooie.prepare_source(workspace, profile)
            cache = workspace / 'build-android'; cache.mkdir(); (cache / 'retained.o').write_bytes(b'cached')
            profile['identity']['commit'] = second
            profile['identity']['tree'] = command('rev-parse', second + '^{tree}')
            self.assertEqual(tooie.prepare_source(workspace, profile), source)
            self.assertEqual((source / 'tracked.txt').read_text(), 'second')
            self.assertEqual((cache / 'retained.o').read_bytes(), b'cached')
            (source / 'tracked.txt').write_text('personal change')
            with self.assertRaisesRegex(RuntimeError, 'Tracked source-cache changes'):
                tooie.prepare_source(workspace, profile)
            self.assertEqual((source / 'tracked.txt').read_text(), 'personal change')

    def test_bootstrap_installs_once_reuses_wrapper_and_preserves_conflicts(self):
        repo, _, _, command = self.local_repository()
        folder = repo / 'tools/android-workflow'; folder.mkdir(parents=True)
        (folder / 'tooie.py').write_text('# local bootstrap fixture\n')
        command('add', '.'); command('commit', '-qm', 'tool fixture')
        commit = command('rev-parse', 'HEAD')
        command('branch', tooie.TOOL_BRANCH)
        source = (ROOT / 'bootstrap.py.in').read_text().replace('@@RUNNER_COMMIT@@', commit)
        namespace = {'__name__': 'bootstrap_fixture'}
        exec(compile(source, '<bootstrap-fixture>', 'exec'), namespace)
        namespace['REPOSITORY'] = str(repo)
        home = self.root / 'fake-home'; home.mkdir()
        with patch.object(Path, 'home', return_value=home), patch.object(sys, 'argv', ['setup.py']):
            namespace['main']()
            wrapper = home / 'projects/tooie-tools/tooie'
            first = wrapper.read_bytes()
            namespace['main']()
            self.assertEqual(wrapper.read_bytes(), first)
            checkout = home / 'projects/tooie-tools/workflow-runner/repository'
            self.assertEqual(tooie.git(checkout, 'rev-parse', 'HEAD'), commit)
            wrapper.write_text('other content\n')
            with self.assertRaisesRegex(RuntimeError, 'other content'):
                namespace['main']()
            self.assertEqual(wrapper.read_text(), 'other content\n')

    def test_new_profile_cannot_relabel_an_old_capture(self):
        archive = self.evidence()
        later = copy.deepcopy(self.profile)
        later['identity']['commit'] = 'f' * 40
        handoff, summary = tooie.package_evidence(archive, later)
        self.assertEqual(summary['source_commit'], self.profile['identity']['commit'])
        with zipfile.ZipFile(handoff) as bundle:
            text = bundle.read('README.txt').decode()
            self.assertIn('Captured source: ' + self.profile['identity']['commit'], text)
            self.assertIn('Workflow profile at packaging: ' + 'f' * 40, text)


    def test_mod_receipt_verifies_source_and_package_before_copy(self):
        profile = copy.deepcopy(self.profile)
        archive = self.root / profile['artifact']['mod']
        report = self.root / profile['artifact']['mod_report']
        apk_report = self.root / 'apk-report.txt'
        with zipfile.ZipFile(archive, 'w') as z:
            z.writestr('mod.json', json.dumps({'id':'tooie_mod_check','game_id':'bt'}))
            z.writestr('mod_syms.bin', b'N64RSYMS-host-fixture')
            z.writestr('mod_binary.bin', b'not executable: metadata validation fixture')
        digest = tooie.sha256(archive)
        value = {'source_commit':profile['identity']['commit'], 'archive_sha256':digest,
                 'source_files':profile['mod_check']['source_files']}
        report.write_text(json.dumps(value))
        fields = profile['report_fields'] | {'Example mod SHA-256':digest}
        apk_report.write_text(profile['report_header']+'\n'+'\n'.join(k+': '+v for k,v in fields.items()))
        artifact = {'apk':str(self.root/'game.apk'), 'report':str(apk_report)}
        result = tooie.mod_artifact(profile, artifact)
        self.assertEqual(result, {'path':str(archive),'sha256':digest})
        report.write_text(json.dumps(value | {'source_commit':'0'*40}))
        with self.assertRaisesRegex(RuntimeError, 'source and APK build report'):
            tooie.mod_artifact(profile, artifact)
        report.write_text(json.dumps(value))
        archive.write_bytes(b'changed after building')
        with self.assertRaisesRegex(RuntimeError, 'source and APK build report'):
            tooie.mod_artifact(profile, artifact)

    def test_mod_phase_uses_same_terminal_and_explicit_gameplay(self):
        args = tooie.parser().parse_args(['test','--mod-check','gold'])
        self.assertEqual(args.mod_check,'gold')
        artifact = {'apk':str(self.root/'game.apk'),'report':str(self.root/'report.txt'),
                    'sha256':'a'*64,'build_id':'b'*40}
        mod = {'path':str(self.root/'check.nrm'),'sha256':'c'*64}
        text = tooie.render_collector(self.profile,artifact,self.args.downloads,self.root/'session',mod_check='gold',mod=mod)
        namespace={'__name__':'fixture'};exec(text,namespace)
        self.assertTrue(namespace['GAMEPLAY_ONLY'])
        self.assertEqual(namespace['MOD_CHECK'],'gold')
        self.assertEqual(namespace['MOD_FILE'],Path(mod['path']))
        self.assertIn('session.nrm',namespace['MOD_REMOTE'])

    def test_missing_removal_ui_stops_before_artifact_or_device_access(self):
        args = tooie.parser().parse_args(['test', '--mod-check', 'removed'])
        with patch.object(tooie, 'select_artifact', side_effect=AssertionError('No artifact/device work')), \
             patch.object(tooie.subprocess, 'Popen', side_effect=AssertionError('No install/launch')):
            with self.assertRaisesRegex(RuntimeError, 'no Remove option'):
                tooie.capture(args, self.profile)
        with self.assertRaisesRegex(RuntimeError, 'no Remove option'):
            tooie.render_collector(self.profile, {}, self.args.downloads, self.root/'session', mod_check='removed')

    def test_mod_setup_copies_only_gold_and_rejects_changed_bytes(self):
        artifact = {'apk':str(self.root/'game.apk'),'report':str(self.root/'report.txt'),
                    'sha256':'a'*64,'build_id':'b'*40}
        path=self.root/'check.nrm';path.write_bytes(b'controlled copy fixture')
        mod={'path':str(path),'sha256':tooie.sha256(path)}
        text=tooie.render_collector(self.profile,artifact,self.args.downloads,self.root/'session',mod_check='gold',mod=mod)
        namespace={'__name__':'fixture'};exec(text,namespace)
        namespace['prefix']=self.root/'evidence'
        calls=[]
        def adb(*args,**kwargs):
            calls.append(args)
            return subprocess.CompletedProcess([],0,('' if args[0]=='push' else mod['sha256']+'  file').encode(),b'')
        namespace['adb']=adb
        namespace['command']=lambda *a,**k:subprocess.CompletedProcess([],0,b'C:\\Downloads\\check.nrm',b'')
        namespace['mod_setup']()
        self.assertEqual([c[0] for c in calls],['push','shell'])
        for phase in ('blue','persist','disabled'):
            calls.clear();namespace['MOD_CHECK']=phase;namespace['prefix']=self.root/('evidence-'+phase);namespace['mod_setup']()
            self.assertEqual(calls,[])
        calls.clear(); namespace['MOD_CHECK']='removed'; namespace['prefix']=self.root/'unsupported-removal'
        with self.assertRaisesRegex(RuntimeError, 'no Remove option'):
            namespace['mod_setup']()
        self.assertEqual(calls, [])
        self.assertFalse((self.root/'unsupported-removal-Mod-Check-Plan.txt').exists())
        calls.clear();namespace['MOD_CHECK']='gold';path.write_bytes(b'changed')
        with self.assertRaisesRegex(RuntimeError,'changed after workflow validation'):
            namespace['mod_setup']()
        self.assertEqual(calls,[])
        path.write_bytes(b'controlled copy fixture')
        namespace['prefix']=self.root/'copy-failed'
        def failed_push(*args,**kwargs):
            calls.append(args)
            return subprocess.CompletedProcess([],1,b'',b'uncertain connection')
        namespace['adb']=failed_push
        with self.assertRaisesRegex(RuntimeError,'failed or was uncertain'):
            namespace['mod_setup']()
        self.assertEqual([c[0] for c in calls],['push'])

    def test_presentation_control_preserves_artifact_and_default_mode(self):
        artifact = {**self.profile['artifact'], 'sha256':'a'*64, 'build_id':'b'*40,
                    'apk':str(self.root/'game.apk'), 'report':str(self.root/'report.txt')}
        args = tooie.parser().parse_args(['test', '--presentation-hz', '60'])
        self.assertEqual(args.presentation_hz, 60)
        self.assertIsNone(tooie.parser().parse_args(['test']).presentation_hz)
        normal = tooie.render_collector(self.profile, artifact, self.args.downloads, self.root/'normal')
        control = tooie.render_collector(self.profile, artifact, self.args.downloads, self.root/'control', 60)
        def assignments(source):
            return {n.targets[0].id: ast.dump(n.value) for n in ast.parse(source).body
                    if isinstance(n,ast.Assign) and isinstance(n.targets[0],ast.Name)}
        one,two = assignments(normal),assignments(control)
        self.assertEqual(one['PRESENTATION_TEST_HZ'], 'Constant(value=0)')
        self.assertEqual(two['PRESENTATION_TEST_HZ'], 'Constant(value=60)')
        self.assertEqual({k:v for k,v in one.items() if k not in ('MARKER','PRESENTATION_TEST_HZ')},
                         {k:v for k,v in two.items() if k not in ('MARKER','PRESENTATION_TEST_HZ')})
        with self.assertRaises(RuntimeError):
            tooie.render_collector(self.profile, artifact, self.args.downloads, self.root/'bad', 30)


    def test_gameplay_only_is_an_explicit_session_override(self):
        artifact = {**self.profile['artifact'], 'sha256':'a'*64, 'build_id':'b'*40,
                    'apk':str(self.root/'game.apk'), 'report':str(self.root/'report.txt')}
        args = tooie.parser().parse_args(['test', '--gameplay-only'])
        self.assertTrue(args.gameplay_only)
        self.assertFalse(tooie.parser().parse_args(['test']).gameplay_only)
        normal = tooie.render_collector(self.profile, artifact, self.args.downloads, self.root/'session')
        direct = tooie.render_collector(self.profile, artifact, self.args.downloads, self.root/'session', gameplay_only=True)
        def assignments(source):
            return {n.targets[0].id: ast.dump(n.value) for n in ast.parse(source).body
                    if isinstance(n,ast.Assign) and isinstance(n.targets[0],ast.Name)}
        one,two = assignments(normal),assignments(direct)
        self.assertEqual(two.pop('GAMEPLAY_ONLY'), 'Constant(value=True)')
        self.assertEqual(one.pop('GAMEPLAY_ONLY'), 'Constant(value=False)')
        self.assertEqual(one,two)


class PrivateBackup(unittest.TestCase):
    """Real binary archive consumers with a stubbed, read-only ADB transport."""
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='tooie-private-backup-tests-')
        self.root = Path(self.temp.name)
        self.directory = self.root / 'private-backups'
        self.device = {'serial': 'fixture-odin', 'package': 'io.github.banjotooierecompiled'}
        self.module = tooie.private_backup
        self.data = {'files/data/save.bin': b'\0\xff\x80\r\n\0private save\n',
                     'files/data/texture pack.rtz': b'PK\x03\x04binary archive\0',
                     'shared_prefs/settings.xml': b'<settings aim="1"/>'}
        self.selected = ['files', 'shared_prefs']
        self.payload = self.tar()
        self.hash_calls = 0
        self.commands = []
        self.redirect = contextlib.redirect_stdout(io.StringIO())
        self.redirect.__enter__()

    def tearDown(self):
        self.redirect.__exit__(None, None, None)
        self.temp.cleanup()

    def tar(self, *, extra=None):
        result = io.BytesIO()
        with tarfile.open(fileobj=result, mode='w') as archive:
            for name in self.selected:
                item = tarfile.TarInfo(name)
                item.type = tarfile.DIRTYPE
                archive.addfile(item)
            for name, content in self.data.items():
                item = tarfile.TarInfo(name)
                item.size = len(content)
                archive.addfile(item, io.BytesIO(content))
            if extra:
                archive.addfile(extra)
        return result.getvalue()

    def fake_adb(self, args, **kwargs):
        self.assertEqual(args[:3], ['fixture-adb', '-s', 'fixture-odin'])
        self.assertEqual(kwargs['stdin'], subprocess.DEVNULL)
        self.assertIn('timeout', kwargs)
        command = args[3:]
        self.commands.append(command)
        if command == ['get-state']:
            value, code = b'device\r\n', 0
        elif command == ['shell', '-T', 'getprop ro.product.model']:
            value, code = b'Odin2 Portal\r\n', 0
        elif command == ['shell', '-T', 'getprop ro.build.version.sdk']:
            value, code = b'33\n', 0
        elif command == ['shell', '-T', 'pidof ' + self.device['package']]:
            value, code = b'', 1
        elif command == self.module.app_shell(self.device['package'],
                'find ' + ' '.join(self.selected) + ' -type f -exec sha256sum {} +'):
            self.hash_calls += 1
            value = ''.join(hashlib.sha256(content).hexdigest() + '  ' + name + '\n'
                            for name, content in self.data.items()).encode()
            code = 0
        elif command[:2] == ['shell', '-T'] and command[2].startswith('run-as ') and 'for d in ' in command[2]:
            value, code = ('\n'.join(self.selected) + '\n').encode(), 0
        elif command == self.module.app_exec(self.device['package'], self.module.BINARY_PROBE_SCRIPT):
            kwargs['stdout'].write(self.module.BINARY_PROBE)
            value, code = None, 0
        elif command[:5] == ['exec-out', 'run-as', self.device['package'], 'sh', '-c'] and command[-1].startswith('tar -cf - '):
            nonce = re.search(r'TOOIE_PRIVATE_TAR_EXIT_([A-Za-z0-9]+):', command[-1])[1]
            kwargs['stdout'].write(self.payload + b'\nTOOIE_PRIVATE_TAR_EXIT_' + nonce.encode() + b':0\n')
            value, code = None, 0
        else:
            self.fail('Unexpected operation: ' + repr(command))
        return subprocess.CompletedProcess(args, code, value, b'')

    def backup(self):
        with patch.object(subprocess, 'run', side_effect=self.fake_adb):
            return self.module.create_backup('fixture-adb', self.device, self.directory)

    def test_binary_copy_manifest_hashes_permissions_and_evidence_guard(self):
        output = self.backup()
        self.assertEqual(output.stat().st_mode & 0o777, 0o600)
        self.assertEqual(self.directory.stat().st_mode & 0o777, 0o700)
        self.assertEqual(self.hash_calls, 2)
        with zipfile.ZipFile(output) as bundle:
            self.assertEqual(bundle.read('app-data.tar'), self.payload)
            manifest = json.loads(bundle.read(self.module.MANIFEST))
        self.assertEqual(manifest['kind'], self.module.KIND)
        self.assertFalse(manifest['share_allowed'])
        self.assertFalse(manifest['device_modified'])
        self.assertFalse(manifest['restoration_tested'])
        self.assertEqual(manifest['tar_sha256'], hashlib.sha256(self.payload).hexdigest())
        self.assertEqual(manifest['device']['api'], 33)
        self.assertEqual(set(manifest['files']), set(self.data))
        for name, content in self.data.items():
            self.assertEqual(manifest['files'][name]['sha256'], hashlib.sha256(content).hexdigest())
        with self.assertRaisesRegex(RuntimeError, 'cannot be packaged as evidence'):
            tooie.package_evidence(output, {'identity': {'commit': '0' * 40}})
        with self.assertRaisesRegex(RuntimeError, 'No original evidence'):
            tooie.latest_evidence(self.directory)
        self.assertEqual(list(self.directory.iterdir()), [output])

    def test_running_unknown_and_failed_pid_checks_never_copy(self):
        for code, value, error, message in [(0, b'112 113\r\n', b'', 'App is running'),
                (0, b'', b'', 'inconclusive'), (1, b'', b'denied', 'inconclusive'),
                (2, b'', b'', 'inconclusive')]:
            with self.subTest(code=code, value=value, error=error):
                def fake(args, **kwargs):
                    if args[-1].startswith('pidof '):
                        return subprocess.CompletedProcess(args, code, value, error)
                    return self.fake_adb(args, **kwargs)
                with patch.object(subprocess, 'run', side_effect=fake), self.assertRaisesRegex(RuntimeError, message):
                    self.module.create_backup('fixture-adb', self.device, self.directory)
        self.assertFalse(any('tar -cf' in str(command) for command in self.commands))
        self.assertFalse(list(self.directory.glob('*.zip')))

    def test_actual_binary_subprocess_transport_roundtrip(self):
        tar = self.root / 'synthetic-data.tar'
        tar.write_bytes(self.payload)
        hashes = ''.join(hashlib.sha256(content).hexdigest() + '  ' + name + '\n'
                         for name, content in self.data.items()).encode()
        script = self.root / 'synthetic-adb'
        script.write_text('#!' + sys.executable + '\n'
            'import pathlib,re,sys\n'
            'a=sys.argv[1:]\n'
            'assert a[:2] == ["-s", "fixture-odin"]\n'
            'c=a[2:]\n'
            'if c == ["get-state"]: b=b"device\\r\\n"\n'
            'elif c[-1] == "getprop ro.product.model": b=b"Odin2 Portal\\r\\n"\n'
            'elif c[-1] == "getprop ro.build.version.sdk": b=b"33\\n"\n'
            'elif c[-1].startswith("pidof "): sys.exit(1)\n'
            'elif "for d in " in c[-1]: b=b"files\\nshared_prefs\\n"\n'
            'elif "sha256sum" in c[-1]: b=' + repr(hashes) + '\n'
            'elif c[-1].startswith("printf "): b=bytes((0,1,10,13,26,127,128,255))\n'
            'elif c[-1].startswith("tar -cf - "):\n'
            ' n=re.search(r"TOOIE_PRIVATE_TAR_EXIT_([A-Za-z0-9]+):",c[-1])[1]\n'
            ' b=pathlib.Path(' + repr(str(tar)) + ').read_bytes()+b"\\nTOOIE_PRIVATE_TAR_EXIT_"+n.encode()+b":0\\n"\n'
            'else: raise AssertionError(c)\n'
            'sys.stdout.buffer.write(b)\n')
        script.chmod(0o700)
        output = self.module.create_backup(script, self.device, self.directory)
        with zipfile.ZipFile(output) as archive:
            self.assertEqual(archive.read('app-data.tar'), self.payload)

    def test_existing_backup_is_never_overwritten_or_removed_on_collision(self):
        class Clock(dt.datetime):
            @classmethod
            def now(cls, tz=None):
                return cls(2026, 10, 23, 12, 0, 0, tzinfo=tz)
        with patch.object(self.module.dt, 'datetime', Clock), \
                patch.object(self.module.uuid, 'uuid4', return_value=types.SimpleNamespace(hex='fixturecollision')):
            original = self.backup()
            data = original.read_bytes()
            with self.assertRaises(FileExistsError):
                self.backup()
            self.assertEqual(original.read_bytes(), data)
            self.assertEqual(list(self.directory.iterdir()), [original])

    def test_failed_saved_zip_readback_does_not_export_a_verified_backup(self):
        original = self.module.digest
        def corrupted_readback(stream):
            return '0' * 64 if isinstance(stream, zipfile.ZipExtFile) else original(stream)
        with patch.object(self.module, 'digest', side_effect=corrupted_readback), \
                self.assertRaisesRegex(RuntimeError, 'readback differs'):
            self.backup()
        self.assertFalse(list(self.directory.iterdir()))

    def test_pid_and_transfer_timeouts_are_unknown_without_retry(self):
        for stage in ('pidof ', 'tar -cf - '):
            with self.subTest(stage=stage):
                failures = []
                def fake(args, **kwargs):
                    if stage in args[-1]:
                        failures.append(args)
                        raise subprocess.TimeoutExpired(args, kwargs['timeout'])
                    return self.fake_adb(args, **kwargs)
                with patch.object(subprocess, 'run', side_effect=fake), self.assertRaisesRegex(RuntimeError, 'timed out.*unknown'):
                    self.module.create_backup('fixture-adb', self.device, self.directory)
                self.assertEqual(len(failures), 1)
                self.assertFalse(list(self.directory.iterdir()))

    def test_remote_transfer_error_is_not_a_backup(self):
        def fake(args, **kwargs):
            if 'tar -cf - ' in args[-1]:
                kwargs['stdout'].write(self.payload)
                return subprocess.CompletedProcess(args, 1, None, b'private read failed')
            return self.fake_adb(args, **kwargs)
        with patch.object(subprocess, 'run', side_effect=fake), self.assertRaisesRegex(RuntimeError, 'Private app-data copy failed'):
            self.module.create_backup('fixture-adb', self.device, self.directory)
        self.assertFalse(list(self.directory.iterdir()))

    def test_changed_device_or_corrupt_copied_bytes_block_export(self):
        original = self.payload
        self.payload = original.replace(b'private save', b'changed save')
        with self.assertRaisesRegex(RuntimeError, 'copied bytes differ'):
            self.backup()
        self.payload = original
        self.hash_calls = 0
        def fake(args, **kwargs):
            result = self.fake_adb(args, **kwargs)
            if 'sha256sum' in args[-1] and self.hash_calls == 2:
                result.stdout = result.stdout.replace(hashlib.sha256(self.data['files/data/save.bin']).hexdigest().encode(), b'0' * 64)
            return result
        with patch.object(subprocess, 'run', side_effect=fake), self.assertRaisesRegex(RuntimeError, 'files changed'):
            self.module.create_backup('fixture-adb', self.device, self.directory)
        self.assertFalse(list(self.directory.iterdir()))

    def test_tar_rejects_traversal_duplicate_links_and_missing_end_marker(self):
        traversal = tarfile.TarInfo('files/../../outside')
        duplicate = tarfile.TarInfo('files/data/save.bin')
        link = tarfile.TarInfo('files/link')
        link.type, link.linkname = tarfile.SYMTYPE, '/outside'
        payloads = [self.tar(extra=item) for item in (traversal, duplicate, link)]
        # A cleanly ended last file with no zero records is still incomplete.
        end = 0
        with tarfile.open(fileobj=io.BytesIO(self.payload)) as archive:
            for member in archive:
                end = member.offset_data + ((member.size + 511) // 512) * 512
        payloads += [self.payload[:end], self.payload[:end + 512], self.payload[:-1], b'broken' * 512]
        for index, payload in enumerate(payloads):
            with self.subTest(index=index):
                self.payload = payload
                with self.assertRaises(RuntimeError):
                    self.backup()
                self.assertFalse(list(self.directory.iterdir()))

    def test_device_identity_and_root_checks_precede_copy(self):
        for label, output in [('getprop ro.product.model', b'Another device\n'),
                              ('getprop ro.build.version.sdk', b'unknown\n'),
                              ('for d in ', b'files\nfiles\n')]:
            with self.subTest(label=label):
                def fake(args, **kwargs):
                    result = self.fake_adb(args, **kwargs)
                    if label in args[-1]:
                        result.stdout = output
                    return result
                with patch.object(subprocess, 'run', side_effect=fake), self.assertRaises(RuntimeError):
                    self.module.create_backup('fixture-adb', self.device, self.directory)
                self.assertFalse(list(self.directory.iterdir()))

    def test_private_destination_rejects_git_and_open_permissions_before_adb(self):
        self.directory.mkdir(mode=0o755)
        with patch.object(subprocess, 'run') as transport, self.assertRaisesRegex(RuntimeError, 'mode 700'):
            self.module.create_backup('fixture-adb', self.device, self.directory)
        transport.assert_not_called()
        (self.root / '.git').mkdir()
        with patch.object(subprocess, 'run') as transport, self.assertRaisesRegex(RuntimeError, 'outside Git'):
            self.module.create_backup('fixture-adb', self.device, self.directory)
        transport.assert_not_called()

    def test_cli_uses_device_lock_private_home_and_no_artifact_or_installer(self):
        home = self.root / 'home'
        home.mkdir()
        with patch.object(Path, 'home', return_value=home), patch.object(tooie, 'default_adb', return_value='fixture-adb'), \
                patch.object(self.module, 'create_backup') as backup, patch.object(tooie, 'select_artifact') as artifact:
            self.assertEqual(tooie.main(['--tool-root', str(self.root / 'tools'), 'backup-debug-data']), 0)
        self.assertEqual(backup.call_args.args[0], 'fixture-adb')
        self.assertEqual(backup.call_args.args[2], home / '.local/share/tooie-recompiled/private-backups')
        self.assertTrue((self.root / 'tools/workflow/device.lock').exists())
        artifact.assert_not_called()

    def test_binary_probe_rejects_text_conversion_before_hashes_or_copy(self):
        def fake(args, **kwargs):
            if args[3:] == self.module.app_exec(self.device['package'], self.module.BINARY_PROBE_SCRIPT):
                kwargs['stdout'].write(self.module.BINARY_PROBE.replace(b'\n', b'\r\n'))
                return subprocess.CompletedProcess(args, 0, None, b'')
            return self.fake_adb(args, **kwargs)
        with patch.object(subprocess, 'run', side_effect=fake), self.assertRaisesRegex(RuntimeError, 'binary transport check differs'):
            self.module.create_backup('fixture-adb', self.device, self.directory)
        self.assertEqual(self.hash_calls, 0)
        self.assertFalse(list(self.directory.iterdir()))

    def test_real_probe_script_outputs_binary_bytes(self):
        result = subprocess.run(['/bin/sh', '-c', self.module.BINARY_PROBE_SCRIPT],
                                stdin=subprocess.DEVNULL, capture_output=True, timeout=10, check=True)
        self.assertEqual(result.stdout, self.module.BINARY_PROBE)
        self.assertEqual(result.stderr, b'')

    def test_raw_copy_rejects_missing_wrong_failed_or_followed_completion_records(self):
        for failure in ('missing', 'wrong', 'nonzero', 'followed'):
            with self.subTest(failure=failure):
                def fake(args, **kwargs):
                    if args[-1].startswith('tar -cf - '):
                        nonce = re.search(r'TOOIE_PRIVATE_TAR_EXIT_([A-Za-z0-9]+):', args[-1])[1]
                        token = nonce if failure != 'wrong' else 'anothercopy'
                        footer = b'\nTOOIE_PRIVATE_TAR_EXIT_' + token.encode() + (b':1\n' if failure == 'nonzero' else b':0\n')
                        if failure == 'missing':
                            footer = b''
                        elif failure == 'followed':
                            footer += b'extra bytes'
                        kwargs['stdout'].write(self.payload + footer)
                        # The client can report success even for remote exit 1.
                        return subprocess.CompletedProcess(args, 0, None, b'')
                    return self.fake_adb(args, **kwargs)
                with patch.object(subprocess, 'run', side_effect=fake), self.assertRaisesRegex(RuntimeError, 'completion record|Android TAR command failed'):
                    self.module.create_backup('fixture-adb', self.device, self.directory)
                self.assertFalse(list(self.directory.iterdir()))

    def test_real_shell_tar_and_explicit_remote_failure(self):
        source = self.root / 'app-data'
        source.mkdir()
        for name, data in self.data.items():
            path = source / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
        for selected, succeeds in ((self.selected, True), (['missing-directory'], False)):
            with self.subTest(selected=selected):
                command = self.module.tar_command(self.device['package'], selected, 'fixture123')[-1]
                result = subprocess.run(['/bin/sh', '-c', command], cwd=source,
                                        stdin=subprocess.DEVNULL, capture_output=True, timeout=10)
                path = self.root / 'actual-shell.tar'
                path.write_bytes(result.stdout)
                if succeeds:
                    self.assertEqual(result.returncode, 0)
                    self.module.finish_tar(path, 'fixture123')
                    actual = self.module.inspect_tar(path, self.selected)
                    self.assertEqual({n: v['sha256'] for n, v in actual.items()},
                                     {n: hashlib.sha256(v).hexdigest() for n, v in self.data.items()})
                else:
                    self.assertNotEqual(result.returncode, 0)
                    with self.assertRaisesRegex(RuntimeError, 'Android TAR command failed'):
                        self.module.finish_tar(path, 'fixture123')

    def test_size_rejections_print_metadata_only(self):
        path = self.root / 'size-check.tar'
        for data, error in ((b'', 'bytes=0'), (self.payload + b'\r\n', 'remainder=2')):
            path.write_bytes(data)
            with self.assertRaisesRegex(RuntimeError, error):
                self.module.inspect_tar(path, self.selected)
        with path.open('wb') as stream:
            stream.truncate(self.module.MAX_BYTES + 512)
        with self.assertRaisesRegex(RuntimeError, '8 GiB limit'):
            self.module.inspect_tar(path, self.selected)


if __name__ == '__main__':
    unittest.main(verbosity=2)
