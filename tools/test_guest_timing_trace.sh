#!/usr/bin/env bash
set -euo pipefail
r=$(cd "$(dirname "$0")/.." && pwd)
t=$(mktemp -d);trap 'rm -rf "$t"' EXIT
python3 - "$r" "$t" <<'PY'
from pathlib import Path
import re,subprocess,sys
r,t=map(Path,sys.argv[1:]);base='d40c4e150d2372fbdfe82681078575b4dd47fd2c'
names=['glrecord_entrypoint_0','func_800A7D30','func_80014F4C',
 'func_800D8FA0','func_800D8F50','func_800D8F78','func_800D9004','func_800D8FF8','func_80018444',
 'func_800F0D90','func_800F0DB8','static_3_800F0DA8',
 'func_8001608C','func_800A9CAC','func_800A9CC8','func_800A9CDC','func_800DC0C0','func_80015D60',
 'func_800C95D4','func_800C91C8','func_800167F4','func_80015FA0','func_80015FC8',
 'gsattract_entrypoint_0','gsattract_entrypoint_1',
 'func_80015178','func_80015184','func_80014F38','func_800151BC','func_80015410','func_8001546C','func_80014A54']
files=['funcs_0.c','funcs_1.c','funcs_24.c','funcs_28.c','funcs_30.c','funcs_82.c','funcs_33.c','funcs_77.c']
for profile in ('current','parent','bad-frame','bad-timer'):
 sources=[subprocess.check_output(['git','-C',str(r),'show',base+':RecompiledFuncs/'+f],text=True) if profile=='parent' else (r/'RecompiledFuncs'/f).read_text() for f in files]
 bodies=[]
 for name in names:
  found=[m[0] for s in sources if (m:=re.search(r'^RECOMP_FUNC void '+name+r'\(.*?(?=^RECOMP_FUNC |\Z)',s,re.M|re.S))]
  assert len(found)==1,(name,len(found));body=found[0]
  if profile=='bad-frame' and name=='func_800151BC':
   old='MEM_H(-0X709C, ctx->r1) = ctx->r14;';assert body.count(old)==1;body=body.replace(old,'MEM_H(-0X709C, ctx->r1) = ADD32(ctx->r14, 1);')
  if profile=='bad-timer' and name=='func_8001608C':
   old='func_800D8FA0(rdram, ctx);';assert body.count(old)==1;body=body.replace(old,'/* missing recorded-span override */')
  bodies.append(body)
 (t/(profile+'.c')).write_text('#include "tooie_recomp.h"\n#include "funcs.h"\nvoid osDpGetStatus_recomp(uint8_t*,recomp_context*);\nvoid osYieldThread_recomp(uint8_t*,recomp_context*);\n'+''.join(bodies))
 # Use the real paired input call site for the retained sparse input fixture;
 # direct calls would intentionally bypass its before/after observer hooks.
 main=sources[0];a=main.index('L_80012458:');b=main.index('    // 0x80012460:',a)
 block=main[a:b]
 assert 'after_4:' in block
 (t/(profile+'-input.inc')).write_text(block)
 prime=sources[2];a=prime.index('    after_2:',prime.index('RECOMP_FUNC void func_800A9924('));a=prime.index('\n',a)+1;b=prime.index('    // 0x800A9988:',a)
 (t/(profile+'-prime.inc')).write_text(prime[a:b])
# Compare complete modified TUs, with current bridge declarations in both
# sources. Even the timing-only profile may add only observer calls.
import os
for f in ['funcs_0.c','funcs_1.c','funcs_24.c','funcs_77.c']:
 parent=t/('parent-'+f);parent.write_bytes(subprocess.check_output(['git','-C',str(r),'show',base+':RecompiledFuncs/'+f]))
 for defs in [[],['-D__ANDROID__'],['-D__ANDROID__','-DTOOIE_ANDROID_TIMING_TRACE=0'],['-D__ANDROID__','-DTOOIE_ANDROID_TIMING_TRACE=1'],['-DTOOIE_ANDROID_TIMING_TRACE=1']]:
  flags=[os.environ.get('CC','gcc'),'-E','-P','-DNDEBUG',*defs,'-I'+str(r/'include'),'-I'+str(r/'RecompiledFuncs'),'-I'+str(r/'lib/N64Recomp/include')]
  a=subprocess.check_output([*flags,str(parent)],text=True)
  b=subprocess.check_output([*flags,str(r/'RecompiledFuncs'/f)],text=True)
  b=re.sub(r'\btooie_correction_probe\(rdram, ctx, [0-9]+U, 0x[0-9A-F]+U\);','',b)
  b=re.sub(r'\btooie_demo_pacing\(rdram, ctx, [0-3]U\);','',b)
  assert re.sub(r'\s+',' ',a)==re.sub(r'\s+',' ',b),(f,defs)
