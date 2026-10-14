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
    grep -q 'PASS: Android WSI lifecycle BGRA/RGBA' "$test_dir/$mode.out"
    grep -q 'PASS: failed/incomplete surface enumeration' "$test_dir/$mode.out"
    grep -q 'PASS: VI nearest/linear/pixel and UI' "$test_dir/$mode.out"
    grep -q 'Surface format selected: requested=44 selected=37 colorSpace=0' "$test_dir/log-$mode/banjotooi-debug.log"
    grep -q 'UI pipeline: path=composite format=20 samples=1' "$test_dir/log-$mode/banjotooi-debug.log"
    grep -q 'Surface format request: vkFormat=44' "$test_dir/log-$mode/banjotooi-debug.log"
    grep -q 'Surface format pair: attempt=1 index=2 format=44 colorSpace=0 complete=1' "$test_dir/log-$mode/banjotooi-debug.log"
    grep -q 'Surface format data: attempt=1 result=5 written=1 capacity=1' "$test_dir/log-$mode/banjotooi-debug.log"
    grep -q 'Surface format data: attempt=2 result=0 written=2 capacity=2' "$test_dir/log-$mode/banjotooi-debug.log"
    if grep -q 'Swapchain record: format=0 ' "$test_dir/log-$mode/banjotooi-debug.log"; then
        echo 'FAIL: undefined chosen image format was logged' >&2; exit 1
    fi
    echo "PASS: $mode recording, surface enumeration/initialization/format consistency, image queries and swapchain usage validation"
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
    grep -q "reason=$reason" "$test_dir/rejection.out" || { cat "$test_dir/rejection.out"; echo "FAIL: expected rejection $reason"; exit 1; }
}

build_and_record proposal -DTOOIE_VULKAN_QUEUE_ISOLATION_TEST=1
build_and_record legacy-transfer -DTOOIE_VULKAN_TRANSFER_CLEAR_TEST=1 -DTOOIE_VULKAN_QUEUE_ISOLATION_TEST=1
build_and_record load-op-control -DTOOIE_VULKAN_TRANSFER_CLEAR_TEST=0 -DTOOIE_VULKAN_QUEUE_ISOLATION_TEST=1
build_and_record barrier-control -DTOOIE_VULKAN_TRANSFER_CLEAR_TEST=0 -DTOOIE_VULKAN_CLEAR_BARRIER_TEST=1 -DTOOIE_VULKAN_QUEUE_ISOLATION_TEST=1
build_and_record fallback-comparison -DTOOIE_VULKAN_ALLOW_ATTACHMENT_CLEAR_FALLBACK=1
build_and_record legacy-skip-comparison -DTOOIE_VULKAN_ALLOW_ATTACHMENT_CLEAR_FALLBACK=1 -DTOOIE_VULKAN_TRANSFER_CLEAR_TEST=1

# Each child prohibits begin/clear/barrier/submit callbacks before its expected
# rejection, so a return through the former fallback cannot masquerade as a pass.
for reason in explicit-attachment-view not-2d linear-tiling \
    multiple-mip-levels multiple-array-layers framebuffer-extent-mismatch \
    missing-transfer-dst-usage attachment-format-mismatch unsupported-clear-format \
    format-transfer-dst-unsupported null-image null-image-view null-framebuffer \
    null-render-pass layout-not-color-write missing-graphics-stage; do
    expect_rejection proposal "--reject=$reason" "$reason"
done
for reason in null-image null-view format-mismatch; do
    expect_rejection proposal "--framebuffer-reject=$reason" invalid-color-attachment-image-view-or-format
done
for reason in unsupported-draw-format format-color-attachment-unsupported \
    missing-color-attachment-usage null-attachment-view layout-not-color-write missing-graphics-stage; do
    expect_rejection proposal "--draw-reject=$reason" "$reason"
