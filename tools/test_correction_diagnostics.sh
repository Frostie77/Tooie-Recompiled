#!/usr/bin/env bash
set -euo pipefail
r=$(cd "$(dirname "$0")/.." && pwd)
t=$(mktemp -d);trap 'rm -rf "$t"' EXIT
rt="$r/lib/rt64/src"
python3 - "$r" "$t" <<'PY'
from pathlib import Path
import os,subprocess,sys,tomllib,re
r,t=map(Path,sys.argv[1:]);source=(r/'lib/rt64/src/hle/rt64_rsp.cpp').read_text();methods=[]
for name in ['RSP','reset','clearExtended','maskPhysicalAddress','fromSegmented','fromSegmentedMasked','matrixCommon','matrix','popMatrix']:
 needle='RSP::'+name+'(';start=source.rfind('\n',0,source.index(needle))+1;brace=source.index('{',start);depth=1;end=brace+1
 while depth:
  depth+=(source[end]=='{')-(source[end]=='}');end+=1
 methods.append(source[start:end])
(t/'correction-rsp-methods.inc').write_text('\n'.join(methods))
# Extract the real HLE dispatch loop and actual extended NOOP consumer too.
for filename,signature,target in [('hle/rt64_interpreter.cpp','void Interpreter::processDisplayLists(', 'correction-interpreter.inc'),
                                  ('gbi/rt64_gbi_extended.cpp','void noOp(', 'correction-noop.inc')]:
 source=(r/'lib/rt64/src'/filename).read_text();start=source.rfind('\n',0,source.index(signature))+1;brace=source.index('{',start);depth=1;end=brace+1
 while depth:
  depth+=(source[end]=='{')-(source[end]=='}');end+=1
 body=source[start:end].replace('Interpreter::','TraceInterpreter::')
 (t/target).write_text(body)

workload=(r/'lib/rt64/src/hle/rt64_workload.cpp').read_text();start=workload.index('    void Workload::resetDrawData()')
end=workload.index('    void Workload::resetDrawDataRanges()',start)
(t/'correction-workload-reset.inc').write_text(workload[start:end])
# Check both declarations and the exact generated function scope. Overlay VRAM
# addresses repeat between functions, so searching by address alone is invalid.
config=tomllib.loads((r/'banjotooie.us.toml').read_text());count=0
for h in config['patches']['hook']:
 if 'tooie_correction_probe' not in h['text']:continue
 count+=1;name=h['func'];address=h['before_vram'];found=[]
 for p in (r/'RecompiledFuncs').glob('funcs_*.c'):
  match=re.search(r'^RECOMP_FUNC void '+re.escape(name)+r'\(.*?(?=^RECOMP_FUNC |\Z)',p.read_text(),re.M|re.S)
  if match:found.append(match.group())
 assert len(found)==1,name
 call=re.search(r'tooie_correction_probe\([^;]+;',h['text']).group()
 assert call in found[0] and '#if TOOIE_CORRECTION_TRACE_ENABLED' in h['text'],name
 after=found[0].split(call,1)[1]
 after=re.sub(r'#if TOOIE_SKELETAL_POSE_ENABLED\n\s*tooie_rt64_skeleton_builder\([^;]+;\n#endif\n','',after)
 assert after.lstrip().startswith('#endif\n    // 0x%08X:'%address),name
assert count==14,count
api=(r/'src/recomp_api.cpp').read_text()
for event in [0,3,4,5]:assert re.search(r'guest::probe\(rdram, ctx, '+str(event)+r',',api)
print('PASS: config/generated hook parity in exact core/overlay function scopes; 14 new hooks plus existing frame/root/bone boundaries')
# Guard against silently changing ordinary generated game work. Includes use
# the current bridge declarations; compare actual preprocessed instruction
# bodies after removing whitespace, with the observer undefined and zero.
for filename in ['funcs_0.c','funcs_1.c','funcs_24.c','funcs_25.c','funcs_30.c','funcs_77.c']:
 parent=t/filename;parent.write_bytes(subprocess.check_output(['git','-C',str(r),'show','3286c6fb1f0c6ed34664fb5fddd75aca24c7450e:RecompiledFuncs/'+filename]))
 for selector in [[],['-DTOOIE_ANDROID_CORRECTION_TRACE=0']]:
  args=[os.environ.get('CC','gcc'),'-E','-P','-DNDEBUG','-D__ANDROID__',*selector,
        '-I'+str(r/'include'),'-I'+str(r/'RecompiledFuncs'),'-I'+str(r/'lib/N64Recomp/include')]
  a=subprocess.check_output([*args,str(parent)],text=True);b=subprocess.check_output([*args,str(r/'RecompiledFuncs'/filename)],text=True)
  # The only new production hook calls are checked separately; guest instruction bodies stay exact.
  b=re.sub(r'\btooie_rt64_skeleton_builder\(rdram, ctx, [12]U\);','',b)
  assert re.sub(r'\s+',' ',a)==re.sub(r'\s+',' ',b),filename
