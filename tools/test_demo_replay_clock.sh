#!/usr/bin/env bash
set -euo pipefail
r=$(cd "$(dirname "$0")/.." && pwd)
t=$(mktemp -d);trap 'rm -rf "$t"' EXIT
python3 - "$r" "$t" <<'PY'
from pathlib import Path
import re,sys
r,t=map(Path,sys.argv[1:]);names=[
 'glrecord_entrypoint_0','func_800A7D30','func_80014F4C',
 'func_800D8FA0','func_800D8F50','func_800D8F78','func_800D9004',
 'func_800F0D90','func_800F0DB8','static_3_800F0DA8']
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
(t/'generated.c').write_text('#include "recomp.h"\n'+declarations+'\n'.join(functions))
source=(t/'generated.c').read_text()
for name,old,new in (
 ('bad-span','ctx->r12 = MEM_BU(ctx->r3, 0X4);','ctx->r12 = MEM_BU(ctx->r3, 0X5);'),
 ('bad-index','ctx->r24 = ADD32(ctx->r2, 0X1);','ctx->r24 = ADD32(ctx->r2, 0X2);')):
 assert source.count(old)==1,(name,source.count(old))
 (t/(name+'.c')).write_text(source.replace(old,new))
PY
# Generated MIPS LUI uses signed C shifts for its wrapping 32-bit result.
# GCC -fwrapv gives those operations the intended modulo semantics; retain
# ASan/UBSan (fatal on any remaining error) for the replay and RAM consumers.
"${CC:-gcc}" -std=c11 -O1 -fwrapv -fsanitize=address,undefined -fno-omit-frame-pointer \
 -I"$r/lib/N64Recomp/include" "$t/generated.c" "$r/tests/demo_replay_clock.c" -lm -o "$t/clock"
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 "$t/clock"
for mode in bad-span bad-index;do
 "${CC:-gcc}" -std=c11 -O1 -fwrapv -fsanitize=address,undefined -fno-omit-frame-pointer \
  -I"$r/lib/N64Recomp/include" "$t/$mode.c" "$r/tests/demo_replay_clock.c" -lm -o "$t/$mode"
 if ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 "$t/$mode" > "$t/control.log" 2>&1;then
  echo "FAIL: consumer accepted $mode";exit 1
 else test "$?" -eq 1;fi
 rg -q '^FAIL line' "$t/control.log"
done
echo 'PASS: changed recorded-span byte and double-consumption controls fail the actual generated consumer'
