#!/usr/bin/env bash
# Optional arguments are retained Runtime-geometry logs, never a ROM or device.
set -euo pipefail
r=$(cd "$(dirname "$0")/.." && pwd)
t=$(mktemp -d);trap 'rm -rf "$t"' EXIT
rt="$r/lib/rt64/src"
python3 - "$r" "$t" <<'PY'
from pathlib import Path
import sys
r,t=map(Path,sys.argv[1:])
(t/'matching_queue.h').write_text('#include "hle/rt64_workload.h"\nnamespace RT64 {struct WorkloadQueue {std::array<Workload,4> workloads;};}\n')
for name in ('hle/rt64_game_frame.cpp','render/rt64_transform_processor.cpp'):
 s=(r/'lib/rt64/src'/name).read_text().replace('#include "rt64_workload_queue.h"','#include "matching_queue.h"').replace('#include "hle/rt64_workload_queue.h"','#include "matching_queue.h"')
 s=s.replace('#include "../../../../src/render_diagnostics.h"','#include "render_diagnostics.h"')
 (t/Path(name).name).write_text(s)
PY
flags=(-std=c++17 -O1 -pthread -DNDEBUG -DHLSL_CPU -D__PRFCHWINTRIN_H -ffunction-sections -fdata-sections
 -I"$t" -I"$r/src" -I"$rt" -I"$rt/render" -I"$rt/hle" -I"$rt/common" -I"$rt/contrib" -I"$rt/contrib/hlslpp/include"
 -I"$rt/contrib/json/single_include" -I"$rt/contrib/dxc/inc" -I"$rt/contrib/plume")
sources=("$r/tests/geometry_capture.cpp" "$t/rt64_transform_processor.cpp" "$t/rt64_game_frame.cpp")
for source in common/rt64_math.cpp hle/rt64_rigid_body.cpp hle/rt64_framebuffer_changes.cpp hle/rt64_framebuffer_storage.cpp hle/rt64_projection.cpp common/rt64_common.cpp preset/rt64_preset_scene.cpp preset/rt64_preset.cpp;do sources+=("$rt/$source");done
"${CXX:-g++}" "${flags[@]}" -fsanitize=address,undefined -fno-omit-frame-pointer "${sources[@]}" -Wl,--gc-sections -o "$t/consumer"
export ASAN_OPTIONS=detect_leaks=0
export UBSAN_OPTIONS=halt_on_error=1
python3 "$r/tools/replay_geometry_trace.py" --consumer-input "$r/tests/fixtures/odin-step81-geometry.txt" > "$t/fixture.json"
"$t/consumer" "$t/fixture.json"
for log in "$@";do
 python3 "$r/tools/replay_geometry_trace.py" --consumer-input "$log" > "$t/external.json"
 "$t/consumer" "$t/external.json"
done
python3 - "$r" "$t" <<'PY'
from pathlib import Path
import importlib.util,json,subprocess,sys
r,t=map(Path,sys.argv[1:]);tool=r/'tools/replay_geometry_trace.py'
spec=importlib.util.spec_from_file_location('replay',tool);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
source=(r/'tests/fixtures/odin-step81-geometry.txt').read_text();fixture=json.loads((t/'fixture.json').read_text())
assert len(fixture['triangles'])==3
path=t/'utf16.txt';path.write_text(source,encoding='utf-16');assert m.consumer_input(path)==fixture
for name,bad in (
 ('torn','\n'.join(s for s in source.splitlines() if not ('sample=239755628136 ' in s and 'corner=2 matrix=consumed_previous ' in s))),
 ('duplicate',source+source.splitlines()[0]+'\n'),
 ('mixed-process',source+source.replace('pid=24984','pid=24985').replace('sample=','sample=1')),
 ('owner-conflict',source.replace('tid=25058','tid=12345',1)),
 ('bad-flags',source.replace('lerp_flags=0x3F','lerp_flags=0xFF',1)),
 ('nan',source.replace('pos=-2/131/-33','pos=nan/131/-33',1)),
 ('no-trace','Runtime interpolation: player_mapped=1\n')):
 path=t/(name+'.txt');path.write_text(bad)
 try:m.consumer_input(path)
 except ValueError:pass
 else:raise AssertionError('accepted '+name)
for name,change in (
 ('matrix',lambda g:g['triangles'][0]['corners'][0]['consumed'][3].__setitem__(0,9999)),
 ('skip',lambda g:g['triangles'][2]['corners'][0]['meta'].__setitem__('lerp_flags','0x3F'))):
 bad=json.loads(json.dumps(fixture));change(bad);path=t/(name+'.json');path.write_text(json.dumps(bad))
 result=subprocess.run([str(t/'consumer'),str(path)],capture_output=True,text=True)
 assert result.returncode!=0 and 'capture mismatch:' in result.stderr,(name,result.stderr)
print('PASS: UTF-16 input; torn/duplicate/mixed-process/owner/flags/NaN/absent controls; corrupted matrix and altered AUTO skip fail the actual consumer')
PY
if [ -n "${SIMD_CXX:-}" ];then
 "${SIMD_CXX}" "${flags[@]}" "${sources[@]}" -Wl,--gc-sections -o "$t/consumer-simd"
 "$t/consumer-simd" "$t/fixture.json"
 for log in "$@";do
  python3 "$r/tools/replay_geometry_trace.py" --consumer-input "$log" > "$t/external.json"
  "$t/consumer-simd" "$t/external.json"
 done
fi
