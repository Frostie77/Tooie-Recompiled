#!/usr/bin/env python3
"""Compare the production matcher with its verified pre-storage-change parent."""
import argparse
import hashlib
import os
from pathlib import Path
import subprocess
import tempfile

BASE = 'd403031830c34ad0146724cf6d20a85c96502d26'
BASE_BLOBS = {
    'rt64_game_frame.cpp': '4ffcb93acfb324899f1ab50632d533fde3faa46c',
    'rt64_match_candidates.h': '256d974a6adcd42820415075890b8864401ee22e',
}


def run(argv, **kwargs):
    return subprocess.run([str(x) for x in argv], check=True, **kwargs)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--tested-base-root', type=Path)
    parser.add_argument('--build-dir', type=Path)
    parser.add_argument('--opaque-window-types', action='store_true',
                        help='Supply unused opaque X11 handle declarations on minimal host test machines')
    parser.add_argument('--scalar', action='store_true', help='Force the independent HLSL scalar math path')
    parser.add_argument('--require-simd', action='store_true', help='Refuse a scalar-only compiler/configuration')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    rt = root / 'lib/rt64/src'
    compiler = os.environ.get('CXX', 'g++')
    parent = {}
    for name, expected in BASE_BLOBS.items():
        path = 'lib/rt64/src/hle/' + name
        data = ((args.tested_base_root / path).read_bytes() if args.tested_base_root
                else subprocess.check_output(['git', '-C', str(root), 'show', BASE + ':' + path]))
        actual = hashlib.sha1(b'blob ' + str(len(data)).encode() + b'\0' + data).hexdigest()
        if actual != expected:
            raise SystemExit('STOP: parent blob mismatch: ' + path)
        parent[name] = data.decode().replace('\r\n', '\n')

    with tempfile.TemporaryDirectory(prefix='tooie-candidate-storage-') as temporary:
        build = (args.build_dir or Path(temporary)).resolve()
        build.mkdir(parents=True, exist_ok=True)
        (build / 'matching_queue.h').write_text(
            '#include "hle/rt64_workload.h"\n'
            'namespace RT64 {struct WorkloadQueue {std::array<Workload,4> workloads;};}\n')
        flags = ['-std=c++17', '-O1', '-pthread', '-DNDEBUG', '-DHLSL_CPU',
                 '-D__PRFCHWINTRIN_H', '-ffunction-sections', '-fdata-sections',
                 '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-fno-pie', '-no-pie']
        if args.scalar:
            flags += ['-DHLSLPP_SCALAR']
        if args.opaque_window_types:
            stub = build / 'platform/X11/Xlib.h'
            stub.parent.mkdir(parents=True, exist_ok=True)
            stub.write_text('#pragma once\ntypedef struct _XDisplay Display;\ntypedef unsigned long Window;\n')
            flags += ['-I' + str(build / 'platform')]
            print('Host fixture uses opaque unused X11 types; no window/GPU execution.', flush=True)
        for include in [build, root / 'tests', rt, rt / 'hle', rt / 'common', rt / 'contrib',
                        rt / 'contrib/hlslpp/include', rt / 'contrib/json/single_include',
                        rt / 'contrib/dxc/inc', rt / 'contrib/plume']:
            flags += ['-I' + str(include)]
        macros = subprocess.check_output([compiler, *flags, '-dM', '-E', '-x', 'c++',
            '-include', str(rt / 'common/rt64_common.h'), '-'], input=b'').decode()
        scalar = '#define HLSLPP_SCALAR' in macros
        if args.require_simd and scalar:
            raise SystemExit('STOP: an independent SIMD matcher build was required, but scalar math was selected')
        print('Fixture math path:', 'scalar' if scalar else 'SIMD', flush=True)
        support = ['common/rt64_math.cpp', 'hle/rt64_rigid_body.cpp',
                   'hle/rt64_framebuffer_changes.cpp', 'hle/rt64_framebuffer_storage.cpp',
                   'hle/rt64_projection.cpp', 'common/rt64_common.cpp',
                   'preset/rt64_preset_scene.cpp', 'preset/rt64_preset.cpp']
        objects = []
        for source in support:
            obj = build / (Path(source).name + '.o')
            print('Compiling', source, flush=True)
            run([compiler, *flags, '-c', rt / source, '-o', obj])
            objects.append(obj)
        fixture = build / 'fixture.o'
        run([compiler, *flags, '-c', root / 'tests/matching_equivalence.cpp', '-o', fixture])
        header = (rt / 'hle/rt64_match_candidates.h').read_text()
        source = (rt / 'hle/rt64_game_frame.cpp').read_text()
        unfinalized = source
        for name in ['transformCheckSet', 'tileCheckSet', 'lookAtCheckSet']:
            anchor = name + '.finalize();'
            assert unfinalized.count(anchor) == 1, 'Missing production finalization: ' + name
            unfinalized = unfinalized.replace(anchor, '/* wrong unfinished candidate union */')
        assert header.count('std::sort(pairs.begin(), pairs.end());') == 1
        assert header.count('pairs.erase(std::unique(pairs.begin(), pairs.end()), pairs.end());') == 1
        variants = {
            'parent': (parent['rt64_game_frame.cpp'], parent['rt64_match_candidates.h']),
            'current': (source, header),
            'unfinalized': (unfinalized, header),
            'unordered': (source,
                          header.replace('std::sort(pairs.begin(), pairs.end());', '/* wrong ordering */')),
            'duplicates': (source,
                           header.replace('pairs.erase(std::unique(pairs.begin(), pairs.end()), pairs.end());',
                                          '/* wrong union */')),
        }
        env = {**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0'}
        for mode, (source, candidate_header) in variants.items():
            directory = build / mode
            directory.mkdir(exist_ok=True)
            (directory / 'rt64_match_candidates.h').write_text(candidate_header)
            source = source.replace('#include "../../../../src/render_diagnostics.h"',
                                    '#include "matching_timer.h"\n#include "matching_equivalence.h"')
            source = source.replace('#include "rt64_workload_queue.h"', '#include "matching_queue.h"')
            source = source.replace('hashMap.emplace(hashFromCall(call, matrixIdHash),',
                                    'hashMap.emplace(matching_test::forceCollision ? 7 : hashFromCall(call, matrixIdHash),')
            anchor = '        stages.elapsed(4,pairsStart);'
            assert source.count(anchor) == 1
            source = source.replace(anchor,
                '        matching_test::capture(transformCheckSet,tileCheckSet,lookAtCheckSet);\n' + anchor)
            cpp, obj, exe = directory / 'matcher.cpp', directory / 'matcher.o', directory / 'test'
            cpp.write_text(source)
            print('Compiling production matcher:', mode, flush=True)
            run([compiler, *flags, '-c', cpp, '-o', obj])
            run([compiler, *flags, fixture, obj, *objects, '-Wl,--gc-sections', '-o', exe])
            with (directory / 'run.log').open('w') as log:
                result = subprocess.run([str(exe), str(directory / 'output.bin')], env=env,
                                        stdout=log, stderr=subprocess.STDOUT)
            if mode in ['parent', 'current']:
                if result.returncode:
                    raise SystemExit((directory / 'run.log').read_text())
                print((directory / 'run.log').read_text(), end='')
            elif result.returncode not in [0, 1]:
                raise SystemExit('Mutation terminated unexpectedly: ' + mode)
            elif result.returncode == 0 and (directory / 'output.bin').read_bytes() == (build / 'parent/output.bin').read_bytes():
                raise SystemExit('FAIL: actual-consumer mutation accepted: ' + mode)
            else:
                print('PASS: actual-consumer mutation rejected:', mode, flush=True)
        if (build / 'parent/output.bin').read_bytes() != (build / 'current/output.bin').read_bytes():
            raise SystemExit('FAIL: candidate/geometry/tile/look-at/velocity output differs from exact parent')
        print('PASS: 303 full-matcher frames are byte-identical to exact d403031 parent', flush=True)
        for mode in ['parent', 'current']:
            print('Host observation only, ASan/UBSan:', mode, flush=True)
            run([build / mode / 'test', build / 'unused.bin', '--device-timing'], env=env)


if __name__ == '__main__':
    main()
