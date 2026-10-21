#!/usr/bin/env bash
set -euo pipefail
r=$(cd "$(dirname "$0")/.." && pwd)
t=$(mktemp -d)
trap 'rm -rf "$t"' EXIT
python3 - "$r" "$t" <<'PY'
from pathlib import Path
import json,re,subprocess,sys,tomllib
r,t=map(Path,sys.argv[1:])
names=['glrecord_entrypoint_0','func_800A7D30','func_80014F4C',
 'func_800D8FA0','func_800D8F50','func_800D8F78','func_800D9004','func_800D8FF8','func_80018444',
 'func_800F0D90','func_800F0DB8','static_3_800F0DA8','func_8001608C',
 'func_800A9CAC','func_800A9CC8','func_800A9CDC','func_800DC0C0','func_80015D60',
 'func_800C95D4','func_800C91C8','func_800167F4','func_80015FA0','func_80015FC8',
 'func_80015178','func_80015184','func_80014F38','func_800151BC','func_80015410','func_8001546C','func_80014A54']
sources=[p.read_text() for p in (r/'RecompiledFuncs').glob('funcs_*.c')]
bodies=[]
for name in names:
 found=[m[0] for source in sources if (m:=re.search(r'^RECOMP_FUNC void '+name+r'\(.*?(?=^RECOMP_FUNC |\Z)',source,re.M|re.S))]
 assert len(found)==1,(name,len(found));bodies.append(found[0])
body=''.join(bodies)
calls=sorted(set(re.findall(r'\b([A-Za-z_][A-Za-z_0-9]*)\(rdram,\s*ctx\)',body)))
declarations=''.join('void '+name+'(uint8_t*,recomp_context*);\n' for name in sorted(set(names+calls)))
declarations+='void tooie_demo_pacing(uint8_t*,recomp_context*,uint32_t);\n'
header='#include "demo_replay_adapters.h"\n'+declarations
(t/'generated.c').write_text(header+body)
parent=re.sub(r'#if defined\(__ANDROID__\)\n\s*tooie_demo_pacing\(rdram, ctx, [0-3]U\);\n#endif\n','',body)
assert parent!=body and body.count('tooie_demo_pacing(rdram, ctx,')==5
(t/'parent.c').write_text(header+parent)
# Mutation: changing the recorded span into a fixed multiplier is rejected.
first,separator,last=body.rpartition('tooie_demo_pacing(rdram, ctx, 2U);')
assert separator
bad=first+'ctx->r2 = 2;'+last
(t/'bad-interval.c').write_text(header+bad)
cfg=tomllib.loads((r/'banjotooie.us.toml').read_text())
hooks=[h for h in cfg['patches']['hook'] if 'tooie_demo_pacing(' in h['text']]
assert [(h['func'],h['before_vram']) for h in hooks]==[
 ('func_8001608C',0x800160F8),('func_800151BC',0x800151BC),
 ('func_800151BC',0x8001520C),('func_800151BC',0x8001524C),('func_800151BC',0x800153FC)]
for h in hooks:
 function=next(b for b in bodies if b.startswith('RECOMP_FUNC void '+h['func']+'('))
 assert re.sub(r'\s+','',h['text']) in re.sub(r'\s+','',function)
# Keep the existing OS arrival/queue/swap adapters, without their old test main.
prefix=(r/'tests/guest_frame_sync.cpp').read_text().split('int main(int argc,char**)',1)[0]
a=prefix.index('extern "C" void timed_input(');b=prefix.index('static std::vector<uint8_t>',a)
prefix=prefix[:a]+prefix[b:]
prefix=prefix.replace('#include "tooie_recomp.h"','#include "recomp.h"\n#include "demo_pacing_guest.h"')
prefix=prefix.replace('    std::memset(rdram,0,memory.size());','    tooie::demo::pacing()={};\n    std::memset(rdram,0,memory.size());')
prefix=prefix.replace('vis=0;ackRemaining=0;', 'vis=0;swapDelay=0;ackRemaining=0;')
(t/'demo_pacing_prefix.inc').write_text(prefix)
capture=json.loads((r/'tests/fixtures/odin-demo-pacing-step120.json').read_text())
assert capture['original_pid']==31303 and len(capture['records'])==279
rows=[]
for row in capture['records']:
 assert row['mode']=='2/2'
 rows.append('{'+row['recorded_span']+',0x'+row['delta_bits']+'U}')
(t/'captured_demo_spans.h').write_text('struct DemoInput {unsigned span,delta_bits;};\nstatic constexpr DemoInput captured_demo_inputs[]={'+','.join(rows)+'};\n')
print('PASS: exact production record/VI-wait extraction and five generated/config hook boundaries; 279 retained sparse replay records')
PY
flags=(-O1 -g -fwrapv -fsanitize=address,undefined -fno-omit-frame-pointer -I"$r/include" -I"$r/src" -I"$r/tests" -I"$t" -I"$r/lib/N64Recomp/include")
export ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1
for profile in android desktop parent bad-interval; do
  defs=(); source=generated
  if [[ "$profile" != desktop ]]; then defs=(-D__ANDROID__); fi
  case "$profile" in parent|bad-interval) source=$profile;; esac
  "${CC:-gcc}" -std=c11 "${flags[@]}" "${defs[@]}" -Werror=implicit-function-declaration -c "$t/$source.c" -o "$t/generated.o"
  "${CC:-gcc}" -std=c11 "${flags[@]}" "${defs[@]}" -c "$r/tests/demo_replay_adapters.c" -o "$t/adapters.o"
  "${CXX:-g++}" -std=c++20 "${flags[@]}" "${defs[@]}" "$r/tests/demo_pacing.cpp" "$t/generated.o" "$t/adapters.o" -lm -o "$t/test"
  if "$t/test" > "$t/$profile.log" 2>&1; then
    case "$profile" in parent|bad-interval) echo "FAIL: accepted $profile"; exit 1;; esac
    cat "$t/$profile.log"
  else
    rc=$?
    case "$profile" in parent|bad-interval)
      [[ "$rc" == 1 ]] && rg -q '^frame-sync failure:' "$t/$profile.log" || { cat "$t/$profile.log"; exit 1; };;
    *) cat "$t/$profile.log"; exit 1;; esac
  fi
done
echo 'PASS: exact-parent missing pacing and altered wait controls fail; desktop, ordinary timing, input and dt semantics retained'
