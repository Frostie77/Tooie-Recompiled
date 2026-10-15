#!/usr/bin/env bash
set -euo pipefail
r=$(cd "$(dirname "$0")/.." && pwd)
t=$(mktemp -d)
trap 'rm -rf "$t"' EXIT
base=36c61d884e4ea4446a5542e14d2107911a4666c9
python3 - "$r" "$t" "$base" "${1:-}" <<'PY'
from pathlib import Path
import subprocess,sys
r,t=map(Path,sys.argv[1:3]);base=sys.argv[3]
sources=[('current',(r/'lib/rt64/src/hle/rt64_present_queue.cpp').read_text())]
if sys.argv[4]=='--baseline':
 sources.append(('base',subprocess.check_output(['git','-C',str(r),'show',base+':lib/rt64/src/hle/rt64_present_queue.cpp'],text=True)))
for name,p in sources:
 d=t/name;d.mkdir()
 a=p.index('            {',p.index('        while (presentThreadRunning)'))
 b=p.index('            if (processCursor >= 0)',a)
 (d/'outer-select.inc').write_text(p[a:b])
 a=p.index('                // Do not hold the lifecycle lease')
 a=p.index('\n',a)+1;b=p.index('                // May display',a)
 (d/'producer-wait.inc').write_text(p[a:b])
 a=p.index('                skipPresent = skipPresent || ',p.index('void PresentQueue::threadLoop'))
 b=p.index('                if (!present.paused)',a)
 # Exclude observation-only capture, retain all actual skip/notify dispatch.
 if '#if defined(__ANDROID__)\n                queueStages.elapsed' in p[a:b]:
  b=p.index('#if defined(__ANDROID__)\n                queueStages.elapsed',a)
 (d/'outer-dispatch.inc').write_text(p[a:b])
 marker='#if defined(__ANDROID__)\n                queueStages.elapsed(2,dispatchStart);'
 if marker in p:
  a=p.index(marker);b=p.index('                if (!present.paused)',a)
  (d/'outer-observe.inc').write_text(p[a:b])
  a=p.index('#if defined(__ANDROID__)\n                queueStages.report(queueStart,queueContext,decisionTotals);')
  b=p.index('#endif',a)+len('#endif')
  (d/'outer-report.inc').write_text(p[a:b])
 else:
  (d/'outer-observe.inc').write_text('')
  (d/'outer-report.inc').write_text('')
 for method in ('skipInterpolation','notifyPresentId','threadAdvanceBarrier'):
  a=p.index('    void PresentQueue::'+method+'(');b=p.index('\n    }',a)+len('\n    }')
  (d/(method+'.inc')).write_text(p[a:b].replace('PresentQueue::',''))
PY
flags=(-std=c++17 -pthread -fsanitize=address,undefined -fno-omit-frame-pointer)
"${CXX:-g++}" "${flags[@]}" -D__ANDROID__ -I"$t/current" "$r/tests/present_progress.cpp" -o "$t/current-test"
ASAN_OPTIONS=detect_leaks=0 "$t/current-test"
"${CXX:-g++}" "${flags[@]}" -D__ANDROID__ -I"$r/src" "$r/tests/render_queue_diagnostics.cpp" -o "$t/diagnostics"
APP_FOLDER_PATH="$t" ASAN_OPTIONS=detect_leaks=0 "$t/diagnostics" "$t/banjotooi-debug.log"
# Desktop keeps its existing backlog policy; use the same actual selection block.
"${CXX:-g++}" "${flags[@]}" -I"$t/current" "$r/tests/present_progress.cpp" -o "$t/desktop-test"
ASAN_OPTIONS=detect_leaks=0 "$t/desktop-test"
if [ "${1:-}" = --baseline ]; then
 "${CXX:-g++}" "${flags[@]}" -D__ANDROID__ -I"$t/base" "$r/tests/present_progress.cpp" -o "$t/base-test"
 if ASAN_OPTIONS=detect_leaks=0 "$t/base-test" >"$t/base.log" 2>&1;then
  echo 'FAIL: exact tested source passed the starvation control';exit 1
 else test "$?" -eq 1 || { cat "$t/base.log";exit 1; };fi
 grep -q 'FAIL: bounded presentation progress' "$t/base.log"
 cat "$t/base.log"
 echo 'PASS: exact 36c61d8 rendering source (identical in b12ac59) fails sustained-load progress control'
fi
