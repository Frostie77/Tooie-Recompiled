#!/usr/bin/env python3
"""Host recording of the actual optional compose setup; no GPU execution."""
from pathlib import Path
import argparse
import os
import re
import subprocess
import tempfile


def extract(source):
    setup = source.split('void ShaderLibrary::setupCommonShaders(', 1)[1].split(
        'void ShaderLibrary::setupMultisamplingShaders(', 1)[0]
    start = setup.index('// Raytracing compose.')
    end = setup.index('/*', start)
    capability = re.search(r'const RenderDeviceCapabilities deviceCapabilities = device->getCapabilities\(\);', setup)
    assert capability is not None
    return capability[0], setup[start:end]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline', type=Path, help='Check exact old ShaderLibrary source fails the recorded raster-only case')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    current = root / 'lib/rt64/src/render/rt64_shader_library.cpp'
    capability, block = extract((args.baseline or current).read_text())
    env = {**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0',
           'UBSAN_OPTIONS': 'halt_on_error=1:print_stacktrace=1'}
    with tempfile.TemporaryDirectory(prefix='tooie-optional-compose-') as directory:
        build = Path(directory)
        (build/'compose-capabilities.inc').write_text(capability+'\n')
        flags = ['-std=c++17', '-D__ANDROID__', '-DNDEBUG', '-fsanitize=address,undefined',
                 '-fno-omit-frame-pointer', '-no-pie', '-I'+str(build),
                 '-I'+str(root/'tests/android_headers'),
                 '-I'+str(root/'lib/rt64/src/contrib/plume')]

        def compile_and_run(text, rt):
            (build/'compose-production.inc').write_text(text)
            exe = build/'probe'
            subprocess.run([os.environ.get('CXX', 'g++'), *flags, '-DRT_ENABLED='+str(rt),
                            str(root/'tests/optional_raytracing_compose.cpp'), '-o', str(exe)], check=True)
            return subprocess.run([str(exe)], env=env, capture_output=True, text=True)

        def fails_as_recorded(result):
            assert result.returncode == 64, (result.returncode, result.stdout, result.stderr)
            assert 'unexpected compose request on raster-only path' in result.stderr, result.stderr
            assert 'format=109 optimal=0xDC83' in result.stderr, result.stderr

        if args.baseline:
            fails_as_recorded(compile_and_run(block, 0))
            print('PASS: exact old source fails the recorded raster-only format/blend case.')
            return
        for rt in (0, 1):
            result = compile_and_run(block, rt)
            assert result.returncode == 0, (result.stdout, result.stderr)
            print(result.stdout.strip())
        # Restore unconditional creation while retaining the real production
        # descriptor/shader setup. The recording device must expose the failure.
        control, changed = re.subn(r'#if RT_ENABLED\s+if \(deviceCapabilities\.raytracing\)',
                                  'if (true)', block)
        assert changed == 1 and control.count('#endif') == 1
        control = control.replace('#endif', '')
        fails_as_recorded(compile_and_run(control, 0))
        print('PASS: restoring unconditional compose creation reproduces the recorded refusal.')
        print('Scope: extracted production setup, actual descriptor/capability types and recording stand-ins; no full startup, shader binary or GPU/device execution.')


if __name__ == '__main__':
    main()
