#!/usr/bin/env bash
set -euo pipefail
r=$(cd "$(dirname "$0")/.." && pwd)
t=$(mktemp -d)
trap 'rm -rf "$t"' EXIT
: "${GLSLC:?Set GLSLC to glslc}"
"$GLSLC" --target-env=vulkan1.2 -fshader-stage=comp "$r/tests/android_vulkan_gpu_tiles.comp" -o "$t/tiles.spv"
if [ -n "${SPIRV_VAL:-}" ]; then "$SPIRV_VAL" --target-env vulkan1.2 "$t/tiles.spv"; fi
python3 - "$t" <<'PY'
import struct,sys
from pathlib import Path
p=Path(sys.argv[1]);d=(p/'tiles.spv').read_bytes();v=struct.unpack('<%dI'%(len(d)//4),d)
(p/'gpu-tiles-test.spirv.h').write_text('static const uint32_t GPUTilesTest[] = {'+','.join(hex(x) for x in v)+'};\n')
PY
p="$r/lib/rt64/src/contrib/plume"
flags=(-std=c++17 -pthread -D__ANDROID__ -DNDEBUG -DHLSL_CPU -D__PRFCHWINTRIN_H -ffunction-sections -fdata-sections
 -I"$t" -I"$r/tests/android_headers" -I"$p" -I"$p/contrib/volk" -I"$p/contrib/Vulkan-Headers/include" -I"$p/contrib/VulkanMemoryAllocator/include"
 -I"$r/lib/rt64/src" -I"$r/lib/rt64/src/render" -I"$r/lib/rt64/src/contrib" -I"$r/lib/rt64/src/contrib/hlslpp/include"
 -I"$r/lib/rt64/src/contrib/json/single_include" -I"$r/lib/rt64/src/contrib/dxc/inc")
sources=("$p/plume_vulkan.cpp" "$r/tests/android_vulkan_gpu_tiles.cpp" "$r/lib/rt64/src/render/rt64_buffer_uploader.cpp"
 "$r/lib/rt64/src/render/rt64_render_worker.cpp" "$r/lib/rt64/src/common/rt64_thread.cpp")
"${CXX:-clang++}" "${flags[@]}" "${sources[@]}" "$r/lib/rt64/src/render/rt64_framebuffer_renderer.cpp" -Wl,--gc-sections -ldl -o "$t/tiles"
APP_FOLDER_PATH="$t" MESA_SHADER_CACHE_DIR="$t/cache" "$t/tiles"
while IFS='|' read -r mode reason; do
 if APP_FOLDER_PATH="$t" MESA_SHADER_CACHE_DIR="$t/cache" "$t/tiles" "$mode" >"$t/failure.log" 2>&1; then
  echo "FAIL: accepted tile lifecycle $mode" >&2;exit 1
 else
  test "$?" -eq 1 || { cat "$t/failure.log";exit 1; }
 fi
 grep -q "reason=$reason" "$t/failure.log" || { cat "$t/failure.log";exit 1; }
 if grep -q 'TEST tile write:\|VALIDATION:' "$t/failure.log"; then cat "$t/failure.log";exit 1;fi
done <<'CASES'
missing-empty|null-buffer-object
missing-real|null-buffer-object
empty-as-real|gpu-tile-buffer-not-prepared
stale|buffer-not-live-or-wrong-device
source-missing|gpu-tile-source-missing
range-invalid|gpu-tile-upload-range-invalid
prefix-missing|gpu-tile-upload-prefix-not-prepared
size-overflow|gpu-tile-size-overflow
growth-overflow|gpu-tile-size-overflow
range-small|gpu-tile-buffer-range-too-small
CASES
echo 'PASS: ten unsafe tile lifecycle controls stop before binding 4 update, preserving exact null/stale guards'
if [ "${1:-}" = --baseline ]; then
 mkdir -p "$t/base/render"
 base=3a836de6c1484b866b66408d5d127b4282a84a4c
 git -C "$r" show "$base:lib/rt64/src/render/rt64_framebuffer_renderer.cpp" > "$t/base/render/rt64_framebuffer_renderer.cpp"
 git -C "$r" show "$base:lib/rt64/src/render/rt64_framebuffer_renderer.h" > "$t/base/render/rt64_framebuffer_renderer.h"
 "${CXX:-clang++}" -I"$t/base" -DTOOIE_GPU_TILES_BASELINE_CONTROL=1 "${flags[@]}" "${sources[@]}" "$t/base/render/rt64_framebuffer_renderer.cpp" -Wl,--gc-sections -ldl -o "$t/baseline"
 if APP_FOLDER_PATH="$t" MESA_SHADER_CACHE_DIR="$t/cache" "$t/baseline" >"$t/baseline.log" 2>&1; then echo 'FAIL: baseline accepted fresh empty GPU tiles';exit 1;else test "$?" -eq 1;fi
 grep -q 'Descriptor rejected: reason=null-buffer-object .*ctx=framebuffer-shader-views .*index=3 binding=4 type=7' "$t/baseline.log"
 if grep -q 'TEST tile write:\|VALIDATION:' "$t/baseline.log";then cat "$t/baseline.log";exit 1;fi
 echo 'PASS: exact tested-base framebuffer caller reproduces null-buffer-object index=3 binding=4 type=7 with fresh empty DrawBuffers'
fi