done
for geometry in inverted-x inverted-y zero-width zero-height outside-malformed; do
    expect_rejection proposal "--draw-reject=$geometry" malformed-rectangle
    grep -q 'rectIndex=1 rectCount=2 originalRect=.* fbExtent=640x480' "$test_dir/rejection.out"
done
expect_rejection proposal --draw-reject=framebuffer-extent-unrepresentable framebuffer-extent-unrepresentable
expect_rejection proposal --draw-reject=empty-framebuffer empty-framebuffer
expect_rejection proposal --draw-reject=outside-unsupported-format unsupported-draw-format
expect_rejection proposal --draw-reject=outside-unsafe-layout layout-not-color-write
grep -q 'PASS: clipped/empty/mixed rectangles.*samples=2' "$test_dir/proposal.out"
grep -q 'PASS: clipping domain fbExtent=320x241 imageExtent=320x241 viewMip=0 samples=2' "$test_dir/proposal.out"
grep -q 'PASS: clipping domain fbExtent=320x241 imageExtent=640x482 viewMip=1 samples=1' "$test_dir/proposal.out"
grep -q 'originalRect=-40,-30..-10,-1 .*intersection=empty-no-draw fbExtent=640x480' "$test_dir/log-proposal/banjotooi-debug.log"
grep -q 'originalRect=-5,-7..9,13 rect=0,0..9,13 intersection=nonempty fbExtent=640x480' "$test_dir/log-proposal/banjotooi-debug.log"
# Exercise the production isolation entry point, including startup failures.
for mode in proposal legacy-transfer load-op-control barrier-control; do
    for argument in --isolation --isolation-rgba --isolation-acquire-fails --isolation-surface-fails; do
        APP_FOLDER_PATH="$test_dir/log-$mode" "$test_dir/$mode" "$argument" >"$test_dir/isolation.out" 2>&1 || { cat "$test_dir/isolation.out"; exit 1; }
    done
done
mkdir -p "$test_dir/log-isolation-timeout"
if APP_FOLDER_PATH="$test_dir/log-isolation-timeout" "$test_dir/proposal" --isolation-timeout >"$test_dir/isolation-timeout.out" 2>&1; then
    echo 'FAIL: isolation accepted fence timeout' >&2; exit 1
else
    test "$?" -eq 1
fi
grep -q 'DIAG A: empty submit' "$test_dir/log-isolation-timeout/banjotooi-debug.log"
if grep -q 'DIAG B:' "$test_dir/log-isolation-timeout/banjotooi-debug.log"; then
    echo 'FAIL: isolation continued after A timeout' >&2; exit 1
fi
# Guard scheduling location as well as command sequencing: never move the test
# back behind shader setup/workload threads or into per-frame raster work.
python3 - "$repo_root" <<'PYTEST'
from pathlib import Path
import sys
root = Path(sys.argv[1])
s = (root / 'lib/rt64/src/hle/rt64_application.cpp').read_text()
start = s.index('RunVulkanQueueDiagnostic(')
assert s.index('createSwapChain(swapChainDesc)') < start < s.index('shaderLibrary->setupCommonShaders')
assert start < s.index('workloadQueue->setup(') < s.index('presentQueue->setup(') < s.index('state->setup(')
assert 'RunVulkanQueueDiagnostic' not in (root / 'lib/rt64/src/hle/rt64_present_queue.cpp').read_text()
PYTEST
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
expect_rejection proposal --pipeline-reject=bgra attachment-blend-unsupported
expect_rejection proposal --pipeline-reject=rgba attachment-blend-unsupported

mkdir -p "$test_dir/log-late"
APP_FOLDER_PATH="$test_dir/log-late" "$test_dir/proposal" --late-categories >"$test_dir/late.out" 2>&1
# Category records must survive 32 earlier unrelated clears on the same thread.
grep -q 'owned=0 swapchain=1' "$test_dir/log-late/banjotooi-debug.log"
grep -q 'samples=2 .*owned=1 swapchain=0' "$test_dir/log-late/banjotooi-debug.log"

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
