#!/usr/bin/env bash
set -euo pipefail
r=$(cd "$(dirname "$0")/.." && pwd)
t=$(mktemp -d);trap 'rm -rf "$t"' EXIT
rt="$r/lib/rt64/src"
python3 - "$r" "$t" <<'PY'
from pathlib import Path
import sys,re,subprocess,tomllib
r,t=map(Path,sys.argv[1:])
(t/'matching_queue.h').write_text('#pragma once\n#include "hle/rt64_workload.h"\nnamespace RT64 {struct WorkloadQueue {std::array<Workload,4> workloads;int writeCursor=0;};}\n')
for name in ['hle/rt64_game_frame.cpp','render/rt64_transform_processor.cpp']:
 s=(r/'lib/rt64/src'/name).read_text().replace('#include "rt64_workload_queue.h"','#include "matching_queue.h"').replace('#include "hle/rt64_workload_queue.h"','#include "matching_queue.h"')
 s=s.replace('#include "../../../../src/render_diagnostics.h"','#include "render_diagnostics.h"')
 (t/Path(name).name).write_text(s)
def method(source,needle):
 start=source.rfind('\n',0,source.index(needle))+1;brace=source.index('{',start);end=brace+1;depth=1
 while depth:depth+=(source[end]=='{')-(source[end]=='}');end+=1
 return source[start:end]
source=(r/'lib/rt64/src/hle/rt64_rsp.cpp').read_text()
names=['RSP','reset','clearExtended','maskPhysicalAddress','fromSegmented','fromSegmentedMasked','matrixCommon','matrix','popMatrix','skeletalBinding','matrixId','popMatrixId','computeModelViewProj']
s='\n'.join(method(source,'RSP::'+n+'(') for n in names)
for n in ['readExtendedVertexSegment','setVertexCommon']:
 body=method(source,'RSP::'+n+'(');pos=source.index(body);template=source[source.rfind('    template<',0,pos):pos];s+='\n'+template+body
source=(r/'lib/rt64/src/gbi/rt64_gbi.cpp').read_text()
for n in ['p0','p1']:s+='\n'+method(source,'DisplayList::'+n+'(')
source=(r/'lib/rt64/src/hle/rt64_workload.cpp').read_text()
s+='\n'+method(source,'Workload::resetDrawData(')
(t/'skeletal-rsp.inc').write_text(s)
source=(r/'lib/rt64/src/gbi/rt64_gbi_extended.cpp').read_text()
(t/'skeletal-gbi.inc').write_text('\n'.join(method(source,'void '+n+'(') for n in ['matrixGroupCommand','matrixGroupV1','popMatrixGroupV1']))
# Generated/config parity for the two new, independent production hook sites.
config=tomllib.loads((r/'banjotooie.us.toml').read_text())
hooks=[h for h in config['patches']['hook'] if 'tooie_rt64_skeleton_builder' in h['text']]
assert len(hooks)==2
generated=(r/'RecompiledFuncs/funcs_25.c').read_text()
scope=re.search(r'^RECOMP_FUNC void func_800AE820\(.*?(?=^RECOMP_FUNC |\Z)',generated,re.M|re.S)[0]
for hook in hooks:
 call=re.search(r'tooie_rt64_skeleton_builder\([^;]+;',hook['text'])[0]
 assert call in scope and '#if TOOIE_SKELETAL_POSE_ENABLED' in hook['text']
 assert scope.split(call,1)[1].lstrip().startswith('#endif\n    // 0x%08X:'%hook['before_vram'])
# Exact parent matcher/consumer remain the failing control, with today's data
# layouts but without any hierarchy selection or rendering correction.
for n in ['hle/rt64_game_frame.cpp','render/rt64_transform_processor.cpp']:
 s=subprocess.check_output(['git','-C',str(r),'show','60c0c89369347f6c7a9a722471be00b149447f92:lib/rt64/src/'+n],text=True)
 s=s.replace('#include "rt64_workload_queue.h"','#include "matching_queue.h"').replace('#include "hle/rt64_workload_queue.h"','#include "matching_queue.h"').replace('#include "../../../../src/render_diagnostics.h"','#include "render_diagnostics.h"')
 (t/('parent-'+Path(n).name)).write_text(s)
