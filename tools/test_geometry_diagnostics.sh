#!/usr/bin/env bash
set -euo pipefail
r=$(cd "$(dirname "$0")/.." && pwd)
if [ -n "${TOOIE_GEOMETRY_TEST_BUILD_DIR:-}" ]; then
 t="$TOOIE_GEOMETRY_TEST_BUILD_DIR";mkdir -p "$t"
else
 t=$(mktemp -d);trap 'rm -rf "$t"' EXIT
fi
rt="$r/lib/rt64/src"
python3 - "$r" "$t" <<'PY'
from pathlib import Path
import subprocess,sys
r,t=map(Path,sys.argv[1:]);base='98f909cc6837cb9bc66cad1018fefd3b090add8d'
(t/'matching_queue.h').write_text('#include "hle/rt64_workload.h"\nnamespace RT64 {struct WorkloadQueue {std::array<Workload,4> workloads;};}\n')
for mode in ('current','base'):
 for name in ('hle/rt64_game_frame.cpp','render/rt64_transform_processor.cpp'):
  s=(r/'lib/rt64/src'/name).read_text() if mode=='current' else subprocess.check_output(['git','-C',str(r),'show',base+':lib/rt64/src/'+name],text=True)
  s=s.replace('#include "../../../../src/render_diagnostics.h"','#include "render_diagnostics.h"')
  s=s.replace('#include "rt64_workload_queue.h"','#include "matching_queue.h"').replace('#include "hle/rt64_workload_queue.h"','#include "matching_queue.h"')
  (t/(mode+'-'+Path(name).name)).write_text(s)
PY
flags=(-std=c++17 -O1 -pthread -DNDEBUG -DHLSL_CPU -D__PRFCHWINTRIN_H -ffunction-sections -fdata-sections
 -I"$t" -I"$r/tests/android_headers" -I"$r/src" -I"$rt" -I"$rt/render" -I"$rt/hle" -I"$rt/common" -I"$rt/contrib" -I"$rt/contrib/hlslpp/include"
 -I"$rt/contrib/json/single_include" -I"$rt/contrib/dxc/inc" -I"$rt/contrib/plume"
 -fsanitize=address,undefined -fno-omit-frame-pointer)
objects=()
for source in common/rt64_math.cpp hle/rt64_rigid_body.cpp hle/rt64_framebuffer_changes.cpp hle/rt64_framebuffer_storage.cpp hle/rt64_projection.cpp common/rt64_common.cpp preset/rt64_preset_scene.cpp preset/rt64_preset.cpp; do
 object="$t/$(basename "$source").o";"${CXX:-g++}" "${flags[@]}" -c "$rt/$source" -o "$object";objects+=("$object")
done
for mode in current base android-off desktop-on; do
 source_mode=$mode;definitions=(-D__ANDROID__ -DTOOIE_ANDROID_GEOMETRY_TRACE=1)
 if [ "$mode" = android-off ];then source_mode=current;definitions=(-D__ANDROID__ -DTOOIE_ANDROID_GEOMETRY_TRACE=0);fi
 if [ "$mode" = desktop-on ];then source_mode=current;definitions=(-DTOOIE_ANDROID_GEOMETRY_TRACE=1);fi
 "${CXX:-g++}" "${flags[@]}" "${definitions[@]}" "$r/tests/geometry_diagnostics.cpp" "$t/$source_mode-rt64_game_frame.cpp" "$t/$source_mode-rt64_transform_processor.cpp" "${objects[@]}" -Wl,--gc-sections -o "$t/$mode"
 mkdir -p "$t/$mode-log"
 rm -f "$t/$mode-log/banjotooi-debug.log"
 APP_FOLDER_PATH="$t/$mode-log" ASAN_OPTIONS=detect_leaks=0 "$t/$mode" "$t/$mode.bin"
