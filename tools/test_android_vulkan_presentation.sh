#!/usr/bin/env bash
# Actual production presentation shaders/pipeline factories on a CPU Vulkan driver.
set -euo pipefail
repo_root=$(cd "$(dirname "$0")/.." && pwd)
test_dir=$(mktemp -d)
trap 'rm -rf "$test_dir"' EXIT
python3 "$repo_root/tools/compile_vulkan_presentation_shaders.py" \
    --dxc "${DXC:-dxc}" --spirv-val "${SPIRV_VAL:-spirv-val}" --output "$test_dir/shaders"
plume_dir="$repo_root/lib/rt64/src/contrib/plume"
"${CXX:-clang++}" -std=c++17 -D__ANDROID__ -DNDEBUG \
    -I"$repo_root/tests/android_headers" -I"$plume_dir" -I"$test_dir/shaders" \
    -I"$plume_dir/contrib/volk" -I"$plume_dir/contrib/Vulkan-Headers/include" \
    -I"$plume_dir/contrib/VulkanMemoryAllocator/include" \
    "$plume_dir/plume_vulkan.cpp" "$repo_root/tests/android_vulkan_presentation_validation.cpp" \
    -ldl -o "$test_dir/presentation"
mkdir -p "$test_dir/mesa-cache"
MESA_SHADER_CACHE_DIR="$test_dir/mesa-cache" "$test_dir/presentation"