print('PASS: all six generated C files retain exact parent instruction bodies with trace undefined/0 (preprocessed with current bridge declarations)')
PY
flags=(-std=c++20 -O1 -fwrapv -pthread -DHLSL_CPU -D__PRFCHWINTRIN_H -DNDEBUG -ffunction-sections -fdata-sections
 -fsanitize=address,undefined -fno-omit-frame-pointer -I"$t" -I"$r/src" -I"$r/tests/android_headers" -I"$r/lib/N64Recomp/include"
 -I"$rt" -I"$rt/contrib/hlslpp/include" -I"$rt/contrib" -I"$rt/contrib/plume" -I"$rt/contrib/json/single_include" -I"$rt/contrib/dxc/inc")
definitions=(-D__ANDROID__ -DTOOIE_ANDROID_CORRECTION_TRACE=1)
"${CXX:-g++}" "${flags[@]}" "${definitions[@]}" "$r/tests/correction_trace.cpp" "$r/tests/correction_rsp.cpp" \
 "$rt/common/rt64_math.cpp" "$rt/common/rt64_common.cpp" "$rt/hle/rt64_framebuffer_storage.cpp" "$rt/hle/rt64_framebuffer_changes.cpp" -Wl,--gc-sections -o "$t/source"
mkdir "$t/log"
APP_FOLDER_PATH="$t/log" ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 "$t/source"
for mode in android-off desktop-on;do
 defs=(-DTOOIE_ANDROID_CORRECTION_TRACE=1)
 if [ "$mode" = android-off ];then defs=(-D__ANDROID__ -DTOOIE_ANDROID_CORRECTION_TRACE=0);fi
 "${CXX:-g++}" "${flags[@]}" "${defs[@]}" "$r/tests/correction_trace.cpp" -o "$t/$mode"
 APP_FOLDER_PATH="$t/log" ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 "$t/$mode"
done
python3 - "$t/log/banjotooi-debug.log" <<'PY'
from pathlib import Path
import sys
lines=Path(sys.argv[1]).read_text().splitlines()
assert all(len(s)<2048 and 'pid=' in s and 'tid=' in s for s in lines)
assert any('kind=builder' in s and 'count=2 complete=1' in s for s in lines)
assert any('kind=model' in s and 'builder_match=1' in s for s in lines)
bone_matrices=[line for line in lines if 'kind=matrix' in line and 'bone=1 ' in line]
assert len(bone_matrices)==3 and ' builder=0 ' not in bone_matrices[0] and ' builder=0 ' in bone_matrices[1]
assert 'selected_list=0x80204000' in bone_matrices[2] and ' builder=0 ' not in bone_matrices[2]
assert any('kind=bone-root-float' in s and 'words=40000000/' in s for s in lines)
assert any('kind=bind' in s for s in lines) and any('kind=local' in s for s in lines)
assert any('recorded_span=6 elapsed_span=3 applied_span=6' in s for s in lines)
assert any('header_magic=80000000 crc1=12000000' in s and 'country=45 revision=01' in s for s in lines)
print('PASS: real diagnostic writer, bounded records, complete authored words and observed clock values/ROM header (synthetic RAM)')
PY
python3 "$r/tests/correction_replay.py"

# The actual parent must demonstrate both observed diagnostic coverage defects.
mkdir -p "$t/parent" "$t/selection-log"
git -C "$r" show '5db2d7f3719df6d196bf46a7a52a5e749d152956:src/correction_trace_guest.h' > "$t/parent/correction_trace_guest.h"
"${CXX:-g++}" "${flags[@]}" "${definitions[@]}" -DTOOIE_SELECTION_CURRENT=1 "$r/tests/correction_selection.cpp" -o "$t/selection"
APP_FOLDER_PATH="$t/selection-log" "$t/selection"
"${CXX:-g++}" -I"$t/parent" "${flags[@]}" "${definitions[@]}" "$r/tests/correction_selection.cpp" -o "$t/selection-parent"
for mode in static-root pair-only;do
 args=();if [ "$mode" = pair-only ];then args=(--pair-only);fi
 if APP_FOLDER_PATH="$t/selection-log" "$t/selection-parent" "${args[@]}" > "$t/$mode.log" 2>&1;then
  echo "FAIL: exact parent accepted $mode";exit 1
 else test "$?" -eq 1;fi
 rg -q '^selection failure:' "$t/$mode.log"
done
echo 'PASS: exact 5db2d7f parent fails static-root admission and second fresh nonplayer endpoint controls'
