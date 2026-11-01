#!/usr/bin/env python3
"""Run actual guest visibility wrappers and primitive tests with strict controls."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile
import tomllib


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--build-dir',type=Path);args=parser.parse_args()
    root=Path(__file__).resolve().parents[1]
    if args.build_dir:args.build_dir.mkdir(parents=True,exist_ok=True)
    build=Path(tempfile.mkdtemp(prefix='tooie-visibility-',dir=args.build_dir))
    fixture=json.loads((root/'tests/fixtures/visibility-consumers-parent.json').read_text());bodies={};parents={}
    stripped=r'^    (?:tooie_expand_visibility_scope\(rdram, ctx, [01]U\);|if \(tooie_expand_frustum\(rdram, ctx, [012]U\)\) return;)\n'
    for name,row in fixture['functions'].items():
        body=re.search(r'^RECOMP_FUNC void '+name+r'\(.*?(?=^RECOMP_FUNC |\Z)',(root/row['file']).read_text(),re.M|re.S).group()
        canonical=re.sub(stripped,'',body,flags=re.M)
        assert hashlib.sha256(canonical.encode()).hexdigest()==row['canonical_sha256'],name
        bodies[name]=body;parents[name]=canonical
    cfg=tomllib.loads((root/'banjotooie.us.toml').read_text());hooks=[h for h in cfg['patches']['hook'] if 'tooie_expand_' in h['text']];assert len(hooks)==9
    for h in hooks:
        assert '    '+h['text'].strip()+f'\n    // 0x{h["before_vram"]:08X}:' in bodies[h['func']]
    for name,pc in [('func_800E3D0C',0x800E3DB8),('func_800E3DC0',0x800E3E84),('func_800E3E8C',0x800E3EF4)]:
        needle=f'    // 0x{pc:08X}:';assert parents[name].count(needle)==1
        parents[name]=parents[name].replace(needle,'    tooie_expand_visibility(rdram, ctx);\n'+needle)
        assert hashlib.sha256(parents[name].encode()).hexdigest()==fixture['functions'][name]['old_sha256']
    transition=json.loads((root/'tests/fixtures/transition-scale-parent.json').read_text())
    name=transition['function']
    body=re.search(r'^RECOMP_FUNC void '+name+r'\(.*?(?=^RECOMP_FUNC |\Z)',(root/transition['file']).read_text(),re.M|re.S).group()
    canonical=body.replace('    tooie_transition_coverage(rdram, ctx);\n','')
    assert hashlib.sha256(canonical.encode()).hexdigest()==transition['canonical_sha256']
    hook=[h for h in cfg['patches']['hook'] if h['func']==name and h['before_vram']==int(transition['before_vram'],16)]
    assert len(hook)==1 and hook[0]['text']=='tooie_transition_coverage(rdram, ctx);'
    assert '    '+hook[0]['text']+'\n    // '+transition['before_vram']+':' in body
    bodies[name]=body;parents[name]=canonical
    current=''.join(bodies.values());parent=''.join(parents.values())
    for name in sorted(bodies,key=len,reverse=True):parent=re.sub(r'\b'+name+r'\b','parent_'+name,parent)
    parent=parent.replace('tooie_expand_visibility(rdram, ctx);','parent_expand_visibility(rdram, ctx);')
    calls=set(re.findall(r'\b(\w+)\(rdram, ctx\);',current+parent))
    header='#include "tooie_recomp.h"\n'+''.join('void '+n+'(uint8_t*,recomp_context*);\n' for n in sorted(calls) if n not in {'tooie_expand_frustum','tooie_expand_visibility_scope'})
    (build/'guest.c').write_text(header+current+parent)
    assert 'tooie::visibility::publishAspect(workloadConfig.aspectRatioSource, workloadConfig.aspectRatioTarget);' in (root/'lib/rt64/src/hle/rt64_workload_queue.cpp').read_text()
    assert '${CMAKE_SOURCE_DIR}/src/widescreen_visibility.cpp' in (root/'CMakeLists.txt').read_text()
    includes=[root/'include',root/'src',root/'lib/N64Recomp/include',root/'lib/N64ModernRuntime/ultramodern/include',root/'lib/N64ModernRuntime/thirdparty']
    flags=['-O1','-g1','-fwrapv','-D__ANDROID__','-fno-pie','-fsanitize=address,undefined','-fno-omit-frame-pointer',*['-I'+str(p) for p in includes]]
    env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1:print_stacktrace=1'}
    def run(cmd,label,expected=0):
        p=subprocess.run([str(x) for x in cmd],cwd=root,env=env,capture_output=True,text=True,timeout=180);output=p.stdout+p.stderr;(build/(label+'.log')).write_text(output)
        if p.returncode!=expected or 'runtime error:' in output or 'ERROR: AddressSanitizer' in output:
            raise RuntimeError(f'{label}: exit {p.returncode}, expected {expected}\n{output[-5000:]}\nLogs: {build}')
        if expected:assert 'visibility failure' in output,output
        for line in output.splitlines():
            if line.startswith('PASS:'):print(line,flush=True)
    cc,cxx=os.environ.get('CC','gcc'),os.environ.get('CXX','g++');obj=build/'guest.o';binary=build/'test'
    run([cc,'-std=c11',*flags,'-c',build/'guest.c','-o',obj],'compile')
    test=build/'test.o';visibility=build/'visibility.o';coverage=build/'coverage.o'
    run([cxx,'-std=c++20',*flags,'-c',root/'tests/widescreen_visibility.cpp','-o',test],'compile-test')
    run([cxx,'-std=c++20',*flags,'-c',root/'src/widescreen_visibility.cpp','-o',visibility],'compile-visibility')
    run([cxx,'-std=c++20',*flags,'-c',root/'src/transition_coverage.cpp','-o',coverage],'compile-coverage')
    run([cxx,*flags,test,visibility,coverage,obj,'-no-pie','-o',binary],'link')
    run([binary],'current');run([binary,'--old-uncull'],'old-control',84);run([binary,'--no-widen'],'no-width-control',84)
    old_gate=build/'old-gate.cpp'
    source=(root/'src/widescreen_visibility.cpp').read_text()
    needle='ultramodern::renderer::uses_widened_view(ultramodern::renderer::get_graphics_config().ar_option)'
    assert source.count(needle)==1
    old_gate.write_text(source.replace(needle,'ultramodern::renderer::get_graphics_config().ar_option==ultramodern::renderer::AspectRatio::Expand'))
    run([cxx,'-std=c++20',*flags,'-c',old_gate,'-o',build/'old-gate.o'],'compile-old-gate')
    run([cxx,*flags,test,build/'old-gate.o',coverage,obj,'-no-pie','-o',build/'old-gate'],'link-old-gate')
    run([build/'old-gate'],'old-preset-gate-control',84)
    missing=build/'missing-coverage.cpp'
    missing.write_text('#include "tooie_recomp.h"\nextern "C" void tooie_transition_coverage(uint8_t*,recomp_context*) {}\n')
    run([cxx,'-std=c++20',*flags,'-c',missing,'-o',build/'missing-coverage.o'],'compile-missing-coverage')
    run([cxx,*flags,test,visibility,build/'missing-coverage.o',obj,'-no-pie','-o',build/'missing-coverage'],'link-missing-coverage')
    run([build/'missing-coverage'],'missing-transition-coverage-control',84)
    print('PASS: exact prior unconditional uncull fails rear/vertical visibility; disabling horizontal widening fails the newly visible side region.',flush=True)
    print('PASS: restoring the Expand-only gate fails a named fixed ratio; omitting transition expansion fails the real guest draw-scale handoff.',flush=True)
    print('Scope: constructed camera/bounds and original transition helper through actual guest consumers; animation/model geometry and final pixels remain device checks.',flush=True)


if __name__=='__main__':main()
