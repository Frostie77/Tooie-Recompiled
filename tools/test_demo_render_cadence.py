#!/usr/bin/env python3
"""Run production command/decode/tick expressions on synthetic guarded RAM."""
from pathlib import Path
import hashlib
import os
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def function(source, signature):
    start = source.index(signature)
    brace = source.index('{', start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end] + '\n'


def command_block(source):
    frame = function(source, 'extern "C" void tooie_rt64_begin_frame(')
    start = frame.index('    if (!append_command(rdram, kRenderBufferState, kEnableWord0,')
    end = frame.index('#if TOOIE_CORRECTION_TRACE_ENABLED', start)
    return frame[start:end]


def main():
    api = (ROOT / 'src/recomp_api.cpp').read_text()
    extended = (ROOT / 'lib/rt64/src/gbi/rt64_gbi_extended.cpp').read_text()
    state = (ROOT / 'lib/rt64/src/hle/rt64_state.cpp').read_text()
    queue = (ROOT / 'lib/rt64/src/hle/rt64_workload_queue.cpp').read_text()
    # Prove where the non-consuming borrow runs relative to input acceptance
    # and the existing wait, in actual generated caller bodies.
    core = (ROOT / 'RecompiledFuncs/funcs_0.c').read_text()
    draw = (ROOT / 'RecompiledFuncs/funcs_24.c').read_text()
    update = function(core, 'RECOMP_FUNC void func_800123F4(')
    assert update.index('func_8001608C(rdram, ctx);') < update.index('func_800123BC(rdram, ctx);')
    wrapper = function(draw, 'RECOMP_FUNC void func_800A7840(')
    assert wrapper.index('func_800A73F4(rdram, ctx);') < wrapper.index('func_80015410(rdram, ctx);')
    assert 'tooie_rt64_begin_frame(rdram, ctx);' in function(draw, 'RECOMP_FUNC void func_800A73F4(')
    helpers = ''.join(function(api, signature) for signature in (
        'uint32_t read_word(', 'void write_word(', 'bool in_rdram(', 'bool append_command('))
    constants = ''.join(re.findall(r'^constexpr uint32_t (?:kRdramStart|kRdramEnd|kViRetraceInterval|kRenderBufferState) = .*?;\n', api, re.M))
    assert constants.count('constexpr') == 4
    # These are the native source's existing static-asserted command values.
    for value in ('kEnableWord0 == 0xE0525464U', 'kEnableWord1 == 0x10000064U', 'kSetRefreshRateWord0 == 0x64000009U'):
        assert value in api
    prefix = '''#include "demo_pacing_guest.h"
#include <algorithm>
#include <cassert>
'''+constants+helpers+'''
constexpr uint32_t kEnableWord0=0xE0525464U, kEnableWord1=0x10000064U, kSetRefreshRateWord0=0x64000009U;
static unsigned failures=0, g_frame_enables=0;
void note_append_failure(unsigned) { ++failures; }
struct DisplayList { uint32_t words[2]; uint32_t p1(unsigned shift,unsigned width) const { return (words[1]>>shift)&((1U<<width)-1); } };
struct Extended {uint16_t refreshRate=0;};
struct State {Extended extended; void setRefreshRate(uint16_t);};
'''
    setter = function(state, 'void State::setRefreshRate(')
    decode = function(extended, 'void setRefreshRateV1(')
    assignment = re.search(r'workload\.viOriginalRate = extended\.refreshRate;', state)[0]
    schedule_start = queue.index('                uint32_t displayFrames = 1;')
    schedule_end = queue.index('                ext.sharedResources->viOriginalRate', schedule_start)
    schedule = queue[schedule_start:schedule_end]
    consumers = setter+decode+'''
struct Workload {uint32_t viOriginalRate=0;};
void copy_rate(Extended extended,Workload &workload) {'''+assignment+'''}
uint32_t planned_frames(uint32_t rate,uint32_t target) {
    Workload workload{rate}; struct {uint32_t targetRate;} workloadConfig{target};
    bool generateInterpolatedFrames=rate>0 && target>rate, frameReduction=false;
    int64_t logicalTicks=0,displayTicks=0; float deltaTimeMs=0;
'''+schedule+'''return displayFrames; }
'''
    fixture = ROOT / 'tests/fixtures/demo-render-base-emission.inc'
    old = fixture.read_text()
    assert hashlib.sha256(fixture.read_bytes()).hexdigest() == 'fc1289a969a2021c86edf1c49453533041010ceb4847f55c30795dc0af52db9d'
    assert 'renderInterval' not in old and 'const uint32_t divisor = read_word(rdram, kViRetraceInterval);' in old
    # The fixture is the complete unchanged native emission block from 8bcaad0;
    # retain an independent exact-parent comparison when its Git object exists.
    git = subprocess.run(['git', '-C', str(ROOT), 'show', '8bcaad010df3ff89f1833c7958a8a52cad77259b:src/recomp_api.cpp'], capture_output=True, text=True)
    if git.returncode == 0:
        assert command_block(git.stdout) == old
    with tempfile.TemporaryDirectory(prefix='tooie-render-cadence-') as folder:
        folder = Path(folder)
        for name, android, block in [('android', True, command_block(api)), ('desktop', False, command_block(api)), ('parent', True, old)]:
            (folder / 'consumer.inc').write_text(prefix+consumers+'void native_commands(uint8_t *rdram) {\n'+block+'}\n')
            command = [*shlex.split(os.environ.get('CXX', 'g++')), '-std=c++20', '-pthread', '-O1', '-g', '-fwrapv', '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-I'+str(ROOT/'src'), '-I'+str(ROOT/'lib/N64Recomp/include'), '-I'+str(folder)]
            if android:
                command.append('-D__ANDROID__')
            subprocess.run([*command, str(ROOT/'tests/demo_render_cadence.cpp'), '-o', str(folder/'test')], check=True)
            result = subprocess.run([str(folder/'test')], capture_output=True, text=True, env={**os.environ, 'ASAN_OPTIONS':'detect_leaks=0', 'UBSAN_OPTIONS':'halt_on_error=1'})
            if name == 'parent':
                assert result.returncode == 1 and 'render-cadence mismatch: configured=2 record=3 wait=3 emitted=30 planned120=4' in result.stderr, result.stdout+result.stderr
                print(result.stderr.strip())
            else:
                assert result.returncode == 0, result.stdout+result.stderr
                print(name+': '+result.stdout.strip())
    print('PASS: exact 8bcaad0 command-emission control fails the replay renderer-duration requirement')


if __name__ == '__main__':
    main()
