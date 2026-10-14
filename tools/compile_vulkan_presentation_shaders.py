#!/usr/bin/env python3
"""Compile production VI/UI HLSL with the repository's SPIR-V flags for tests.
DXC and spirv-val must be installed; no test shader substitutes are used.
"""
import argparse
import pathlib
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--dxc', required=True)
parser.add_argument('--spirv-val', required=True)
parser.add_argument('--output', type=pathlib.Path, required=True)
args = parser.parse_args()
root = pathlib.Path(__file__).resolve().parents[1]
args.output.mkdir(parents=True, exist_ok=True)
shaders = [
    ('FullScreenVS', 'lib/rt64/src/shaders/FullScreenVS.hlsl', 'VSMain', 'vs_6_3', []),
    ('VideoInterfacePSRegular', 'lib/rt64/src/shaders/VideoInterfacePS.hlsl', 'PSMain', 'ps_6_3', []),
    ('VideoInterfacePSPixel', 'lib/rt64/src/shaders/VideoInterfacePS.hlsl', 'PSMain', 'ps_6_3', ['-D', 'PIXEL_ANTIALIASING']),
    ('InterfaceVS', 'lib/RecompFrontend/recompui/shaders/InterfaceVS.hlsl', 'VSMain', 'vs_6_3', []),
    ('InterfacePS', 'lib/RecompFrontend/recompui/shaders/InterfacePS.hlsl', 'PSMain', 'ps_6_3', []),
]
for name, source, entry, profile, defines in shaders:
    binary = args.output / (name + '.spv')
    flags = ['-spirv', '-fspv-target-env=vulkan1.0', '-fvk-use-dx-layout',
             '-I', str(root / 'lib/rt64/src'), '-E', entry, '-T', profile]
    if profile.startswith('vs'):
        flags += ['-fvk-invert-y']
    subprocess.run([args.dxc, *flags, *defines, str(root / source), '-Fo', str(binary)], check=True)
    subprocess.run([args.spirv_val, '--target-env', 'vulkan1.2', str(binary)], check=True)
    shader_bytes = binary.read_bytes()
    lines = ['#pragma once', '#include <cstdint>', f'alignas(4) constexpr unsigned char {name}BlobSPIRV[] = {{']
    for offset in range(0, len(shader_bytes), 16):
        lines += ['  ' + ', '.join(f'0x{byte:02x}' for byte in shader_bytes[offset:offset + 16]) + ',']
    lines += ['};', '']
    (args.output / (name + '.hlsl.spirv.h')).write_text('\n'.join(lines))
