#!/usr/bin/env bash
# Host recording tests of production Android code. No GPU work is executed.
set -euo pipefail
repo_root=$(cd "$(dirname "$0")/.." && pwd)
test_dir=$(mktemp -d)
trap 'rm -rf "$test_dir"' EXIT
plume_dir="$repo_root/lib/rt64/src/contrib/plume"

build_and_record() {
    local mode=$1
    shift
    mkdir -p "$test_dir/log-$mode"
    "${CXX:-clang++}" -std=c++17 -D__ANDROID__ -DNDEBUG "$@" \
        -I"$repo_root/tests/android_headers" -I"$plume_dir" \
        -I"$plume_dir/contrib/volk" \
        -I"$plume_dir/contrib/Vulkan-Headers/include" \
        -I"$plume_dir/contrib/VulkanMemoryAllocator/include" \
        "$plume_dir/plume_vulkan.cpp" "$repo_root/tests/android_vulkan_clear.cpp" \
        -ldl -o "$test_dir/$mode"
    APP_FOLDER_PATH="$test_dir/log-$mode" "$test_dir/$mode"
}

build_and_record proposal
build_and_record legacy-transfer -DTOOIE_VULKAN_TRANSFER_CLEAR_TEST=1
build_and_record load-op-control -DTOOIE_VULKAN_TRANSFER_CLEAR_TEST=0
build_and_record barrier-control -DTOOIE_VULKAN_TRANSFER_CLEAR_TEST=0 -DTOOIE_VULKAN_CLEAR_BARRIER_TEST=1

# Negative controls distinguish a checked recording from unconditional success.
if "$test_dir/proposal" --timeout >"$test_dir/timeout.log" 2>&1; then
    echo 'FAIL: timed-out fence was accepted' >&2
    exit 1
else
    result=$?
    test "$result" -eq 1
fi
grep -q 'vkWaitForFences failed with error code 0x2' "$test_dir/timeout.log"
if APP_FOLDER_PATH="$test_dir/log-proposal" "$test_dir/proposal" --invalid-state; then
    echo 'FAIL: an invalid tracked layout reached the transfer clear' >&2
    exit 1
else
    result=$?
    test "$result" -eq 1
fi
grep -q 'invalid tracked state' "$test_dir/log-proposal/banjotooi-debug.log"
grep -q 'path=transfer' "$test_dir/log-proposal/banjotooi-debug.log"
grep -q 'path=load-op-control' "$test_dir/log-load-op-control/banjotooi-debug.log"
grep -q 'no render pass or clear' "$test_dir/log-barrier-control/banjotooi-debug.log"
echo 'PASS: diagnostic modes and recorded state; timeout and invalid-layout controls terminate with failure'
