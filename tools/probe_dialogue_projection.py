#!/usr/bin/env python3
"""Probe synthetic camera/aspect transitions through production RT64 CPU code.

This is not a replay of MD-002 and cannot identify its cause. No ROM, GPU,
Android SDK, installation or device access is used.
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
        subprocess.run([compiler, *flags, str(root / 'tests/dialogue_projection_probe.cpp'),
                        *map(str, sources), *(str(rt / s) for s in support),
                        '-Wl,--gc-sections', '-o', str(exe)], check=True)
        env = {**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0'}
        for name in ['TOOIE_TRACE_PROJ', 'BT_RT64_SKY_INFINITE']:
            env.pop(name, None)
        subprocess.run([str(exe)], check=True, env=env)


if __name__ == '__main__':
    main()
