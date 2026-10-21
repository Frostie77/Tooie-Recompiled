#!/usr/bin/env bash
set -euo pipefail
r=$(cd "$(dirname "$0")/.." && pwd)
t=$(mktemp -d);trap 'rm -rf "$t"' EXIT
python3 - "$r" "$t" <<'PY'
from pathlib import Path
import re,sys
r,t=map(Path,sys.argv[1:]);names=[
 'glrecord_entrypoint_0','func_800A7D30','func_80014F4C',
 'func_800D8FA0','func_800D8F50','func_800D8F78','func_800D9004','func_800D8FF8','func_80018444',
 'func_800F0D90','func_800F0DB8','static_3_800F0DA8',
 'func_8001608C','func_800A9CAC','func_800A9CC8','func_800A9CDC','func_800DC0C0','func_80015D60',
 'func_800C95D4','func_800C91C8','func_800167F4','func_80015FA0','func_80015FC8',
 'gsattract_entrypoint_0','gsattract_entrypoint_1']
# Locate by exact function start, including generated tail fragments. No hand
# translation or stubbed timing/replay callee is used in this test.
all_sources=[p.read_text() for p in (r/'RecompiledFuncs').glob('funcs_*.c')]
functions=[]
for name in names:
 found=[]
 for s in all_sources:
  match=re.search(r'^RECOMP_FUNC void '+name+r'\(.*?(?=^RECOMP_FUNC |\Z)',s,re.M|re.S)
  if match:found.append(match.group())
 assert len(found)==1,(name,len(found))
 functions+=found
declarations=''.join('void '+name+'(uint8_t*,recomp_context*);\n' for name in names)
(t/'generated.c').write_text('#include "demo_replay_adapters.h"\n'+declarations+'\n'.join(functions))
source=(t/'generated.c').read_text()
for name,old,new in (
 ('bad-span','ctx->r12 = MEM_BU(ctx->r3, 0X4);','ctx->r12 = MEM_BU(ctx->r3, 0X5);'),
 ('bad-index','ctx->r24 = ADD32(ctx->r2, 0X1);','ctx->r24 = ADD32(ctx->r2, 0X2);'),
 ('bad-timer','ctx->f12.fl = ctx->f4.fl + ctx->f0.fl;','ctx->f12.fl = ctx->f4.fl + 2*ctx->f0.fl;')):
 assert source.count(old)==1,(name,source.count(old))
 (t/(name+'.c')).write_text(source.replace(old,new))
for name,function,old,new in (
 ('bad-input','func_8001608C','func_800D8FA0(rdram, ctx);','/* missing replay override */'),
 ('bad-attract','gsattract_entrypoint_1','ctx->f8.fl = ctx->f6.fl + ctx->f0.fl;','ctx->f8.fl = ctx->f6.fl + 2*ctx->f0.fl;'),
 ('bad-gate','gsattract_entrypoint_1','if (ctx->r2 == 0) {','if (ctx->r2 != 0) {')):
 match=re.search(r'^RECOMP_FUNC void '+function+r'\(.*?(?=^RECOMP_FUNC |\Z)',source,re.M|re.S)
 assert match and old in match.group()
 altered=match.group().replace(old,new,1)
 (t/(name+'.c')).write_text(source[:match.start()]+altered+source[match.end():])
PY
# Generated MIPS LUI uses signed C shifts for its wrapping 32-bit result.
# GCC -fwrapv gives those operations the intended modulo semantics; retain
# ASan/UBSan (fatal on any remaining error) for the replay and RAM consumers.
"${CC:-gcc}" -std=c11 -O1 -fwrapv -fsanitize=address,undefined -fno-omit-frame-pointer \
 -I"$r/lib/N64Recomp/include" -I"$r/tests" "$t/generated.c" "$r/tests/demo_replay_adapters.c" "$r/tests/demo_replay_clock.c" -lm -o "$t/clock"
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 "$t/clock"
for mode in bad-span bad-index bad-timer;do
 "${CC:-gcc}" -std=c11 -O1 -fwrapv -fsanitize=address,undefined -fno-omit-frame-pointer \
  -I"$r/lib/N64Recomp/include" -I"$r/tests" "$t/$mode.c" "$r/tests/demo_replay_adapters.c" "$r/tests/demo_replay_clock.c" -lm -o "$t/$mode"
 if ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 "$t/$mode" > "$t/control.log" 2>&1;then
  echo "FAIL: consumer accepted $mode";exit 1
 else test "$?" -eq 1;fi
 rg -q '^FAIL line' "$t/control.log"
done
echo 'PASS: changed recorded-span byte, double-consumption and doubled game-timer controls fail the actual generated consumers'
python3 "$r/tests/correction_clock_records.py" "$r/tests/fixtures/odin-step86-clocks.txt" "$t/captured_clocks.h"
for mode in generated bad-input bad-attract bad-gate;do
 "${CC:-gcc}" -std=c11 -O1 -fwrapv -fsanitize=address,undefined -fno-omit-frame-pointer \
  -I"$r/lib/N64Recomp/include" -I"$r/tests" -I"$t" "$t/$mode.c" "$r/tests/demo_replay_adapters.c" "$r/tests/demo_replay_captured.c" -lm -o "$t/captured-$mode"
 if [ "$mode" = generated ];then
  ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 "$t/captured-$mode"
 else
  if ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 "$t/captured-$mode" > "$t/control.log" 2>&1;then
   echo "FAIL: captured consumer accepted $mode";exit 1
  else test "$?" -eq 1;fi
  rg -q '^captured clock failure:' "$t/control.log"
 fi
done
echo 'PASS: missing input override, doubled attract clock and reversed real attract gate fail generated consumers'
