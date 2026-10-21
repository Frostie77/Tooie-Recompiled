#!/usr/bin/env bash
set -euo pipefail
r=$(cd "$(dirname "$0")/.." && pwd)
t=$(mktemp -d)
trap 'rm -rf "$t"' EXIT
rt="$r/lib/rt64/src"
flags=(-std=c++17 -pthread -DNDEBUG -DHLSL_CPU -D__PRFCHWINTRIN_H -ffunction-sections -fdata-sections
 -I"$r/src" -I"$rt" -I"$rt/hle" -I"$rt/common" -I"$rt/contrib" -I"$rt/contrib/hlslpp/include"
 -I"$rt/contrib/json/single_include" -I"$rt/contrib/dxc/inc" -I"$rt/contrib/plume")
support=("$rt/hle/rt64_framebuffer_changes.cpp" "$rt/hle/rt64_framebuffer_storage.cpp"
 "$rt/preset/rt64_preset_scene.cpp" "$rt/preset/rt64_preset.cpp")
"${CXX:-g++}" "${flags[@]}" -fsanitize=address,undefined -fno-omit-frame-pointer \
 "$r/tests/render_interpolation.cpp" "$rt/common/rt64_math.cpp" "$rt/hle/rt64_game_frame.cpp" "$rt/hle/rt64_rigid_body.cpp" "${support[@]}" \
 -Wl,--gc-sections -o "$t/interpolation"
ASAN_OPTIONS=detect_leaks=0 "$t/interpolation"
if [ "${1:-}" = --baseline ]; then
 base=59d3ad0aca6eea957552219b83672fb82fe8a3b2
 mkdir -p "$t/base/hle" "$t/base/common"
 git -C "$r" show "$base:lib/rt64/src/common/rt64_math.cpp" > "$t/base/common/rt64_math.cpp"
 git -C "$r" show "$base:lib/rt64/src/common/rt64_math.h" > "$t/base/common/rt64_math.h"
 for file in rt64_game_frame.cpp rt64_game_frame.h rt64_rigid_body.cpp rt64_rigid_body.h; do
  git -C "$r" show "$base:lib/rt64/src/hle/$file" > "$t/base/hle/$file"
 done
 "${CXX:-g++}" -I"$t/base" "${flags[@]}" -fsanitize=address,undefined -fno-omit-frame-pointer \
  "$r/tests/render_interpolation.cpp" "$t/base/common/rt64_math.cpp" "$t/base/hle/rt64_game_frame.cpp" "$t/base/hle/rt64_rigid_body.cpp" "${support[@]}" \
  -Wl,--gc-sections -o "$t/baseline"
 for mode in first reset toggle rotation; do
  if ASAN_OPTIONS=detect_leaks=0 "$t/baseline" "$mode" >"$t/control.log" 2>&1; then
   echo "FAIL: tested base passed $mode";exit 1
  else test "$?" -eq 1 || { cat "$t/control.log";exit 1; };fi
  if [ "$mode" = rotation ];then grep -q 'FAIL.*bone.x' "$t/control.log";else grep -q 'FAIL.*length(bone)' "$t/control.log";fi
 done
 echo 'PASS: exact 59d3ad0 base fails all four cold/reset/mode/independent-warm-rotation geometry controls'
fi
# A second build exercises host SIMD, independently of GCC's scalar HLSL path.
if [ -n "${SIMD_CXX:-}" ]; then
 "$SIMD_CXX" "${flags[@]}" "$r/tests/render_interpolation.cpp" "$rt/common/rt64_math.cpp" "$rt/hle/rt64_game_frame.cpp" "$rt/hle/rt64_rigid_body.cpp" "${support[@]}" -Wl,--gc-sections -o "$t/simd"
 "$t/simd"
fi
