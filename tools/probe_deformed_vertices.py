#!/usr/bin/env python3
"""Execute the deforming-vertex and skeletal seam regression without a ROM/GPU.

Real generated deformation functions, native metadata, RSP/GBI, matcher,
transform processor and shader expression run; queue/RAM/allocation are fixtures.
"""
import argparse
import os
from pathlib import Path
import re
import subprocess
import tempfile


def method(source, needle):
    pos = source.index(needle)
    start = source.rfind('\n', 0, pos) + 1
    end = source.index('{', pos) + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline-api', type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    rt = root/'lib/rt64/src'
    with tempfile.TemporaryDirectory(prefix='tooie-deformed-vertices-') as folder:
        build = Path(folder)
        (build/'matching_queue.h').write_text('#pragma once\n#include "hle/rt64_workload.h"\nnamespace RT64 {struct WorkloadQueue {std::array<Workload,4> workloads;int writeCursor=0;};}\n')
        stub = build/'platform/X11/Xlib.h';stub.parent.mkdir(parents=True)
        stub.write_text('#pragma once\ntypedef struct _XDisplay Display;typedef unsigned long Window;\n')
        for name in ['hle/rt64_game_frame.cpp','render/rt64_transform_processor.cpp']:
            s=(rt/name).read_text().replace('#include "rt64_workload_queue.h"','#include "matching_queue.h"').replace('#include "hle/rt64_workload_queue.h"','#include "matching_queue.h"')
            s=s.replace('#include "../../../../src/render_diagnostics.h"','#include "render_diagnostics.h"')
            (build/Path(name).name).write_text(s)
        source=(rt/'hle/rt64_rsp.cpp').read_text()
        names=['RSP','reset','clearExtended','maskPhysicalAddress','fromSegmented','fromSegmentedMasked','matrixCommon','matrix','popMatrix','skeletalBinding','matrixId','popMatrixId','computeModelViewProj']
        bodies='\n'.join(method(source,'RSP::'+n+'(') for n in names)
        for name in ['readExtendedVertexSegment','setVertexCommon']:
            body=method(source,'RSP::'+name+'(');pos=source.index(body)
            bodies+='\n'+source[source.rfind('    template<',0,pos):pos]+body
        src=(rt/'gbi/rt64_gbi.cpp').read_text()
        for name in ['p0','p1']:bodies+='\n'+method(src,'DisplayList::'+name+'(')
        bodies+='\n'+method((rt/'hle/rt64_workload.cpp').read_text(),'Workload::resetDrawData(')
        (build/'skeletal-rsp.inc').write_text(bodies)
        src=(rt/'gbi/rt64_gbi_extended.cpp').read_text()
        (build/'skeletal-gbi.inc').write_text('\n'.join(method(src,'void '+n+'(') for n in ['matrixGroupCommand','matrixGroupV1','popMatrixGroupV1']))
        shader=(rt/'shaders/RSPWorldCS.hlsl').read_text()
        matches=re.findall(r'^    const float4 worldPos = [^;]+;',shader,re.M)
        assert len(matches)==1
        (build/'shader-position.inc').write_text(matches[0]+'\n')
        funcs={'dbanim_entrypoint_0':'funcs_73.c','func_800B2190':'funcs_25.c','func_800B2344':'funcs_25.c','func_800ADE2C':'funcs_25.c','func_800ADE40':'funcs_25.c','func_80116568':'funcs_38.c'}
        extracted=[]
        for name,file in funcs.items():
            source=(root/'RecompiledFuncs'/file).read_text()
            found=re.findall(r'^RECOMP_FUNC void '+name+r'\(.*?(?=^RECOMP_FUNC |\Z)',source,re.M|re.S)
            assert len(found)==1;extracted.append(found[0])
        source=''.join(extracted);calls=set(re.findall(r'\b(\w+)\(rdram, ctx\);',source))
        header='#include "tooie_recomp.h"\n'+''.join('void '+n+'(uint8_t*,recomp_context*);\n' for n in sorted(calls|funcs.keys()))
        (build/'deform.c').write_text(header+source)
        runtime=(root/'lib/N64ModernRuntime/librecomp/src/recomp.cpp').read_text()
        (build/'coprocessor.cpp').write_text('#include "recomp.h"\nenum class StatusReg { FR=0x04000000 };\n'+
            '\n'.join(method(runtime, name+'(') for name in ['cop0_status_write','cop0_status_read']))
        inc=[build,build/'platform',root/'src',root/'include',root/'RecompiledFuncs',root/'tests/android_headers',root/'lib/N64Recomp/include',
             root/'lib/N64ModernRuntime/librecomp/include',root/'lib/N64ModernRuntime/librecomp/src',root/'lib/N64ModernRuntime/ultramodern/include',
             root/'lib/N64ModernRuntime/thirdparty',root/'lib/N64ModernRuntime/thirdparty/concurrentqueue',root/'lib/N64Recomp/lib/fmt/include',
             root/'lib/N64Recomp/lib/tomlplusplus',root/'lib/rt64/include',rt,rt/'render',rt/'hle',rt/'common',rt/'contrib',rt/'contrib/hlslpp/include',
             rt/'contrib/json/single_include',rt/'contrib/dxc/inc',rt/'contrib/plume',rt/'contrib/mupen64plus-core/src/api']
        flags=['-O1','-pthread','-DNDEBUG','-D__ANDROID__','-DHLSL_CPU','-D__PRFCHWINTRIN_H','-ffunction-sections','-fdata-sections',
               '-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie',*['-I'+str(p) for p in inc]]
        env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1:print_stacktrace=1','APP_FOLDER_PATH':str(build)}
        def run(cmd):subprocess.run([str(x) for x in cmd],check=True,env=env,cwd=root)
        run([os.environ.get('CC','gcc'),'-std=c11','-fwrapv',*flags,'-c',build/'deform.c','-o',build/'deform.o'])
        cpp=[root/'tests/deformed_vertices.cpp',root/'tests/skeletal_rsp.cpp',args.baseline_api or root/'src/recomp_api.cpp',build/'rt64_game_frame.cpp',build/'rt64_transform_processor.cpp',build/'coprocessor.cpp']
        cpp += [rt/p for p in ['hle/rt64_rigid_body.cpp','common/rt64_math.cpp','hle/rt64_framebuffer_changes.cpp','hle/rt64_framebuffer_storage.cpp','hle/rt64_projection.cpp','common/rt64_common.cpp','preset/rt64_preset_scene.cpp','preset/rt64_preset.cpp']]
        exe=build/'probe'
        run([os.environ.get('CXX','g++'),'-std=c++20',*flags,*cpp,build/'deform.o','-Wl,--gc-sections','-no-pie','-o',exe])
        if args.baseline_api:
            control=subprocess.run([str(exe)],env=env,capture_output=True,text=True)
            assert control.returncode==31 and 'seam gap=' in control.stderr and 'gap<.001f' in control.stderr,control.stderr
            print(control.stderr.strip())
            print('PASS: exact baseline metadata fails the rendered-position seam check.',flush=True)
            return
        run([exe])
        control=subprocess.run([str(exe),'--force-skip'],env=env,capture_output=True,text=True)
        assert control.returncode==31 and 'seam gap=' in control.stderr and 'gap<.001f' in control.stderr,control.stderr
        print('PASS: forcing the former SKIP vertex policy fails the rendered-position seam check.',flush=True)
        print('Scope: controlled translating seam, not a replay of the supplied NPC video; no GPU/device execution.',flush=True)


if __name__=='__main__':main()
