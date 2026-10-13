#!/usr/bin/env bash
# Optional actual Vulkan execution on a selected CPU driver, not an Android GPU.
# Requires the Vulkan loader, Mesa lavapipe (or another CPU driver supporting 4x),
# and VK_LAYER_KHRONOS_validation. Select a driver with VK_DRIVER_FILES if needed.
set -euo pipefail
repo_root=$(cd "$(dirname "$0")/.." && pwd)
test_dir=$(mktemp -d)
trap 'rm -rf "$test_dir"' EXIT
plume_dir="$repo_root/lib/rt64/src/contrib/plume"
mkdir -p "$test_dir/mesa-cache"
for mode in proposal load-op-control; do
    extra_flags=()
    if [ "$mode" = load-op-control ]; then
        extra_flags+=(-DTOOIE_VULKAN_TRANSFER_CLEAR_TEST=0)
    fi
    "${CXX:-clang++}" -std=c++17 -D__ANDROID__ -DNDEBUG "${extra_flags[@]}" \
        -I"$repo_root/tests/android_headers" -I"$plume_dir" \
        -I"$plume_dir/contrib/volk" \
        -I"$plume_dir/contrib/Vulkan-Headers/include" \
        -I"$plume_dir/contrib/VulkanMemoryAllocator/include" \
        "$plume_dir/plume_vulkan.cpp" "$repo_root/tests/android_vulkan_transfer_validation.cpp" \
        -ldl -o "$test_dir/$mode"
    echo "Validating $mode with actual Vulkan calls"
    MESA_SHADER_CACHE_DIR="$test_dir/mesa-cache" "$test_dir/$mode"
done
