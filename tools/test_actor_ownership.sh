#!/usr/bin/env bash
set -euo pipefail
r=$(cd "$(dirname "$0")/.." && pwd)
t=$(mktemp -d);trap 'rm -rf "$t"' EXIT
python3 "$r/tools/prepare_actor_ownership.py" "$r" "$t/generated.c"
rt="$r/lib/rt64/src"
flags=(-O1 -fwrapv -DNDEBUG -D__ANDROID__ -DHLSL_CPU -D__PRFCHWINTRIN_H -pthread -ffunction-sections -fdata-sections
 -I"$r/src" -I"$r/include" -I"$r/RecompiledFuncs" -I"$r/tests/android_headers" -I"$r/lib/N64Recomp/include"
 -I"$r/lib/N64ModernRuntime/librecomp/include" -I"$r/lib/N64ModernRuntime/librecomp/src"
 -I"$r/lib/N64ModernRuntime/ultramodern/include" -I"$r/lib/N64ModernRuntime/thirdparty"
 -I"$r/lib/N64ModernRuntime/thirdparty/concurrentqueue" -I"$r/lib/N64Recomp/lib/fmt/include" -I"$r/lib/N64Recomp/lib/tomlplusplus"
 -I"$rt" -I"$rt/contrib" -I"$rt/contrib/hlslpp/include" -I"$rt/contrib/json/single_include" -I"$rt/contrib/plume"
 -I"$rt/contrib/dxc/inc" -I"$rt/contrib/mupen64plus-core/src/api" -I"$r/lib/rt64/include")
san=(-fsanitize=address,undefined -fno-omit-frame-pointer)
"${CC:-gcc}" -std=gnu11 "${flags[@]}" "${san[@]}" -c "$t/generated.c" -o "$t/generated.o"
"${CXX:-g++}" -std=c++20 "${flags[@]}" "${san[@]}" -c "$r/src/recomp_api.cpp" -o "$t/api.o"
"${CXX:-g++}" -std=c++20 "${flags[@]}" "${san[@]}" -DTOOIE_ACTOR_OWNERSHIP_MAIN "$r/tests/actor_ownership.cpp" "$t/api.o" "$t/generated.o" -Wl,--gc-sections -o "$t/test"
export ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
"$t/test"
git -C "$r" show 'ffd520a6ac40ed92cedfbc6026f8bda51cdefcb6:src/recomp_api.cpp' > "$t/parent.cpp"
"${CXX:-g++}" -std=c++20 "${flags[@]}" "${san[@]}" -c "$t/parent.cpp" -o "$t/parent.o"
"${CXX:-g++}" -std=c++20 "${flags[@]}" "${san[@]}" -DTOOIE_ACTOR_OWNERSHIP_MAIN -DTOOIE_ACTOR_PARENT_BRIDGE "$r/tests/actor_ownership.cpp" "$t/parent.o" "$t/generated.o" -Wl,--gc-sections -o "$t/parent"
if "$t/parent" > "$t/control.log" 2>&1;then echo 'FAIL: parent emitted actor root identity';exit 1;else test "$?" -eq 1;fi
rg -q '^actor ownership failure.*guest::word\(r,0x805F0000\)==0x6400000C' "$t/control.log"
echo 'PASS: exact ffd520a native root bridge fails actor identity assertion despite valid ownership adapters'
