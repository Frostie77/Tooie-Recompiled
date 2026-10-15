#!/usr/bin/env bash
set -euo pipefail
r=$(cd "$(dirname "$0")/.." && pwd)
t=$(mktemp -d)
trap 'rm -rf "$t"' EXIT
rt="$r/lib/rt64/src"
python3 - "$rt" "$t" <<'PY'
from pathlib import Path
import sys
rt,t=map(Path,sys.argv[1:])
s=(rt/'hle/rt64_game_frame.cpp').read_text()
# Only adapters change: queue contains actual Workloads; phase timer stores
# measurements locally. Every production matching algorithm stays verbatim.
s=s.replace('#include "../../../../src/render_diagnostics.h"','#include "matching_timer.h"').replace('#include "rt64_workload_queue.h"','#include "matching_queue.h"')
(t/'matching.cpp').write_text(s)
(t/'matching_queue.h').write_text('#include "hle/rt64_workload.h"\nnamespace RT64 {struct WorkloadQueue {std::array<Workload,4> workloads;};}\n')
PY
flags=(-std=c++17 -O1 -pthread -DNDEBUG -DHLSL_CPU -D__PRFCHWINTRIN_H -ffunction-sections -fdata-sections
 -I"$t" -I"$r/tests" -I"$rt" -I"$rt/hle" -I"$rt/common" -I"$rt/contrib" -I"$rt/contrib/hlslpp/include"
 -I"$rt/contrib/json/single_include" -I"$rt/contrib/dxc/inc" -I"$rt/contrib/plume")
"${CXX:-g++}" "${flags[@]}" -fsanitize=address,undefined -fno-omit-frame-pointer \
 "$r/tests/matching_cost.cpp" "$t/matching.cpp" "$rt/common/rt64_math.cpp" "$rt/hle/rt64_rigid_body.cpp" \
 "$rt/hle/rt64_framebuffer_changes.cpp" "$rt/hle/rt64_framebuffer_storage.cpp" "$rt/hle/rt64_projection.cpp" "$rt/common/rt64_common.cpp" \
 "$rt/preset/rt64_preset_scene.cpp" "$rt/preset/rt64_preset.cpp" -Wl,--gc-sections -o "$t/matching"
ASAN_OPTIONS=detect_leaks=0 "$t/matching"
