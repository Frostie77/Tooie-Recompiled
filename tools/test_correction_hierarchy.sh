#!/usr/bin/env bash
# Optional arguments: original retained logs, never ROMs or a device.
set -euo pipefail
r=$(cd "$(dirname "$0")/.." && pwd)
t=$(mktemp -d);trap 'rm -rf "$t"' EXIT
rt="$r/lib/rt64/src"
python3 - "$r" "$t" <<'PY'
from pathlib import Path
import hashlib,re,subprocess,sys
r,t=map(Path,sys.argv[1:])
assert hashlib.sha256((r/'tests/fixtures/odin-step86-hierarchy.txt').read_bytes()).hexdigest()=='400b89118feb428da08399c85f23c83386da1b90adf9812041a00cdae0953589'
(t/'matching_queue.h').write_text('#include "hle/rt64_workload.h"\nnamespace RT64 {struct WorkloadQueue {std::array<Workload,4> workloads;};}\n')
for name in ['hle/rt64_game_frame.cpp','render/rt64_transform_processor.cpp']:
 # Execute the exact parent processor, rather than symbolize another binary
 # or let a future correction silently replace the failing control.
 s=subprocess.check_output(['git','-C',str(r),'show','5db2d7f3719df6d196bf46a7a52a5e749d152956:lib/rt64/src/'+name],text=True)
 s=s.replace('#include "rt64_workload_queue.h"','#include "matching_queue.h"').replace('#include "hle/rt64_workload_queue.h"','#include "matching_queue.h"')
 s=s.replace('#include "../../../../src/render_diagnostics.h"','#include "render_diagnostics.h"')
 (t/Path(name).name).write_text(s)
(t/'rt64_rigid_body.cpp').write_bytes(subprocess.check_output(['git','-C',str(r),'show','5db2d7f3719df6d196bf46a7a52a5e749d152956:lib/rt64/src/hle/rt64_rigid_body.cpp']))
source=subprocess.check_output(['git','-C',str(r),'show','01495d04a248bb263a0f0d18695c9cfc983cf2d5:RecompiledFuncs/funcs_25.c'],text=True)
function=re.search(r'^RECOMP_FUNC void func_800AEBA8\(.*?(?=^RECOMP_FUNC |\Z)',source,re.M|re.S).group()
old='ctx->f10.fl = ctx->f10.fl - ctx->f19.fl;';assert function.count(old)==1
bad=function.replace('void func_800AEBA8(', 'void func_800AEBA8_bad(').replace(old,'ctx->f10.fl = ctx->f10.fl + ctx->f19.fl;')
(t/'builder.c').write_text('#include "recomp.h"\nvoid func_800AF2F0(uint8_t*,recomp_context*);\n'+function+bad)
PY
flags=(-std=c++17 -O1 -pthread -DNDEBUG -DHLSL_CPU -D__PRFCHWINTRIN_H -ffunction-sections -fdata-sections
 -I"$t" -I"$r/src" -I"$rt" -I"$rt/render" -I"$rt/hle" -I"$rt/common" -I"$rt/contrib" -I"$rt/contrib/hlslpp/include"
 -I"$rt/contrib/json/single_include" -I"$rt/contrib/dxc/inc" -I"$rt/contrib/plume")
sources=("$r/tests/correction_hierarchy.cpp" "$t/rt64_transform_processor.cpp" "$t/rt64_game_frame.cpp" "$t/rt64_rigid_body.cpp")
for source in common/rt64_math.cpp hle/rt64_framebuffer_changes.cpp hle/rt64_framebuffer_storage.cpp hle/rt64_projection.cpp common/rt64_common.cpp preset/rt64_preset_scene.cpp preset/rt64_preset.cpp;do sources+=("$rt/$source");done
"${CC:-gcc}" -std=c11 -O1 -fwrapv -fsanitize=address,undefined -fno-omit-frame-pointer -I"$r/lib/N64Recomp/include" -c "$t/builder.c" -o "$t/builder.o"
"${CXX:-g++}" "${flags[@]}" -I"$r/lib/N64Recomp/include" -fsanitize=address,undefined -fno-omit-frame-pointer "${sources[@]}" "$t/builder.o" -Wl,--gc-sections -o "$t/hierarchy"
export ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1
python3 "$r/tools/replay_correction_trace.py" "$r/tests/fixtures/odin-step86-hierarchy.txt" > "$t/fixture.json"
"$t/hierarchy" "$t/fixture.json"
for mode in --require-current --wrong-pivot --wrong-builder;do
 if "$t/hierarchy" "$t/fixture.json" "$mode" > "$t/control.log" 2>&1;then echo "FAIL: accepted $mode";exit 1;else test "$?" -eq 1;fi
 rg -q '^hierarchy failure:' "$t/control.log"
done
for log in "$@";do
 python3 "$r/tools/replay_correction_trace.py" "$log" > "$t/external.json"
 "$t/hierarchy" "$t/external.json"
done
echo 'PASS: exact parent consumer violates copied joint constraint; wrong authored pivot and changed generated pivot subtraction also fail'
if [ -n "${SIMD_CXX:-}" ];then
 "${CC:-gcc}" -std=c11 -O1 -fwrapv -I"$r/lib/N64Recomp/include" -c "$t/builder.c" -o "$t/builder-simd.o"
 "${SIMD_CXX}" "${flags[@]}" -I"$r/lib/N64Recomp/include" "${sources[@]}" "$t/builder-simd.o" -Wl,--gc-sections -o "$t/hierarchy-simd"
 "$t/hierarchy-simd" "$t/fixture.json"
 for log in "$@";do
  python3 "$r/tools/replay_correction_trace.py" "$log" > "$t/external.json"
  "$t/hierarchy-simd" "$t/external.json"
 done
fi
