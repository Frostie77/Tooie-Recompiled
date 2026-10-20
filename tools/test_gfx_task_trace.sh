#!/usr/bin/env bash
set -euo pipefail
r=$(cd "$(dirname "$0")/.." && pwd)
t=$(mktemp -d);trap 'rm -rf "$t"' EXIT
python3 - "$r" "$t" <<'PY'
from pathlib import Path
import subprocess,sys
r,t=map(Path,sys.argv[1:]);base='121787042b50b3250036bc6013b501c737e4d4ac'
paths=['lib/N64ModernRuntime/ultramodern/src/events.cpp','lib/RecompFrontend/recompui/src/renderer/rt64_render_context.cpp']
for profile in ('current','parent','bad-copy','bad-boundary','bad-order','bad-pose'):
 sources=[subprocess.check_output(['git','-C',str(r),'show',base+':'+p],text=True) if profile=='parent' else (r/p).read_text() for p in paths]
 out=t/profile;out.mkdir();s=sources[0]
 a=s.index('struct SpTaskAction');b=s.index('struct ViState',a);(out/'action-types.inc').write_text(s[a:b])
 a=s.index('void ultramodern::submit_rsp_task(');b=s.index('\nvoid ultramodern::send_si_message()',a);body=s[a:b]
 if profile=='bad-copy':
  old='SpTaskAction action{ *task };';assert body.count(old)==1;body=body.replace(old,'SpTaskAction action{};')
 if profile=='bad-pose':
  old='action.skeletal = tooie::skeletal::mailbox().snapshot();';assert body.count(old)==1;body=body.replace(old,'/* lost owned task snapshot */')
 (out/'submit.inc').write_text(body)
 a=s.index('void gfx_thread_func(');b=s.index('\n#define VI_CTRL_TYPE_16',a);body=s[a:b]
 if profile=='bad-order':
  assert body.count('                sp_complete();')==1;body=body.replace('                sp_complete();','')
  body=body.replace('                dp_complete();','                dp_complete();\n                sp_complete();')
 (out/'gfx-thread.inc').write_text(body)
 s=sources[1];a=s.index('void renderer::RT64Context::send_dl(');b=s.index('\nvoid renderer::RT64Context::send_dummy_workload(',a);body=s[a:b]
 if profile=='bad-boundary':
  assert body.count('    trace.boundary(0);')==1;body=body.replace('    trace.boundary(0);','/* missing texture boundary */')
 (out/'send-dl.inc').write_text(body)
# Compare all edited callable bodies and the private action layout with the
# exact parent after preprocessing disabled/default/desktop configurations.
import os,re
for defs in ([],['-D__ANDROID__'],['-D__ANDROID__','-DTOOIE_ANDROID_TIMING_TRACE=0'],['-DTOOIE_ANDROID_TIMING_TRACE=1']):
 outputs=[]
 for profile in ('current','parent'):
  out=t/profile;path=t/(profile+'-bodies.cpp')
  path.write_text(''.join((out/n).read_text() for n in ('action-types.inc','submit.inc','gfx-thread.inc','send-dl.inc')))
  outputs.append(subprocess.check_output([os.environ.get('CXX','g++'),'-E','-P','-x','c++',*defs,'-include',str(r/'include/timing_trace_config.h'),'-include',str(r/'include/skeletal_pose_config.h'),str(path)],text=True))
 assert re.sub(r'\s+',' ',outputs[0])==re.sub(r'\s+',' ',outputs[1]),defs
print('PASS: full edited runtime/renderer bodies and private action layout equal exact parent in default/zero/desktop profiles')
PY
flags=(-std=c++20 -O1 -g -pthread -fsanitize=address,undefined -fno-omit-frame-pointer
 -I"$r/src" -I"$r/include" -I"$r/tests/android_headers" -I"$r/lib/N64ModernRuntime/ultramodern/include"
 -I"$r/lib/N64ModernRuntime/thirdparty/concurrentqueue" -I"$r/lib/N64ModernRuntime/thirdparty")
export ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1
for profile in timing default zero desktop parent bad-copy bad-boundary bad-order bad-pose;do
 source=current;defs=(-D__ANDROID__)
 case "$profile" in timing)defs+=(-DTOOIE_ANDROID_TIMING_TRACE=1);;zero)defs+=(-DTOOIE_ANDROID_TIMING_TRACE=0);;desktop)defs=(-DTOOIE_ANDROID_TIMING_TRACE=1);;parent|bad-copy|bad-boundary|bad-order|bad-pose)source=$profile;defs+=(-DTOOIE_ANDROID_TIMING_TRACE=1);;esac
 if [ "$profile" = parent ];then defs+=(-DTEST_PARENT=1);fi
 "${CXX:-g++}" "${flags[@]}" "${defs[@]}" -I"$t/$source" "$r/tests/gfx_task_trace.cpp" "$r/tests/gfx_task_trace_peer.cpp" -o "$t/test"
 for mode in {0..7};do
  mkdir "$t/$profile-$mode"
  if APP_FOLDER_PATH="$t/$profile-$mode" "$t/test" "$mode" >"$t/$profile-$mode.txt" 2>&1;then
   if { [ "$profile" = bad-copy ] || [ "$profile" = bad-order ] || [ "$profile" = bad-pose ]; } && [ "$mode" != 6 ];then echo "FAIL: accepted $profile";exit 1;fi
  else
   test "$?" -eq 1
   if { [ "$profile" = bad-copy ] || [ "$profile" = bad-order ] || [ "$profile" = bad-pose ]; } && [ "$mode" != 6 ];then rg -q '^task failure:' "$t/$profile-$mode.txt";continue;fi
   cat "$t/$profile-$mode.txt";exit 1
  fi
 done
done
python3 - "$t" <<'PY'
from pathlib import Path
import re,sys
t=Path(sys.argv[1])
def records(profile,mode,domain):
 p=t/f'{profile}-{mode}/banjotooi-debug.log'
 return [l for l in p.read_text().splitlines() if l.startswith('Runtime '+domain+':')] if p.exists() else []
def maximum(profile,mode,domain,stage):
 return max(int(re.search(r'\b'+stage+r'=\d+/\d+/(\d+)/',l)[1]) for l in records(profile,mode,domain))
for mode in range(8):
 assert len({(t/f'{p}-{mode}.txt').read_text() for p in ('timing','default','zero','desktop','parent')})==1
 for profile in ('default','zero','desktop','parent'):
  for domain in ('gfx-submit','gfx-dispatch','display-list'):assert not records(profile,mode,domain)
 if mode==6:
  assert not records('timing',mode,'gfx-submit');continue
 for domain in ('gfx-submit','gfx-dispatch','display-list'):
  rows=records('timing',mode,domain);assert len(rows)>1
  assert all(len(l)<2048 and ' pid=' in l and ' tid=' in l and 'task=' in l for l in rows)
  assert all('invalid=0' in l for l in rows) if domain!='display-list' else all('correlation_valid=1' in l for l in rows)
  if domain!='display-list':assert int(re.search(r'current_calls=(\d+)',rows[-1])[1])>16
 assert maximum('timing',mode,'gfx-submit','prepare')==810000
 assert maximum('timing',mode,'gfx-submit','enqueue')==(5000 if mode==7 else 5)
 assert maximum('timing',mode,'gfx-dispatch','submit_age')==(811412 if mode==1 else 5005 if mode==7 else 5)
 assert maximum('timing',mode,'gfx-dispatch','prefix')==(810002 if mode==2 else 13)
 assert maximum('timing',mode,'gfx-dispatch','completion')==(810002 if mode==5 else 25)
 assert maximum('timing',mode,'display-list','texture_actions')==(810000 if mode==3 else 7)
 assert maximum('timing',mode,'display-list','interpret')==(810000 if mode==4 else 19)
 assert maximum('bad-boundary',mode,'display-list','texture_actions')!=maximum('timing',mode,'display-list','texture_actions')
 # All first slow events share the unique copied task identity across scopes.
 domains=[records('timing',mode,d)[0] for d in ('gfx-submit','gfx-dispatch','display-list')]
 assert len({re.search(r'(?:peak_)?task=(\d+)',l)[1] for l in domains})==1
print('PASS: 40 actual-source queue/graphics/renderer cases match exact parent; enqueue age, upstream gaps, callbacks, texture/interpret stages, task-copy and skeletal-snapshot lifetime, cross-TU/nested correlation, all action types, failed setup and late reports; exact-parent absence and copy/boundary/order/ownership controls detected')
PY