PY
flags=(-std=c++20 -O1 -pthread -DNDEBUG -D__ANDROID__ -DHLSL_CPU -D__PRFCHWINTRIN_H -ffunction-sections -fdata-sections
 -I"$t" -I"$r/src" -I"$r/tests/android_headers" -I"$r/lib/N64Recomp/include" -I"$rt" -I"$rt/render" -I"$rt/hle" -I"$rt/common" -I"$rt/contrib" -I"$rt/contrib/hlslpp/include"
 -I"$rt/contrib/json/single_include" -I"$rt/contrib/dxc/inc" -I"$rt/contrib/plume")
support=("$r/tests/actor_rigid.cpp" "$r/tests/actor_ownership.cpp" "$r/tests/skeletal_pose.cpp" "$r/tests/skeletal_rsp.cpp" "$rt/hle/rt64_rigid_body.cpp")
for n in common/rt64_math.cpp hle/rt64_framebuffer_changes.cpp hle/rt64_framebuffer_storage.cpp hle/rt64_projection.cpp common/rt64_common.cpp preset/rt64_preset_scene.cpp preset/rt64_preset.cpp;do support+=("$rt/$n");done
san=(-fsanitize=address,undefined -fno-omit-frame-pointer)
api_flags=(-I"$r/include" -I"$r/RecompiledFuncs" -I"$r/lib/N64ModernRuntime/librecomp/include"
 -I"$r/lib/N64ModernRuntime/ultramodern/include" -I"$r/lib/N64ModernRuntime/librecomp/src" -I"$r/lib/rt64/include"
 -I"$rt/contrib/mupen64plus-core/src/api" -I"$r/lib/N64ModernRuntime/thirdparty/concurrentqueue"
 -I"$r/lib/N64ModernRuntime/thirdparty" -I"$r/lib/N64Recomp/lib/fmt/include" -I"$r/lib/N64Recomp/lib/tomlplusplus")
python3 "$r/tools/prepare_actor_ownership.py" "$r" "$t/actor-generated.c"
"${CC:-gcc}" "${flags[@]:1}" "${api_flags[@]}" "${san[@]}" -std=gnu11 -fwrapv -c "$t/actor-generated.c" -o "$t/actor-generated.o"
"${CXX:-g++}" "${flags[@]}" "${api_flags[@]}" "${san[@]}" -c "$r/src/recomp_api.cpp" -o "$t/api.o"
flags+=("${api_flags[@]}")
objects=()
for source in "${support[@]}";do
 object="$t/support-${#objects[@]}.o"
 "${CXX:-g++}" "${flags[@]}" "${san[@]}" -c "$source" -o "$object"
 objects+=("$object")
done
"${CXX:-g++}" "${flags[@]}" "${san[@]}" "$t/api.o" "$t/actor-generated.o" "${objects[@]}" "$t/rt64_game_frame.cpp" "$t/rt64_transform_processor.cpp" -Wl,--gc-sections -o "$t/test"
python3 "$r/tools/replay_correction_trace.py" "$r/tests/fixtures/odin-step86-hierarchy.txt" > "$t/fixture.json"
export ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
"$t/test" "$t/fixture.json"
"${CXX:-g++}" "${flags[@]}" "${san[@]}" "$t/api.o" "$t/actor-generated.o" "${objects[@]}" "$t/parent-rt64_game_frame.cpp" "$t/parent-rt64_transform_processor.cpp" -Wl,--gc-sections -o "$t/parent"
if "$t/parent" "$t/fixture.json" > "$t/control.log" 2>&1;then echo 'FAIL: exact parent accepted production hierarchy';exit 1;else test "$?" -eq 1;fi
rg -q '^hierarchy failure:.*constraint\(p,output,weight,false\)' "$t/control.log"
echo 'PASS: exact 60c0c89 parent rejects live hierarchy regression'
for input in "$@";do
 python3 "$r/tools/replay_correction_trace.py" "$input" > "$t/external.json"
 "$t/test" "$t/external.json"
done
if [ -n "${SIMD_CXX:-}" ];then
 "${CC:-gcc}" "${flags[@]:1}" -std=gnu11 -fwrapv -c "$t/actor-generated.c" -o "$t/actor-simd.o"
 "$SIMD_CXX" "${flags[@]}" "${support[@]}" "$t/actor-simd.o" "$r/src/recomp_api.cpp" "$t/rt64_game_frame.cpp" "$t/rt64_transform_processor.cpp" -Wl,--gc-sections -o "$t/simd"
 "$t/simd" "$t/fixture.json"
 for input in "$@";do
  python3 "$r/tools/replay_correction_trace.py" "$input" > "$t/external.json"; "$t/simd" "$t/external.json"
 done
fi