done
cmp "$t/current.bin" "$t/base.bin"
cmp "$t/current.bin" "$t/android-off.bin"
cmp "$t/current.bin" "$t/desktop-on.bin"
python3 - "$t" <<'PY'
from pathlib import Path
import sys
p=Path(sys.argv[1]);log=(p/'current-log/banjotooi-debug.log').read_text()
geometry=[line for line in log.splitlines() if line.startswith('Runtime geometry:')]
assert any('workload=123 weight=' in s and 'sample=999' not in s for s in geometry), 'actual TransformProcessor trace hook absent'
assert not any('Runtime geometry:' in s and 'sample=999' not in s for s in (p/'base-log/banjotooi-debug.log').read_text().splitlines()), 'exact base unexpectedly has a consumer hook'
assert all(len(s)<2047 and ' pid=' in s and ' tid=' in s for s in geometry)
assert any('identity=unknown' in s for s in geometry)
assert any('class=auto found=1' in s and 'departure=1' in s for s in geometry)
assert any('prev_transform=1' in s and 'prev_vertex_in_range=1' in s for s in geometry)
assert any('matrix=consumed rows=' in s for s in geometry)
assert not any('Runtime geometry:' in s and 'sample=999' not in s for s in (p/'android-off-log/banjotooi-debug.log').read_text().splitlines()), 'Android trace=0 unexpectedly has consumer hook'
assert not (p/'desktop-on-log/banjotooi-debug.log').exists(), 'desktop unexpectedly logged with trace=1'
print('PASS: bounded complete records, PID/TID/workload/sample provenance; exact 98f909c lacks hook; consumed matrices/velocities byte-identical with trace on/off and base')
PY

python3 - "$r" "$t" <<'PYTEST'
from pathlib import Path
import importlib.util,json,subprocess,sys
r,t=map(Path,sys.argv[1:]);tool=r/'tools/replay_geometry_trace.py'
spec=importlib.util.spec_from_file_location('geometry_replay',tool);module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
source=t/'current-log/banjotooi-debug.log';replay=module.replay(source)
control=next(x for x in replay if x['sample']==999 and x['category']=='auto')
assert control['face']==12 and control['consumed_edges'][0]<1e-6 and control['endpoint_envelope_departure']>.9999
assert control['pairs'][0]==[2,1] and control['pairs'][1]==[3,0]
utf16=t/'windows-log.txt';utf16.write_text(source.read_text(),encoding='utf-16')
assert module.replay(utf16)==replay
bad=t/'torn-log.txt';bad.write_text('\n'.join(line for line in source.read_text().splitlines() if not ('sample=999 ' in line and 'corner=2 matrix=consumed_previous ' in line)))
# Remove a required matrix from an explicitly found triangle: a torn sample
# must not be silently accepted as evidence of a complete replay.
try: module.replay(bad)
except ValueError as error: assert 'incomplete sample' in str(error)
else: raise AssertionError('accepted torn sample')
no_trace=t/'no-trace.txt';no_trace.write_text('Runtime interpolation: player_mapped=1\n')
assert subprocess.run([sys.executable,str(tool),str(no_trace)],capture_output=True).returncode==1
print('PASS: replay reconstructs consumed collapse/identity pairs, UTF-16 Windows logs, missing-trace and torn-record controls')
PYTEST

if [ -n "${SIMD_CXX:-}" ];then
 simd_flags=()
 for flag in "${flags[@]}";do
  if [ "$flag" != -fsanitize=address,undefined ] && [ "$flag" != -fno-omit-frame-pointer ];then simd_flags+=("$flag");fi
 done
 "${SIMD_CXX}" "${simd_flags[@]}" -D__ANDROID__ -DTOOIE_ANDROID_GEOMETRY_TRACE=1 \
  "$r/tests/geometry_diagnostics.cpp" "$t/current-rt64_game_frame.cpp" "$t/current-rt64_transform_processor.cpp" \
  "$rt/common/rt64_math.cpp" "$rt/hle/rt64_rigid_body.cpp" "$rt/hle/rt64_framebuffer_changes.cpp" "$rt/hle/rt64_framebuffer_storage.cpp" \
  "$rt/hle/rt64_projection.cpp" "$rt/common/rt64_common.cpp" "$rt/preset/rt64_preset_scene.cpp" "$rt/preset/rt64_preset.cpp" -Wl,--gc-sections -o "$t/simd"
 mkdir -p "$t/simd-log"
 APP_FOLDER_PATH="$t/simd-log" "$t/simd" "$t/simd.bin"
 echo 'PASS: independent SIMD consumed-geometry observer fixture (scalar build carries ASan/UBSan)'
fi
