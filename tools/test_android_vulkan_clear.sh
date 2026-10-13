#!/usr/bin/env bash
# Host recording test of the production Android backend; no GPU is used.
set -euo pipefail
repo_root=$(cd "$(dirname "$0")/.." && pwd)
test_dir=$(mktemp -d)
trap 'rm -rf "$test_dir"' EXIT
plume_dir="$repo_root/lib/rt64/src/contrib/plume"
"${CXX:-clang++}" -std=c++17 -D__ANDROID__ -DNDEBUG \
    -I"$repo_root/tests/android_headers" -I"$plume_dir" \
    -I"$plume_dir/contrib/volk" \
    -I"$plume_dir/contrib/Vulkan-Headers/include" \
    -I"$plume_dir/contrib/VulkanMemoryAllocator/include" \
    "$plume_dir/plume_vulkan.cpp" "$repo_root/tests/android_vulkan_clear.cpp" \
    -ldl -o "$test_dir/android-vulkan-clear-test"
"$test_dir/android-vulkan-clear-test"
# Negative control: a timed-out fence must terminate the process with failure.
if "$test_dir/android-vulkan-clear-test" --timeout >"$test_dir/timeout.log" 2>&1; then
    echo "FAIL: timed-out fence was accepted" >&2
    exit 1
else
    result=$?
    test "$result" -eq 1
fi
grep -q 'vkWaitForFences failed with error code 0x2' "$test_dir/timeout.log"
echo 'PASS: timed-out fence terminates with failure'
