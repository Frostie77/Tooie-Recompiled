#!/usr/bin/env bash
set -euo pipefail
r=$(cd "$(dirname "$0")/.." && pwd)
t=$(mktemp -d);trap 'rm -rf "$t"' EXIT
b=${TOOIE_RECOMP_BUILD_DIR:-$t/core}
cmake -S "$r/lib/N64Recomp" -B "$b" -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build "$b" --target N64Recomp -j"${TOOIE_TEST_JOBS:-2}"
python3 "$r/tests/prepare_first_person_dispatch.py" "$r" "$t"
flags=(-std=c++20 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer
 -I"$r/lib/N64Recomp/include" -I"$r/lib/N64Recomp/src" -I"$r/lib/N64Recomp/lib/fmt/include"
 -I"$r/lib/N64Recomp/lib/rabbitizer/include" -I"$r/lib/N64Recomp/lib/rabbitizer/cplusplus/include")
"${CXX:-g++}" "${flags[@]}" "$t/generate.cpp" "$b/libN64Recomp.a" "$b/libSymbolLists.a" "$b/librabbitizer.a" "$b/lib/fmt/libfmt.a" -o "$t/generate"
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 "$t/generate" "$t/fixed.inc"
apply=()
if [ "${TOOIE_FIRST_PERSON_APPLY:-0}" = 1 ];then apply=(--apply);fi
python3 "$r/tools/check_first_person_dispatch.py" "$r" "$t/original.inc" "$t/fixed.inc" "${apply[@]}"
TOOIE_TEST_ROOT="$r" python3 - "$t" <<'PYTEST'
from pathlib import Path
import json,re,sys
r=Path(__import__('os').environ['TOOIE_TEST_ROOT']);t=Path(sys.argv[1]);a=(t/'original.inc').read_text()
targets=[int(v,16) for v in json.loads((r/'tests/fixtures/bsfirstp_table_us.json').read_text())['raw_offsets']]
# Independent MIPS-style reference keeps the original RAM load and dispatches
# on its loaded target, while the real generator uses the preserved table index.
reference=a.replace('bsfirstp_entrypoint_26(','bsfirstp_reference(',1)
reference=reference.replace('    LOOKUP_FUNC(ctx->r15)(rdram, ctx);\n    return;',
 '    switch ((uint32_t)ctx->r15 - (uint32_t)section_addresses[108]) {\n'+''.join('        case 0x%X: goto L_%08X;\n'%(v,0x80800000+v) for v in sorted(set(targets)))+'        default: switch_error(__func__, 0x808059F8, 0x8080A494);\n    }')
for v in sorted(set(targets)):
 pc=0x80800000+v;label='L_%08X:'%pc
 if label+'\n' not in reference:reference=reference.replace('    // 0x%08X: '%pc,label+'\n    // 0x%08X: '%pc,1)
for mode,body in [('fixed',(t/'fixed.inc').read_text()),('original',a),('reference',reference)]:
 (t/(mode+'.c')).write_text('#include "tooie_recomp.h"\n#include "funcs.h"\n'+body)
# Mapping and epilogue mutations must fail against that independent reference.
b=(t/'fixed.c').read_text()
(t/'bad-case.c').write_text(b.replace('case 46: goto L_80805AE0','case 46: goto L_80805A20'))
(t/'bad-frame.c').write_text(b.replace('ctx->r29 = ADD32(ctx->r29, 0XF8);','ctx->r29 = ADD32(ctx->r29, 0XF4);'))
PYTEST

cflags=(-O1 -g -fwrapv -fno-strict-aliasing -D__ANDROID__ -DNDEBUG -fsanitize=address,undefined -fno-omit-frame-pointer
 -I"$t" -I"$r/include" -I"$r/RecompiledFuncs" -I"$r/lib/N64ModernRuntime/N64Recomp/include")
"${CC:-gcc}" -std=gnu11 "${cflags[@]}" -c "$t/reference.c" -o "$t/reference.o"
for mode in fixed original bad-case bad-frame;do
 "${CC:-gcc}" -std=gnu11 "${cflags[@]}" -c "$t/$mode.c" -o "$t/$mode.o"
 "${CXX:-g++}" -std=c++20 "${cflags[@]}" "$r/tests/first_person_dispatch.cpp" "$t/$mode.o" "$t/reference.o" -o "$t/$mode"
done
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 "$t/fixed"
set +e
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 "$t/original" parent > "$t/original.log" 2>&1
status=$?
set -e
test "$status" -eq 18
rg -q '^EXPECTED parent external lookup:' "$t/original.log"
echo 'PASS: original committed consumer reaches real external lookup at an interior target; no fake function entry or skipped case'

for mode in bad-case bad-frame;do
 if ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 "$t/$mode" > "$t/$mode.log" 2>&1;then
  echo "FAIL: $mode mutation survived";exit 1
 else test "$?" -eq 3;fi
 rg -q '^first-person failure' "$t/$mode.log"
done
echo 'PASS: swapped captured case and damaged stack epilogue fail independent loaded-target reference'
desktop=()
for flag in "${cflags[@]}";do
 if [ "$flag" != -D__ANDROID__ ];then desktop+=("$flag");fi
done
for source in fixed reference;do
 "${CC:-gcc}" -std=gnu11 "${desktop[@]}" -c "$t/$source.c" -o "$t/desktop-$source.o"
done
"${CXX:-g++}" -std=c++20 "${desktop[@]}" "$r/tests/first_person_dispatch.cpp" "$t/desktop-fixed.o" "$t/desktop-reference.o" -o "$t/desktop"
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 "$t/desktop"
echo 'PASS: desktop complete consumer uses the same verified USA cases and preserves all non-table behavior'
