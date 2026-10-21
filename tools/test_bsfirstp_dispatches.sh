#!/usr/bin/env bash
set -euo pipefail
r=$(cd "$(dirname "$0")/.." && pwd)
t=$(mktemp -d);trap 'rm -rf "$t"' EXIT
b=${TOOIE_RECOMP_BUILD_DIR:-$t/core}
cmake -S "$r/lib/N64Recomp" -B "$b" -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build "$b" --target N64Recomp -j"${TOOIE_TEST_JOBS:-2}"
python3 "$r/tools/bsfirstp_dispatches.py" "$r" "$t"
flags=(-std=c++20 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer
 -I"$r/lib/N64Recomp/include" -I"$r/lib/N64Recomp/src" -I"$r/lib/N64Recomp/lib/fmt/include"
 -I"$r/lib/N64Recomp/lib/rabbitizer/include" -I"$r/lib/N64Recomp/lib/rabbitizer/cplusplus/include")
"${CXX:-g++}" "${flags[@]}" "$t/generate.cpp" "$b/libN64Recomp.a" "$b/libSymbolLists.a" "$b/librabbitizer.a" "$b/lib/fmt/libfmt.a" -o "$t/generate"
export ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
"$t/generate" "$t"
apply=(--check)
if [ "${TOOIE_BSFIRSTP_APPLY:-0}" = 1 ];then apply=(--apply);fi
python3 "$r/tools/bsfirstp_dispatches.py" "$r" "$t" "${apply[@]}"
python3 - "$r" "$t" <<'PY'
from pathlib import Path
import hashlib,json,re,sys,subprocess
r,t=map(Path,sys.argv[1:]);sys.path.insert(0,str(r/'tools'))
from bsfirstp_dispatches import load,audit,PARENT
j=load(r);tables=j['tables'];groups={kind:[] for kind in ['fixed','original','reference']};decl=[]
for spec in tables:
 name=spec['function']
 for kind in groups:groups[kind].append((t/(name+'-'+kind+'.inc')).read_text())
 for n in [name,name+'_reference']:decl.append('extern "C" void '+n+'(uint8_t*,recomp_context*);')
(t/'declarations.inc').write_text('\n'.join(decl))
for kind,bodies in groups.items():(t/(kind+'.c')).write_text('#include "tooie_recomp.h"\n#include "funcs.h"\n'+'\n'.join(bodies))
for index,spec in enumerate(tables):
 body=groups['fixed'][index];targets=[int(v,16) for v in spec['raw_offsets']]
 first=targets[0];other=next(v for v in targets if v!=first)
 old=f'case 0: goto L_{0x80800000+first:08X}';assert body.count(old)==1
 wrongcase=body.replace(old,f'case 0: goto L_{0x80800000+other:08X}')
 frame=(-int(spec['code_words_be'][0],16))&0xFFFF
 old=f'ctx->r29 = ADD32(ctx->r29, 0X{frame:X});';assert old in body
 wrongframe=body.replace(old,f'ctx->r29 = ADD32(ctx->r29, 0X{frame-4:X});')
 for kind,mutated in [('case',wrongcase),('frame',wrongframe)]:
  bodies=groups['fixed'].copy();bodies[index]=mutated
  (t/(f'bad-{kind}-{index}.c')).write_text('#include "tooie_recomp.h"\n#include "funcs.h"\n'+'\n'.join(bodies))
# A physical-line change is allowed, but the previous 47-slot function itself
# must remain byte-identical after newline normalization.
sources='\n'.join((r/'RecompiledFuncs'/n).read_text() for n in ['funcs_44.c','funcs_45.c'])
parent='\n'.join(subprocess.check_output(['git','-C',str(r),'show',PARENT+':RecompiledFuncs/'+n],text=True) for n in ['funcs_44.c','funcs_45.c'])
old_functions,old_candidates=audit(parent);new_functions,new_candidates=audit(sources)
assert len(old_functions)==len(new_functions)==306
assert old_candidates==[spec['function'] for spec in tables]
assert not new_candidates
print('PASS: scoped 306-function audit detects all four exact-parent candidates and none after correction')
body=re.search(r'^RECOMP_FUNC void bsfirstp_entrypoint_26\(.*?(?=^RECOMP_FUNC |\Z)',sources,re.M|re.S)[0]
assert hashlib.sha256(body.encode()).hexdigest()==j['retained_47_slot_switch']['function_normalized_lf_sha256']
print('PASS: all 23 ROM targets and bounded inputs verified; previous complete 47-slot consumer is unchanged')
PY
common=(-O1 -g -fwrapv -fno-strict-aliasing -DNDEBUG -fsanitize=address,undefined -fno-omit-frame-pointer
 -I"$t" -I"$r/include" -I"$r/RecompiledFuncs" -I"$r/lib/N64ModernRuntime/N64Recomp/include")
for mode in android desktop;do
 defs=();if [ "$mode" = android ];then defs=(-D__ANDROID__);fi
 "${CXX:-g++}" -std=c++20 "${common[@]}" "${defs[@]}" -c "$r/tests/bsfirstp_dispatches.cpp" -o "$t/$mode-driver.o"
 for kind in fixed reference;do
  "${CC:-gcc}" -std=gnu11 "${common[@]}" "${defs[@]}" -c "$t/$kind.c" -o "$t/$mode-$kind.o"
 done
 "${CXX:-g++}" "${common[@]}" "$t/$mode-driver.o" "$t/$mode-fixed.o" "$t/$mode-reference.o" -o "$t/$mode"
 "$t/$mode"
done
"${CC:-gcc}" -std=gnu11 "${common[@]}" -D__ANDROID__ -c "$t/original.c" -o "$t/original.o"
"${CXX:-g++}" "${common[@]}" "$t/android-driver.o" "$t/original.o" "$t/android-reference.o" -o "$t/parent"
for index in 0 1 2 3;do
 if "$t/parent" "$index" > "$t/parent-$index.log" 2>&1;then echo 'FAIL: parent dispatch survived';exit 1;else test "$?" -eq 18;fi
 rg -q '^EXPECTED parent lookup:' "$t/parent-$index.log"
 for kind in case frame;do
  "${CC:-gcc}" -std=gnu11 "${common[@]}" -D__ANDROID__ -c "$t/bad-$kind-$index.c" -o "$t/mutant.o"
  "${CXX:-g++}" "${common[@]}" "$t/android-driver.o" "$t/mutant.o" "$t/android-reference.o" -o "$t/mutant"
  if "$t/mutant" > "$t/bad-$kind-$index.log" 2>&1;then echo 'FAIL: mutation survived';exit 1;else test "$?" -eq 3;fi
  rg -q '^bsfirstp failure' "$t/bad-$kind-$index.log"
 done
done
echo 'PASS: all four exact-parent misses and eight wrong-case/damaged-frame mutations fail at the real consumers'
