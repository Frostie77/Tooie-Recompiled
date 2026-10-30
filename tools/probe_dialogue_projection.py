#!/usr/bin/env python3
"""Check captured camera scene identity and synthetic viewport transitions.

The default replays captured CPU projection inputs, not draw calls or GPU work.
No ROM, Android SDK, installation or device access is used.
"""
import argparse
import os
from pathlib import Path
import re
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--opaque-window-types', action='store_true',
                        help='Supply unused opaque X11 handles on minimal host machines')
    cases = parser.add_mutually_exclusive_group()
    cases.add_argument('--camera-turns', action='store_true',
                        help='Measure synthetic camera turns; this does not replay a captured collision')
    cases.add_argument('--camera-trace', action='store_true',
                       help='Test the bounded observer through the actual camera consumer and failing controls')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    rt = root / 'lib/rt64/src'
    compiler = os.environ.get('CXX', 'g++')
    producer = (root / 'src/recomp_api.cpp').read_text()
    patterns = [r'constexpr uint32_t matrix_group_flags\([\s\S]*?\n}',
                r'constexpr uint32_t kProjectionGroupFlags = matrix_group_flags\([\s\S]*?;',
                r'constexpr uint32_t kGameplayProjectionId = [^;]+;']
    fragments = []
    for pattern in patterns:
        matches = re.findall(pattern, producer)
        if len(matches) != 1:
            raise SystemExit('STOP: producer declaration missing or ambiguous: ' + pattern)
        fragments.append(matches[0])

    with tempfile.TemporaryDirectory(prefix='tooie-dialogue-projection-') as temporary:
        build = Path(temporary)
        (build / 'producer.h').write_text('\n'.join(fragments) + '\n')
        (build / 'probe_queue.h').write_text(
            '#pragma once\n#include "hle/rt64_workload.h"\n'
            'namespace RT64 {struct WorkloadQueue {std::array<Workload,4> workloads;};}\n')
        sources = []
        for path, queue_include in [('hle/rt64_game_frame.cpp', 'rt64_workload_queue.h'),
                                    ('render/rt64_projection_processor.cpp', 'hle/rt64_workload_queue.h')]:
            source = (rt / path).read_text()
            anchor = '#include "' + queue_include + '"'
            if source.count(anchor) != 1:
                raise SystemExit('STOP: queue adapter anchor changed: ' + path)
            source = source.replace(anchor, '#include "probe_queue.h"')
            # Relocating this translation unit requires an absolute diagnostic
            # include. The production header and all algorithms stay unchanged.
            source = source.replace('#include "../../../../src/render_diagnostics.h"',
                                    '#include "' + str(root / 'src/render_diagnostics.h') + '"')
            destination = build / Path(path).name
            destination.write_text(source)
            sources.append(destination)
        flags = ['-std=c++17', '-O1', '-pthread', '-DNDEBUG', '-DHLSL_CPU',
                 '-D__PRFCHWINTRIN_H', '-ffunction-sections', '-fdata-sections',
                 '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-fno-pie', '-no-pie']
        if args.opaque_window_types:
            stub = build / 'platform/X11/Xlib.h'
            stub.parent.mkdir(parents=True)
            stub.write_text('#pragma once\ntypedef struct _XDisplay Display;\ntypedef unsigned long Window;\n')
            flags += ['-I' + str(build / 'platform')]
            print('Host adapter: opaque X11 handles; no window or GPU execution.', flush=True)
        for include in [build, rt, rt / 'hle', rt / 'render', rt / 'common', rt / 'contrib',
                        rt / 'contrib/hlslpp/include', rt / 'contrib/json/single_include',
                        rt / 'contrib/dxc/inc', rt / 'contrib/plume']:
            flags += ['-I' + str(include)]
        support = ['common/rt64_math.cpp', 'hle/rt64_rigid_body.cpp',
                   'hle/rt64_framebuffer_changes.cpp', 'hle/rt64_framebuffer_storage.cpp',
                   'hle/rt64_projection.cpp', 'common/rt64_common.cpp',
                   'preset/rt64_preset_scene.cpp', 'preset/rt64_preset.cpp']
        exe = build / 'probe'
        print('Compiling production matcher and projection processor with ASan/UBSan.', flush=True)
        env = {**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0'}
        for name in ['TOOIE_TRACE_PROJ', 'BT_RT64_SKY_INFINITE']:
            env.pop(name, None)
        fixture = 'camera_trace_probe.cpp' if args.camera_trace else ('camera_turn_probe.cpp' if args.camera_turns else 'dialogue_projection_probe.cpp')
        original_processor = sources[1].read_text()
        original_matcher = sources[0].read_text()
        header_path = rt / 'render/rt64_camera_diagnostics.h'
        profiles = (['current', 'missing-observer', 'raw-consumed', 'non-android'] if args.camera_trace
                    else ['default'] if args.camera_turns else ['current', 'nearest-scene-control'])
        for profile in profiles:
            processor = original_processor
            matcher = original_matcher
            if profile == 'nearest-scene-control':
                matcher, count = re.subn(r'^            // TOOIE_PRIMARY_GAMEPLAY_CAMERA_BEGIN\n[\s\S]*?^            // TOOIE_PRIMARY_GAMEPLAY_CAMERA_END\n', '', matcher, flags=re.M)
                assert count == 1
            sources[0].write_text(matcher)
            shadow = build / 'rt64_camera_diagnostics.h'
            if shadow.exists(): shadow.unlink()
            if profile == 'missing-observer':
                processor, count = re.subn(r'^            cameraDiagnostics::observe\([\s\S]*?drawData, tracePreviousData, proj, fbPair\);\n', '', processor, flags=re.M)
                assert count == 1
            if profile == 'raw-consumed':
                header = header_path.read_text()
                old = 'matrix("consumed-view",s,d.modViewTransforms[s.transform]);'
                assert header.count(old) == 1
                header = header.replace(old, 'matrix("consumed-view",s,d.viewTransforms[s.transform]);')
                header = header.replace('#include "../../../../src/runtime_diagnostics.h"', '#include "' + str(root / 'src/runtime_diagnostics.h') + '"')
                shadow.write_text(header)
            sources[1].write_text(processor)
            trace_flags = ['-DTOOIE_CAMERA_TRACE_TEST'] if args.camera_trace and profile != 'non-android' else []
            subprocess.run([compiler, *flags, *trace_flags, str(root / 'tests' / fixture),
                            *map(str, sources), *(str(rt / s) for s in support),
                            '-Wl,--gc-sections', '-o', str(exe)], check=True)
            argv = [str(exe), str(build / (profile+'-data'))] if args.camera_trace else [str(exe)]
            if profile == 'nearest-scene-control':
                baseline = subprocess.run([str(exe), '--recorded-camera-scenes'], env=env,
                                          capture_output=True, text=True)
                output = baseline.stdout + baseline.stderr
                if baseline.returncode or 'runtime error:' in output or 'ERROR: AddressSanitizer' in output:
                    raise RuntimeError('Recorded nearest-scene baseline: '+output)
                print(output.strip(), flush=True)
            result = subprocess.run(argv, env=env, capture_output=True, text=True)
            expected = (87 if profile in ('missing-observer', 'raw-consumed')
                        else 88 if profile == 'nearest-scene-control' else 0)
            output = result.stdout + result.stderr
            if result.returncode != expected or 'runtime error:' in output or 'ERROR: AddressSanitizer' in output:
                raise RuntimeError(profile+': '+output)
            if expected:
                assert ('camera scene continuity failure' if expected == 88 else 'camera trace failure') in output
                print('PASS: '+profile+' fails its actual camera-consumer assertion.', flush=True)
            else:
                print(output.strip(), flush=True)


if __name__ == '__main__':
    main()
