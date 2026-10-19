#!/usr/bin/env bash
set -euo pipefail
r=$(cd "$(dirname "$0")/.." && pwd)
t=$(mktemp -d);trap 'rm -rf "$t"' EXIT
b=${TOOIE_RECOMP_BUILD_DIR:-$t/core}
cmake -S "$r/lib/N64Recomp" -B "$b" -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build "$b" --target N64Recomp -j"${TOOIE_TEST_JOBS:-2}"
git -C "$r" show '9fbd60f2455edd23a555df2beaa8fdce179716fa:lib/N64Recomp/src/analysis.cpp' > "$t/parent.cpp"
flags=(-std=c++20 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer
 -I"$r/lib/N64Recomp/include" -I"$r/lib/N64Recomp/src" -I"$r/lib/N64Recomp/lib/fmt/include"
 -I"$r/lib/N64Recomp/lib/rabbitizer/include" -I"$r/lib/N64Recomp/lib/rabbitizer/cplusplus/include")
for mode in fixed parent;do
 source="$r/lib/N64Recomp/src/analysis.cpp"
 if [ "$mode" = parent ];then source="$t/parent.cpp";fi
 "${CXX:-g++}" "${flags[@]}" "$r/tests/jump_table_analysis.cpp" "$source" \
  "$b/libN64Recomp.a" "$b/libSymbolLists.a" "$b/librabbitizer.a" "$b/lib/fmt/libfmt.a" -o "$t/$mode"
 ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "$t/$mode" "$mode"
done
