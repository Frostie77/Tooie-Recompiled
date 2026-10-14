#!/usr/bin/env bash
# Host recording tests of production Android code. No GPU work is executed.
set -euo pipefail
repo_root=$(cd "$(dirname "$0")/.." && pwd)
test_dir=$(mktemp -d)
trap 'rm -rf "$test_dir"' EXIT
plume_dir="$repo_root/lib/rt64/src/contrib/plume"
common_flags=(-std=c++17 -D__ANDROID__ -DNDEBUG
    -I"$repo_root/tests/android_headers" -I"$plume_dir"
    -I"$plume_dir/contrib/volk" -I"$plume_dir/contrib/Vulkan-Headers/include"
    -I"$plume_dir/contrib/VulkanMemoryAllocator/include")

build_and_record() {
    local mode=$1
    shift
    mkdir -p "$test_dir/log-$mode"
    "${CXX:-clang++}" "${common_flags[@]}" "$@" \
        "$plume_dir/plume_vulkan.cpp" "$repo_root/tests/android_vulkan_clear.cpp" \
        -ldl -o "$test_dir/$mode"
    if ! APP_FOLDER_PATH="$test_dir/log-$mode" "$test_dir/$mode" >"$test_dir/$mode.out" 2>&1; then
        cat "$test_dir/$mode.out"
        return 1
    fi
    echo "PASS: $mode recording, image-configuration queries, and swapchain usage validation"
}
expect_rejection() {
    local binary=$1 argument=$2 reason=$3
    if APP_FOLDER_PATH="$test_dir/log-$binary" "$test_dir/$binary" "$argument" >"$test_dir/rejection.out" 2>&1; then
        echo "FAIL: $binary accepted $argument" >&2
        exit 1
    else
        result=$?
        test "$result" -eq 1 || { cat "$test_dir/rejection.out"; exit 1; }
    fi
    grep -q "reason=$reason" "$test_dir/rejection.out"
}

build_and_record proposal -DTOOIE_VULKAN_QUEUE_ISOLATION_TEST=1
build_and_record legacy-transfer -DTOOIE_VULKAN_TRANSFER_CLEAR_TEST=1 -DTOOIE_VULKAN_QUEUE_ISOLATION_TEST=1
build_and_record load-op-control -DTOOIE_VULKAN_TRANSFER_CLEAR_TEST=0 -DTOOIE_VULKAN_QUEUE_ISOLATION_TEST=1
build_and_record barrier-control -DTOOIE_VULKAN_TRANSFER_CLEAR_TEST=0 -DTOOIE_VULKAN_CLEAR_BARRIER_TEST=1 -DTOOIE_VULKAN_QUEUE_ISOLATION_TEST=1
build_and_record fallback-comparison -DTOOIE_VULKAN_ALLOW_ATTACHMENT_CLEAR_FALLBACK=1
build_and_record legacy-skip-comparison -DTOOIE_VULKAN_ALLOW_ATTACHMENT_CLEAR_FALLBACK=1 -DTOOIE_VULKAN_TRANSFER_CLEAR_TEST=1

# Each child prohibits begin/clear/barrier/submit callbacks before its expected
# rejection, so a return through the former fallback cannot masquerade as a pass.
for reason in partial-rectangles explicit-attachment-view not-2d linear-tiling \
    multiple-mip-levels multiple-array-layers framebuffer-extent-mismatch \
    missing-transfer-dst-usage attachment-format-mismatch unsupported-clear-format \
    format-transfer-dst-unsupported null-image null-image-view null-framebuffer \
    null-render-pass layout-not-color-write missing-graphics-stage; do
    expect_rejection proposal "--reject=$reason" "$reason"
done
expect_rejection legacy-transfer --reject=legacy-requires-external-image legacy-requires-external-image
expect_rejection legacy-transfer --reject=legacy-requires-single-sample legacy-requires-single-sample
expect_rejection load-op-control --reject=partial-rectangles partial-rectangles
expect_rejection barrier-control --reject=partial-rectangles partial-rectangles
expect_rejection fallback-comparison --invalid-state layout-not-color-write

# These instantiate a real production constructor with an unsupported mocked
# capability result; it must stop before reaching VMA allocation.
expect_rejection proposal --image-reject=query image-format-query-failed
expect_rejection proposal --image-reject=samples sample-count-unsupported
expect_rejection proposal --image-reject=extent image-extent-unsupported
expect_rejection proposal --image-reject=mips mip-level-count-unsupported
expect_rejection proposal --image-reject=layers array-layer-count-unsupported

mkdir -p "$test_dir/log-late"
APP_FOLDER_PATH="$test_dir/log-late" "$test_dir/proposal" --late-categories >"$test_dir/late.out" 2>&1
# Category records must survive 32 earlier unrelated clears on the same thread.
grep -q 'owned=0 swapchain=1' "$test_dir/log-late/banjotooi-debug.log"
grep -q 'samples=8 .*owned=1 swapchain=0' "$test_dir/log-late/banjotooi-debug.log"

if "$test_dir/proposal" --timeout >"$test_dir/timeout.log" 2>&1; then
    echo 'FAIL: timed-out fence was accepted' >&2
    exit 1
else
    result=$?
    test "$result" -eq 1
fi
grep -q 'vkWaitForFences failed with error code 0x2' "$test_dir/timeout.log"
grep -q 'path=load-op-control' "$test_dir/log-load-op-control/banjotooi-debug.log"
grep -q 'no render pass or clear' "$test_dir/log-barrier-control/banjotooi-debug.log"

# A diagnostic build must not opt into the preserved unsafe comparison fallback.
if "${CXX:-clang++}" "${common_flags[@]}" -DTOOIE_VULKAN_QUEUE_ISOLATION_TEST=1 \
    -DTOOIE_VULKAN_ALLOW_ATTACHMENT_CLEAR_FALLBACK=1 -fsyntax-only \
    "$plume_dir/plume_vulkan.cpp" >"$test_dir/conflict.out" 2>&1; then
    echo 'FAIL: diagnostic fallback flag combination compiled' >&2
    exit 1
fi
grep -q 'fallback comparison is forbidden' "$test_dir/conflict.out"
if "${CXX:-clang++}" "${common_flags[@]}" -UNDEBUG \
    -DTOOIE_VULKAN_ALLOW_ATTACHMENT_CLEAR_FALLBACK=1 -fsyntax-only \
    "$plume_dir/plume_vulkan.cpp" >"$test_dir/debug-conflict.out" 2>&1; then
    echo 'FAIL: debug fallback comparison compiled' >&2
    exit 1
fi
grep -q 'fallback comparison is forbidden' "$test_dir/debug-conflict.out"
echo 'PASS: exact rejection reasons, no rejected clear recording/submission, late category logs, retained controls, and five-second timeout'
