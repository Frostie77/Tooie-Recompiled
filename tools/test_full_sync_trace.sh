#!/usr/bin/env bash
set -euo pipefail
r=$(cd "$(dirname "$0")/.." && pwd)
t=$(mktemp -d);trap 'rm -rf "$t"' EXIT
python3 - "$r" "$t" <<'PY'
from pathlib import Path
import re,subprocess,sys
r,t=map(Path,sys.argv[1:]);base='389c495511299f9ffc758625d38620ab025a558d'
for profile in ('current','parent','bad-observer','bad-publication'):
 s=(r/'lib/rt64/src/hle/rt64_state.cpp').read_text() if profile!='parent' else subprocess.check_output(['git','-C',str(r),'show',base+':lib/rt64/src/hle/rt64_state.cpp'],text=True)
 a=s.index('        // Advance the workload queue at the end of a full synchronization.')
 b=s.index('    void State::updateScreen(',a)
 body=s[a:b]
 if profile=='bad-observer':
  assert body.count('fullSyncTrace.prepared();')==1;body=body.replace('fullSyncTrace.prepared();','/* removed preparation boundary */')
 if profile=='bad-publication':
  assert body.count('if (!fbPair.earlyPresentCandidate()) {')==1;body=body.replace('if (!fbPair.earlyPresentCandidate()) {','if (false) {')
 (t/profile).mkdir();(t/profile/'full-sync-tail.inc').write_text(body)
 s=(r/'lib/rt64/src/hle/rt64_framebuffer_pair.cpp').read_text();a=s.index('    bool FramebufferPair::earlyPresentCandidate()');b=s.index('\n};',a)
 (t/profile/'candidate.inc').write_text(s[a:b])
 # Normal/desktop preprocessed fullSync is byte-identical to the parent
 # after whitespace normalization. No new reads or extra candidate checks.
 if profile in ('current','parent'):
  s=(r/'lib/rt64/src/hle/rt64_state.cpp').read_text() if profile=='current' else subprocess.check_output(['git','-C',str(r),'show',base+':lib/rt64/src/hle/rt64_state.cpp'],text=True)
  a=s.index('    void State::fullSync()');b=s.index('    void State::updateScreen(',a)
  (t/(profile+'.cpp')).write_text(s[a:b])
(t/'old-trace.h').write_text(subprocess.check_output(['git','-C',str(r),'show','121787042b50b3250036bc6013b501c737e4d4ac:src/full_sync_trace.h'],text=True).replace('../include/timing_trace_config.h',str(r/'include/timing_trace_config.h')))
import os
for defs in ([],['-D__ANDROID__'],['-D__ANDROID__','-DTOOIE_ANDROID_TIMING_TRACE=0'],['-DTOOIE_ANDROID_TIMING_TRACE=1']):
 flags=[os.environ.get('CXX','g++'),'-E','-P','-x','c++',*defs,'-include',str(r/'include/timing_trace_config.h')]
 a=subprocess.check_output([*flags,str(t/'parent.cpp')],text=True);b=subprocess.check_output([*flags,str(t/'current.cpp')],text=True)
 assert re.sub(r'\s+',' ',a)==re.sub(r'\s+',' ',b),defs
print('PASS: complete fullSync default/zero/desktop instruction bodies equal exact parent')
PY
flags=(-std=c++17 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -I"$r/src")
export ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1
for profile in timing default zero desktop parent parent-window bad-observer bad-publication;do
 defs=(-D__ANDROID__);source=current
 case "$profile" in timing) defs+=(-DTOOIE_ANDROID_TIMING_TRACE=1);;parent-window) defs+=(-DTOOIE_ANDROID_TIMING_TRACE=1 "-DTEST_FULL_SYNC_TRACE_HEADER=\"$t/old-trace.h\"");;zero) defs+=(-DTOOIE_ANDROID_TIMING_TRACE=0);;desktop) defs=(-DTOOIE_ANDROID_TIMING_TRACE=1);;parent|bad-observer|bad-publication) defs+=(-DTOOIE_ANDROID_TIMING_TRACE=1);source=$profile;;esac
 if [ "$profile" = parent ];then defs+=(-DTEST_PARENT=1);fi
 "${CXX:-g++}" "${flags[@]}" "${defs[@]}" -I"$t/$source" "$r/tests/full_sync_trace.cpp" -o "$t/test"
 for mode in {0..9};do
  mkdir "$t/$profile-$mode"
  if APP_FOLDER_PATH="$t/$profile-$mode" "$t/test" "$mode" >"$t/$profile-$mode.txt" 2>&1;then
   if [ "$profile" = bad-publication ] && { [ "$mode" = 3 ] || [ "$mode" = 7 ]; };then echo 'FAIL: accepted unsafe candidate';exit 1;fi
  else
   test "$?" -eq 1
   if [ "$profile" = bad-publication ] && { [ "$mode" = 3 ] || [ "$mode" = 7 ]; };then rg -q '^full-sync failure:' "$t/$profile-$mode.txt";continue;fi
   cat "$t/$profile-$mode.txt";exit 1
  fi
 done
done
python3 - "$t" <<'PY'
from pathlib import Path
import re,sys
t=Path(sys.argv[1])
for mode in range(10):
 assert len({(t/f'{p}-{mode}.txt').read_text() for p in ('timing','default','zero','desktop','parent')})==1
 for p in ('default','zero','desktop','parent'):assert not (t/f'{p}-{mode}/banjotooi-debug.log').exists()
 text=(t/f'timing-{mode}/banjotooi-debug.log').read_text();lines=text.splitlines();assert len(lines)>=2
 result=4 if mode in (0,8) else 0 if mode in (1,9) else 1 if mode==2 else 2 if mode in (3,7) else 3
 assert all(f'peak_result={result} ' in line and len(line)<2048 and ' pid=' in line and ' tid=' in line for line in lines)
 assert all('task=1234 correlation_valid=1' in line for line in lines)
 def first_sum(profile,stage):
  line=(t/f'{profile}-{mode}/banjotooi-debug.log').read_text().splitlines()[0]
  return int(re.search(r'\b'+stage+r'=\d+/(\d+)/',line)[1])
 assert first_sum('timing','prepare')==12 and first_sum('timing','advance')==24
 assert first_sum('bad-observer','prepare')!=12 # altered boundary is detected
 assert 'entry_gap_max_us=1287318 ' in lines[-1]
 assert any('entry_gap_window_max_us=841862 ' in line for line in lines)
 assert int(re.search(r'entry_gap_window_max_us=(\d+)',lines[-1])[1])<841862
 old=(t/f'parent-window-{mode}/banjotooi-debug.log').read_text()
 assert 'entry_gap_max_us=1287318 ' in old and 'entry_gap_window_max_us=' not in old
 key=['disabled','empty','no_candidate','no_history_match','published'][result]
 # No lifetime sample cap; the late report contains more than sixteen calls.
 assert int(re.search(r'(?:current_)?'+key+r'=(\d+)',lines[-1])[1])>16
print('PASS: 50 publication-tail cases match exact parent; all eligibility reasons, format/width/address mismatch, coverage-only/duplicate rejection, late report and upstream gap attribution; startup-masked exact-parent window control, missing boundary and unsafe-candidate controls detected')
PY
