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
    run([cxx,'-std=c++20',*flags,root/'tests/widescreen_visibility.cpp',root/'src/widescreen_visibility.cpp',obj,'-no-pie','-o',binary],'link')
    run([binary],'current');run([binary,'--old-uncull'],'old-control',84);run([binary,'--no-widen'],'no-width-control',84)
    print('PASS: exact prior unconditional uncull fails rear/vertical visibility; disabling horizontal widening fails the newly visible side region.',flush=True)
    print('Scope: constructed camera/bounds with complete guest visibility consumers; no causal replay or GPU/device acceptance of the lagoon water report.',flush=True)


if __name__=='__main__':main()
