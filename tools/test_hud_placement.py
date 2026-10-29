#!/usr/bin/env python3
"""Exercise whole-component HUD placement through retained guest and real render consumers."""
import argparse
import os
from pathlib import Path
import re
import subprocess
import tempfile
from probe_deformed_vertices import method
from test_hud_diagnostics import read_consumers


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir',type=Path)
    args=parser.parse_args();root=Path(__file__).resolve().parents[1];rt=root/'lib/rt64/src'
    if args.build_dir:args.build_dir.mkdir(parents=True,exist_ok=True)
    build=Path(tempfile.mkdtemp(prefix='tooie-hud-placement-',dir=args.build_dir))
    env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1:print_stacktrace=1'}
    def run(cmd,label,expected=0):
        p=subprocess.run([str(x) for x in cmd],cwd=root,env=env,capture_output=True,text=True,timeout=240)
        output=p.stdout+p.stderr;(build/(label+'.log')).write_text(output)
        if p.returncode!=expected or 'runtime error:' in output or 'ERROR: AddressSanitizer' in output:
            raise RuntimeError(f'{label}: exit {p.returncode}, expected {expected}\n{output[-5000:]}\nLogs: {build}')
        for line in output.splitlines():
            if line.startswith('PASS:'):print(line,flush=True)
    _,bodies,_=read_consumers(root)
    guest=re.sub(r'^    tooie_hud_component_\w+\(rdram, ctx\);\n','',''.join(bodies.values()),flags=re.M)
    calls=set(re.findall(r'\b(\w+)\(rdram, ctx\);',guest))
    header='#include "tooie_recomp.h"\n'+''.join('void '+n+'(uint8_t*,recomp_context*);\n' for n in sorted(calls) if not n.startswith('tooie_hud_placement_'))
    (build/'guest.c').write_text(header+guest)
    (build/'matching_queue.h').write_text('#pragma once\n#include "hle/rt64_workload.h"\nnamespace RT64 {struct WorkloadQueue {std::array<Workload,4> workloads;int writeCursor=0;};}\n')
    stub=build/'platform/X11/Xlib.h';stub.parent.mkdir(parents=True);stub.write_text('#pragma once\ntypedef struct _XDisplay Display;typedef unsigned long Window;\n')
    source=(rt/'hle/rt64_rsp.cpp').read_text()
    names=['RSP','reset','clearExtended','addCurrentProjection','setViewportAlign','setViewport']
    rsp='\n'.join(method(source,'RSP::'+n+'(') for n in names[:-1])
    # Include both the ordinary viewport entry and its explicit-alignment overload.
    rsp+='\n'+method(source,'RSP::setViewport(uint32_t address)')+'\n'+method(source,'RSP::setViewport(uint32_t address, uint16_t ori,')
    rsp+='\n'+method(source,'RSP::maskPhysicalAddress(')+'\n'+method(source,'RSP::fromSegmented(')+'\n'+method(source,'RSP::fromSegmentedMasked(')
    gbi=(rt/'gbi/rt64_gbi.cpp').read_text();rsp+='\n'+'\n'.join(method(gbi,'DisplayList::'+n+'(') for n in ['p0','p1'])
    workload=(rt/'hle/rt64_workload.cpp').read_text();rsp+='\n'+method(workload,'Workload::resetDrawData(')+'\n'+method(workload,'Workload::currentFramebufferPairIndex(')
    rsp+='\n'+method((rt/'hle/rt64_light_manager.cpp').read_text(),'LightManager::reset(')
    (build/'hud-rsp.inc').write_text(rsp)
    rdp=(rt/'hle/rt64_rdp.cpp').read_text()
    selected=[method(rdp,'RDP::'+n+'(') for n in ['reset','clearExtended','movedFromOrigin','updateCallTexcoords']]
    selected+=[method(rdp,'RDP::drawRect(int32_t ulx, int32_t uly, int32_t lrx, int32_t lry, int16_t uls, int16_t ult, int16_t dsdx, int16_t dtdy, bool flip, const ExtendedAlignment &extAlignment)'),method(rdp,'RDP::drawTexRect(int32_t ulx, int32_t uly, int32_t lrx, int32_t lry, uint8_t tile, int16_t uls, int16_t ult, int16_t dsdx, int16_t dtdy, bool flip, const ExtendedAlignment &extAlignment)')]
    (build/'hud-rdp.inc').write_text('\n'.join(selected))
    source=(rt/'gbi/rt64_gbi_extended.cpp').read_text();(build/'hud-gbi.inc').write_text('\n'.join(method(source,'void '+n+'(') for n in ['tooieHudScopeV1','texrectV1']))
    assert 'Map[G_EX_TOOIEHUDSCOPE_V1] = &tooieHudScopeV1;' in source
    source=(rt/'hle/rt64_state.cpp').read_text();block=method(source,'if (drawStatus.isChanged(DrawAttribute::Scissor))')
    (build/'hud-scissor.inc').write_text(block[block.index('{')+1:block.rfind('}')])
    source=(rt/'render/rt64_framebuffer_renderer.cpp').read_text();(build/'hud-coordinates.inc').write_text('\n'.join(method(source,n+'(') for n in ['convertFixedRect','convertViewportRect']))
    includes=[build,build/'platform',root/'src',root/'tests',root/'tests/android_headers',root/'include',root/'lib/N64Recomp/include',root/'lib/N64ModernRuntime/ultramodern/include',root/'lib/N64ModernRuntime/thirdparty',rt,rt/'hle',rt/'render',rt/'common',rt/'contrib',rt/'contrib/hlslpp/include',rt/'contrib/json/single_include',rt/'contrib/dxc/inc',rt/'contrib/plume',root/'lib/rt64/include']
    flags=['-O1','-DNDEBUG','-D__ANDROID__','-DHLSL_CPU','-D__PRFCHWINTRIN_H','-ffunction-sections','-fdata-sections','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie',*['-I'+str(p) for p in includes]]
    cc,cxx=os.environ.get('CC','gcc'),os.environ.get('CXX','g++')
    run([cc,'-std=c11','-fwrapv',*flags,'-c',build/'guest.c','-o',build/'guest.o'],'compile-guest')
    files=[root/'tests/hud_placement.cpp',root/'tests/hud_render_consumers.cpp',rt/'hle/rt64_framebuffer_pair.cpp',rt/'hle/rt64_projection.cpp',rt/'common/rt64_common.cpp',rt/'hle/rt64_framebuffer_changes.cpp',rt/'hle/rt64_framebuffer_storage.cpp',rt/'common/rt64_enhancement_configuration.cpp']
    objects=[build/'guest.o']
    for i,p in enumerate(files):
        obj=build/f'support-{i}.o';run([cxx,'-std=c++20',*flags,'-c',p,'-o',obj],f'compile-{i}');objects.append(obj)
    native=(root/'src/hud_placement.cpp').read_text()
    timer_guard='if(entry==table+11*stride&&(word(r,component+4)>>16)==14U)return;'
    assert native.count(timer_guard)==1
    for label,text in [('current',native),('animated-anchor',native.replace('half(r,entry)','half(r,entry+6)')),
                       ('unprotected-timer',native.replace(timer_guard,''))]:
        p=build/(label+'.cpp');p.write_text(text);obj=build/(label+'.o');binary=build/label
        run([cxx,'-std=c++20',*flags,'-c',p,'-o',obj],'compile-'+label)
        run([cxx,'-fsanitize=address,undefined','-no-pie',*objects,obj,'-Wl,--gc-sections','-o',binary],'link-'+label)
        run([binary],'execute-'+label,0 if label=='current' else 83)
    run([build/'current','--drop-scope'],'execute-no-metadata',83)
    print('PASS: omitted metadata and selecting anchors from animated X each fail the real renderer-position regression.',flush=True)
    print('PASS: restoring the old countdown scope breaks its complete working-Original command/placement equivalence.',flush=True)
    print('Scope: controlled component drawing/resource services, actual guest/component/GBI/RDP/RSP and coordinate consumers; pictured Odin item identities and visual acceptance remain pending.',flush=True)


if __name__=='__main__':main()
