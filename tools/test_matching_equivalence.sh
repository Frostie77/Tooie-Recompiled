#!/usr/bin/env bash
set -euo pipefail
r=$(cd "$(dirname "$0")/.." && pwd)
if [ -n "${TOOIE_MATCH_TEST_BUILD_DIR:-}" ]; then
 t="$TOOIE_MATCH_TEST_BUILD_DIR"
 mkdir -p "$t"
else
 t=$(mktemp -d)
 trap 'rm -rf "$t"' EXIT
fi
rt="$r/lib/rt64/src"
base=fc0a2213b12812d028f763f3f164d0cdb92dbea5
python3 - "$r" "$t" "$base" <<'PY'
from pathlib import Path
import subprocess,sys
r,t=map(Path,sys.argv[1:3]);base=sys.argv[3]
(t/'matching_queue.h').write_text('#include "hle/rt64_workload.h"\nnamespace RT64 {struct WorkloadQueue {std::array<Workload,4> workloads;};}\n')
for mode in ('current','base','base-no-counter'):
 s=(r/'lib/rt64/src/hle/rt64_game_frame.cpp').read_text() if mode=='current' else subprocess.check_output(['git','-C',str(r),'show',base+':lib/rt64/src/hle/rt64_game_frame.cpp'],text=True)
 s=s.replace('#include "../../../../src/render_diagnostics.h"','#include "matching_timer.h"\n#include "matching_equivalence.h"').replace('#include "rt64_workload_queue.h"','#include "matching_queue.h"')
 # Force rare hash collisions in selected fixtures, identically for both
 # production matchers. This does not equate their distinct expansion inputs.
 s=s.replace('hashMap.emplace(hashFromCall(call, matrixIdHash),','hashMap.emplace(matching_test::forceCollision ? 7 : hashFromCall(call, matrixIdHash),')
 anchor='        stages.elapsed(4,pairsStart);'
 extra='        matching_test::capture(transformCheckSet,tileCheckSet,lookAtCheckSet);\n'
 if mode=='current':
  extra+='        matching_test::visits += expansion.transformRangePairs + expansion.tileVisits + expansion.lookAtVisits;\n        matching_test::inputCalls += expansion.inputCalls;\n        matching_test::tileLookups += expansion.tileLookups;\n'
 else:
  original='#if defined(__ANDROID__)\n                ++matchingCounts.callPairs;\n#endif'
  assert original in s
  s=s.replace(original,'                ++matching_test::visits;' if mode=='base' else '')
 assert anchor in s;s=s.replace(anchor,extra+anchor,1)
 (t/(mode+'.cpp')).write_text(s)
PY
flags=(-std=c++17 -O1 -pthread -DNDEBUG -DHLSL_CPU -D__PRFCHWINTRIN_H -ffunction-sections -fdata-sections
 -I"$t" -I"$r/tests" -I"$rt" -I"$rt/hle" -I"$rt/common" -I"$rt/contrib" -I"$rt/contrib/hlslpp/include"
 -I"$rt/contrib/json/single_include" -I"$rt/contrib/dxc/inc" -I"$rt/contrib/plume"
 -fsanitize=address,undefined -fno-omit-frame-pointer)
support=(common/rt64_math.cpp hle/rt64_rigid_body.cpp hle/rt64_framebuffer_changes.cpp hle/rt64_framebuffer_storage.cpp
 hle/rt64_projection.cpp common/rt64_common.cpp preset/rt64_preset_scene.cpp preset/rt64_preset.cpp)
objects=()
for source in "${support[@]}"; do
 object="$t/$(basename "$source").o"
 "${CXX:-g++}" "${flags[@]}" -c "$rt/$source" -o "$object"
 objects+=("$object")
done
"${CXX:-g++}" "${flags[@]}" -c "$r/tests/matching_equivalence.cpp" -o "$t/fixture.o"
for mode in current base base-no-counter; do
 "${CXX:-g++}" "${flags[@]}" -c "$t/$mode.cpp" -o "$t/$mode.o"
 "${CXX:-g++}" "${flags[@]}" "$t/fixture.o" "$t/$mode.o" "${objects[@]}" -Wl,--gc-sections -o "$t/$mode-test"
done
ASAN_OPTIONS=detect_leaks=0 "$t/current-test" "$t/current.bin"
ASAN_OPTIONS=detect_leaks=0 "$t/base-test" "$t/base.bin"
cmp "$t/current.bin" "$t/base.bin"
echo 'PASS: byte-identical ordered candidate sets and consumed geometry/tile/look-at/velocity/history output against exact fc0a221'
for control in --budget --device-budget; do
 ASAN_OPTIONS=detect_leaks=0 "$t/current-test" "$t/budget.bin" "$control"
 if ASAN_OPTIONS=detect_leaks=0 "$t/base-test" "$t/base-budget.bin" "$control" >"$t/control.log" 2>&1;then
  echo 'FAIL: tested source passed redundant-expansion operation bound';exit 1
 else test "$?" -eq 1 || { cat "$t/control.log";exit 1; };fi
 grep -q 'FAIL: redundant candidate expansion' "$t/control.log"
 cat "$t/control.log"
done
echo 'PASS: exact fc0a221 fails both repeated-call and distinct-range/TMEM 704x704 operation controls; optimized source passes without an arbitrary matching cap'
# Same production baseline with/without just the old per-pair diagnostic counter;
# phase clocks and serialization disabled for these host-only observations.
for timing in --timing --device-timing; do
 for mode in base base-no-counter current; do
  echo "Host timing workload=$timing mode=$mode"
  ASAN_OPTIONS=detect_leaks=0 "$t/$mode-test" "$t/timing.bin" "$timing"
 done
done