print('PASS: complete generated instruction bodies equal exact parent across default/zero/timing and desktop profiles (observer calls excluded only when enabled)')
s=(r/'src/recomp_api.cpp').read_text();a=s.index('extern "C" void tooie_correction_probe(');b=s.index('\n}\n',a)+3
(t/'bridge.cpp').write_text('#include "tooie_recomp.h"\n#include "correction_trace_guest.h"\n#include "demo_pacing_guest.h"\n'+s[a:b]+'\nextern "C" void tooie_demo_pacing(uint8_t *ram,recomp_context *ctx,uint32_t event) {tooie::demo::event(ram,ctx,event); }\n')
PY
python3 "$r/tests/correction_clock_records.py" "${TOOIE_CAPTURED_CLOCK_FIXTURE:-$r/tests/fixtures/odin-step86-clocks.txt}" "$t/captured_clocks.h"
flags=(-O1 -g -fwrapv -fsanitize=address,undefined -fno-omit-frame-pointer -I"$r/include" -I"$r/src" -I"$r/tests" -I"$t" -I"$r/RecompiledFuncs" -I"$r/lib/N64Recomp/include")
export ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1
for profile in android-default android-zero android-timing android-legacy desktop-timing parent bad-frame bad-timer;do
 defs=(-D__ANDROID__);source=current
 case "$profile" in
 android-zero) defs+=(-DTOOIE_ANDROID_TIMING_TRACE=0 -DTOOIE_ANDROID_CORRECTION_TRACE=0);;
 android-timing|parent|bad-frame|bad-timer) defs+=(-DTOOIE_ANDROID_TIMING_TRACE=1 -DTOOIE_ANDROID_CORRECTION_TRACE=0);;
 android-legacy) defs+=(-DTOOIE_ANDROID_CORRECTION_TRACE=1);;
 desktop-timing) defs=(-DTOOIE_ANDROID_TIMING_TRACE=1 -DTOOIE_ANDROID_CORRECTION_TRACE=1);;
 esac
 case "$profile" in parent|bad-frame|bad-timer) source=$profile;;esac
 if [ "$profile" = parent ];then defs+=(-DTOOIE_GUEST_TIMING_PARENT=1);fi
 cp "$t/$source-input.inc" "$t/input-call.inc"
 cp "$t/$source-prime.inc" "$t/prime-call.inc"
 "${CC:-gcc}" -std=c11 "${flags[@]}" "${defs[@]}" -c "$t/$source.c" -o "$t/generated.o"
 "${CC:-gcc}" -std=c11 "${flags[@]}" "${defs[@]}" -Dmain=retained_clock_main -Dfunc_8001608C=timed_input -c "$r/tests/demo_replay_captured.c" -o "$t/captured.o"
 "${CC:-gcc}" -std=c11 "${flags[@]}" "${defs[@]}" -c "$r/tests/demo_replay_adapters.c" -o "$t/adapters.o"
 "${CXX:-g++}" -std=c++20 "${flags[@]}" "${defs[@]}" "$r/tests/guest_frame_sync.cpp" "$t/bridge.cpp" "$t/generated.o" "$t/captured.o" "$t/adapters.o" -lm -o "$t/test"
 mkdir "$t/$profile"
 if APP_FOLDER_PATH="$t/$profile" "$t/test" >"$t/$profile.txt" 2>&1;then
  case "$profile" in parent|bad-frame|bad-timer) echo "FAIL: accepted $profile";exit 1;;esac
 else
  rc=$?;test "$rc" -eq 1 || { cat "$t/$profile.txt";exit 1; }
  case "$profile" in parent|bad-frame|bad-timer) rg -q '^frame-sync failure:' "$t/$profile.txt";continue;;esac
  cat "$t/$profile.txt";exit 1
 fi
 rg '^STATE|^PASS' "$t/$profile.txt"
done
python3 - "$t" <<'PY'
from pathlib import Path
import re,sys
t=Path(sys.argv[1]);hashes=set()
for profile in ['android-default','android-zero','android-timing','android-legacy','desktop-timing']:
 row=re.search(r'^STATE.*', (t/(profile+'.txt')).read_text(),re.M)[0]
 hashes.add(re.search(r'ordinary_hash=([0-9a-f]+)',row)[1])
 log=t/profile/'banjotooi-debug.log';text=log.read_text() if log.exists() else ''
 if profile in ('android-timing','android-legacy'):
  assert 'Runtime correction-clock-event:' in text and 'Runtime correction-clock:' in text
  assert all(len(line)<2048 and ' pid=' in line and ' tid=' in line for line in text.splitlines())
 else: assert not text,(profile,text)
 if profile=='android-timing':
  assert text.count('Runtime guest-frame-sync:')>=2 and 'max_us=800000' in text
  assert 'kind=builder' not in text and 'Runtime correction-source' not in text
assert len(hashes)==1,hashes
print('PASS: identical full RAM/register digest for Android default/zero/timing/legacy and desktop; real clock-only writer, late frame-sync report; exact parent lacks clock-only hooks; altered frame count/replay override rejected')
PY
