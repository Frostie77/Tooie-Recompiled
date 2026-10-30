#!/usr/bin/env python3
"""Exercise saved Graphics UI values through the actual RT64 scale consumers."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile
from probe_deformed_vertices import method


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    rt = root / 'lib/rt64/src'
    if args.build_dir:
        args.build_dir.mkdir(parents=True, exist_ok=True)
    build = Path(tempfile.mkdtemp(prefix='tooie-manual-resolution-', dir=args.build_dir))
    stub = build / 'recompui'; stub.mkdir()
    config = (root / 'lib/RecompFrontend/recompui/include/recompui/config.h').read_text()
    (stub / 'config.h').write_text('#pragma once\n#include "librecomp/config.hpp"\n'
        'namespace recompui::config {\n' + method(config, 'namespace graphics {') + '\n'
        'recomp::config::Config &get_graphics_config();\n'
        'recomp::config::Config &get_config(const std::string&);\n'
        'recomp::config::Config &create_config_tab(const std::string&,const std::string&,bool);\n'
        'recomp::config::Config &create_graphics_tab(const std::string&);\n}\n')
    (stub / 'renderer.h').write_text('#pragma once\n#include "common/rt64_user_configuration.h"\n#include "ultramodern/config.hpp"\n'
        'namespace recompui::renderer {RT64::UserConfiguration::Antialiasing RT64MaxMSAA();}\n')
    (build / 'util').mkdir(); (build / 'util/steam_deck.h').write_text('#pragma once\ninline bool is_steam_deck(){return false;}\n')
    (build / 'android').mkdir(); (build / 'android/native_window.h').write_text('#pragma once\nstruct ANativeWindow;\n')
    (build / 'miniz_export.h').write_text('#pragma once\n#define MINIZ_EXPORT\n')
    source = (root / 'lib/RecompFrontend/recompui/src/renderer/rt64_render_context.cpp').read_text()
    conversions = '\n'.join(method(source, 'RT64::UserConfiguration::' + name + ' to_rt64(')
        for name in ['AspectRatio', 'Antialiasing', 'RefreshRate', 'InternalColorFormat'])
    consumer = conversions + '\n' + method(source, 'void set_application_user_config(')
    update = method(source, 'bool renderer::RT64Context::update_config(')
    (build / 'resolution-consumer.inc').write_text(consumer + '\n' + update)
    queue = (rt / 'hle/rt64_workload_queue.cpp').read_text()
    (build / 'resolution-workload.inc').write_text(method(queue, 'void WorkloadQueue::threadConfigurationUpdate('))
    header = (rt / 'hle/rt64_workload_queue.h').read_text()
    (build / 'resolution-workload-config.inc').write_text(method(header, 'struct WorkloadConfiguration {') + ';\n')
    runtime = root / 'lib/N64ModernRuntime'
    includes = [build, root / 'src', runtime / 'librecomp/include', runtime / 'librecomp/include/librecomp',
        runtime / 'N64Recomp/include', runtime / 'ultramodern/include', runtime / 'thirdparty',
        runtime / 'thirdparty/miniz', runtime / 'thirdparty/concurrentqueue', rt, rt / 'common',
        rt / 'contrib', rt / 'contrib/plume', rt / 'contrib/json/single_include', rt / 'contrib/hlslpp/include']
    flags = ['-std=c++20', '-O1', '-D__ANDROID__', '-DRT_ENABLED=0', '-DHLSL_CPU',
        '-D__PRFCHWINTRIN_H', '-ffunction-sections', '-fdata-sections', '-fsanitize=address,undefined,float-cast-overflow',
        '-fno-omit-frame-pointer', '-fno-pie', *['-I' + str(p) for p in includes]]
    env = {**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0', 'UBSAN_OPTIONS': 'halt_on_error=1:print_stacktrace=1'}
    compiler = os.environ.get('CXX', 'g++')
    def run(command, label, expected=0):
        result = subprocess.run([str(x) for x in command], cwd=root, env=env, capture_output=True, text=True, timeout=240)
        output = result.stdout + result.stderr; (build / (label + '.log')).write_text(output)
        if result.returncode != expected or 'runtime error:' in output or 'ERROR: AddressSanitizer' in output:
            raise RuntimeError(f'{label}: exit {result.returncode}, expected {expected}\n{output[-6000:]}\nLogs: {build}')
        if expected and 'resolution failure:' not in output:
            raise RuntimeError(f'{label}: missing intended assertion\n{output}')
        for line in output.splitlines():
            if line.startswith('PASS:'): print(line, flush=True)
    paths = [runtime / 'librecomp/src/config.cpp', runtime / 'librecomp/src/config_option.cpp',
        runtime / 'librecomp/src/files.cpp', rt / 'common/rt64_user_configuration.cpp',
        root / 'lib/RecompFrontend/recompui/src/config/ui_config_tab_graphics.cpp']
    objects = []
    for index, path in enumerate(paths):
        obj = build / f'support-{index}.o'; objects.append(obj)
        run([compiler, *flags, '-c', path, '-o', obj], f'compile-{index}')
    profiles = {'current': consumer + '\n' + update,
        'missing-discard': consumer + '\n' + update.replace(' ||\n        (new_config.res_option == ultramodern::renderer::Resolution::Manual && new_config.res_manual_scale != old_config.res_manual_scale)', '')}
    start = consumer.index('        case ultramodern::renderer::Resolution::Manual:')
    end = consumer.index('        case ultramodern::renderer::Resolution::Original:', start)
    profiles['missing-manual'] = consumer[:start] + consumer[end:] + '\n' + update
    for label, body in profiles.items():
        (build / 'resolution-consumer.inc').write_text(body)
        obj = build / (label + '.o'); exe = build / label
        run([compiler, *flags, '-c', root / 'tests/manual_resolution.cpp', '-o', obj], 'compile-' + label)
        run([compiler, '-fsanitize=address,undefined,float-cast-overflow', '-no-pie', *objects, obj,
            '-Wl,--gc-sections', '-o', exe], 'link-' + label)
        run([exe, build / (label + '-config')], 'execute-' + label, 0 if label == 'current' else 85)
    print('PASS: removing Manual mapping or framebuffer discard on a scale-only change fails the real consumer checks.', flush=True)
    print('Scope: actual Graphics menu/config persistence, frontend mapping and workload scale method; UI surfaces and GPU/resource services are controlled. No Redmi timing or visual acceptance.', flush=True)


if __name__ == '__main__': main()
