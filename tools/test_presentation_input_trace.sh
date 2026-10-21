#!/usr/bin/env bash
set -euo pipefail
r=$(cd "$(dirname "$0")/.." && pwd)
t=$(mktemp -d);trap 'rm -rf "$t"' EXIT
python3 - "$r" "$t" <<'PY'
from pathlib import Path
import subprocess,sys
r,t=map(Path,sys.argv[1:])
for profile in ('current','parent','mutant'):
 s=(r/'lib/rt64/src/hle/rt64_state.cpp').read_text() if profile!='parent' else subprocess.check_output(['git','-C',str(r),'show','d40c4e150d2372fbdfe82681078575b4dd47fd2c:lib/rt64/src/hle/rt64_state.cpp'],text=True)
 a=s.index('    void State::updateScreen(');b=s.index('    void State::updateMultisampling()',a)
 body=s[a:b]
 if profile=='mutant':
  old='if (fromEarlyPresent || viDifferent || fbChangesMade || screenChangesMade) {'
  assert body.count(old)==1;body=body.replace(old,'if (true) {')
 (t/profile).mkdir();(t/profile/'update-screen.inc').write_text(body)
PY
flags=(-std=c++20 -O1 -pthread -fsanitize=address,undefined -fno-omit-frame-pointer -I"$r/src")
export ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1
for profile in timing default zero desktop parent;do
 defs=(-D__ANDROID__);source=current
 case "$profile" in timing|parent) defs+=(-DTOOIE_ANDROID_TIMING_TRACE=1);;zero) defs+=(-DTOOIE_ANDROID_TIMING_TRACE=0);;desktop) defs=(-DTOOIE_ANDROID_TIMING_TRACE=1);;esac
 if [ "$profile" = parent ];then source=parent;fi
 "${CXX:-g++}" "${flags[@]}" "${defs[@]}" -I"$t/$source" "$r/tests/presentation_input.cpp" -o "$t/test"
 for mode in {0..7};do
  mkdir "$t/$profile-$mode"
  APP_FOLDER_PATH="$t/$profile-$mode" "$t/test" "$mode" >"$t/$profile-$mode.txt"
 done
done
"${CXX:-g++}" "${flags[@]}" -D__ANDROID__ -DTOOIE_ANDROID_TIMING_TRACE=1 -I"$t/mutant" "$r/tests/presentation_input.cpp" -o "$t/mutant-test"
mkdir "$t/mutant-log"
if APP_FOLDER_PATH="$t/mutant-log" "$t/mutant-test" 0 >"$t/mutant.txt" 2>&1;then
 echo 'FAIL: accepted unconditional publication';exit 1
else test "$?" -eq 1;fi
rg -q '^input failure:' "$t/mutant.txt"
python3 - "$t" <<'PY'
from pathlib import Path
import sys
t=Path(sys.argv[1])
for mode in range(8):
 outputs={(t/f'{p}-{mode}.txt').read_text() for p in ('timing','default','zero','desktop','parent')}
 assert len(outputs)==1,(mode,outputs)
 for p in ('default','zero','desktop','parent'):assert not (t/f'{p}-{mode}'/'banjotooi-debug.log').exists()
 text=(t/f'timing-{mode}'/'banjotooi-debug.log').read_text()
 result=3 if mode in (1,2,3,5) else 1 if mode==4 else 0 if mode==6 else 2
 assert f'peak_result={result} ' in text,(mode,text)
 assert f'vi_flags_known={int(mode!=6)} ' in text
 assert f'memory_flags_known={int(result in (2,3))} ' in text
 assert 'Runtime present-input:' in text and ' pid=' in text and ' tid=' in text
 assert len(text)<1900
assert not (t/'parent-0'/'banjotooi-debug.log').exists() # exact-parent missing-attribution control
print('PASS: 40 complete updateScreen cases: VI/CPU/FB changes, unchanged/hidden image, early-ignore, early publication, paused repeat; exact-parent/Android/desktop outputs match, actual diagnostic reasons retained; changed publication predicate rejected')
PY
