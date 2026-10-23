#!/usr/bin/env bash
set -euo pipefail
repo_root=$(cd "$(dirname "$0")/.." && pwd)
test_dir=$(mktemp -d)
trap 'rm -rf "$test_dir"' EXIT
p="$repo_root/lib/rt64/src/contrib/plume"
"${CXX:-clang++}" -std=c++17 -pthread -DNDEBUG -D__ANDROID__ -DHLSL_CPU -D__PRFCHWINTRIN_H -ffunction-sections -fdata-sections \
 -I"$repo_root/tests/android_headers" -I"$p" -I"$p/contrib/volk" -I"$p/contrib/Vulkan-Headers/include" -I"$p/contrib/VulkanMemoryAllocator/include" \
 -I"$repo_root/lib/rt64/src" -I"$repo_root/lib/rt64/src/contrib/hlslpp/include" -I"$repo_root/lib/rt64/src/contrib" \
 -I"$repo_root/lib/rt64/src/contrib/json/single_include" -I"$repo_root/lib/rt64/src/contrib/dxc/inc" \
 "$p/plume_vulkan.cpp" "$repo_root/tests/android_vulkan_descriptors.cpp" \
 "$repo_root/lib/rt64/src/render/rt64_rsp_processor.cpp" "$repo_root/lib/rt64/src/render/rt64_vertex_processor.cpp" \
 -Wl,--gc-sections -ldl -o "$test_dir/record"
APP_FOLDER_PATH="$test_dir" "$test_dir/record"
while IFS='|' read -r mode reason; do
 rm -f "$test_dir/banjotooi-debug.log"
 if APP_FOLDER_PATH="$test_dir" "$test_dir/record" "$mode" >"$test_dir/failure.log" 2>&1; then
  echo "FAIL: accepted $mode" >&2;exit 1
 else
  test "$?" -eq 1 || { cat "$test_dir/failure.log";exit 1; }
 fi
 grep -Eq "Descriptor (layout )?rejected: reason=$reason" "$test_dir/failure.log" || { cat "$test_dir/failure.log";exit 1; }
 if [ "$mode" = "pool-failed" ]; then
  grep -q 'Descriptor rejected: reason=descriptor-pool-creation-failed result=-1' "$test_dir/failure.log"
 elif [ "$mode" = "pool-capacity-control" ]; then
  grep -Eq 'Descriptor allocation binding: pid=[0-9]+ tid=[0-9]+ .*type=2 declared=8192 requested=1' "$test_dir/banjotooi-debug.log"
  grep -q 'Descriptor pool capacity: .*type=2 capacity=1 flags=0x2 maxSets=1' "$test_dir/banjotooi-debug.log"
  grep -q 'Descriptor allocation begin: .*variable=1 variableCount=1 countStructType=1000161003 countStructSets=1' "$test_dir/banjotooi-debug.log"
  grep -q 'Descriptor rejected: reason=descriptor-set-allocation-failed result=-1000069000' "$test_dir/failure.log"
 elif [ "$mode" = "declared-accounting-old-plan" ]; then
  grep -q 'policy=actual-count .*reservedUAB=1' "$test_dir/banjotooi-debug.log"
  grep -q 'Descriptor rejected: reason=descriptor-set-allocation-failed result=-1000069000' "$test_dir/failure.log"
 elif [ "$mode" = "reserved-variable-outside" ] || [ "$mode" = "650-variable-outside" ]; then
  grep -q 'policy=declared-bound .*reservedUAB=8192' "$test_dir/banjotooi-debug.log"
  grep -q 'declared=8192 requested=1 .*reservedType=8192' "$test_dir/banjotooi-debug.log"
  grep -q 'Descriptor pool capacity: .*type=2 capacity=8192' "$test_dir/banjotooi-debug.log"
  grep -q 'Descriptor allocation begin: .*variable=1 variableCount=1' "$test_dir/banjotooi-debug.log"
  if [ "$mode" = "650-variable-outside" ]; then
   grep -q 'qualification=adreno-650-proprietary-declared-bound' "$test_dir/banjotooi-debug.log"
  fi
 elif [ "$mode" = "650-overlap-limit" ]; then
  grep -q 'qualification=adreno-650-proprietary-declared-bound .*reservedUAB=8192 liveUAB=8192 limitUAB=16383 error=descriptor-pool-update-after-bind-limit' "$test_dir/banjotooi-debug.log"
 elif [ "$mode" = "reserved-allocation-failed" ]; then
  grep -q 'Descriptor rejected: reason=descriptor-set-allocation-failed result=-1000069000 .* set=0 ' "$test_dir/failure.log"
 elif [ "$mode" = "allocation-failed" ]; then
  grep -q 'Descriptor rejected: reason=descriptor-set-allocation-failed .* set=0 ' "$test_dir/failure.log"
 fi
done <<'CASES'
650-variable-outside|descriptor-array-element-not-allocated
650-overlap-limit|descriptor-pool-update-after-bind-limit
invalid-range|invalid-descriptor-range-type-or-count
layout-failed|descriptor-layout-creation-failed
pool-failed|descriptor-pool-creation-failed
allocation-failed|descriptor-set-allocation-failed
allocation-null|descriptor-set-allocation-null-handle
variable-too-large|invalid-descriptor-range-or-variable-count
layout-empty-variable|invalid-layout-ranges
layout-null-ranges|invalid-layout-ranges
layout-invalid-type|invalid-layout-type-or-count
layout-duplicate-binding|duplicate-layout-binding
layout-unsupported|descriptor-layout-unsupported
indexing-disabled|descriptor-indexing-features-not-enabled
uab-disabled|update-after-bind-feature-not-enabled
stage-limit|sampled-image-update-after-bind-limit
variable-limit|variable-descriptor-layout-limit
variable-not-highest|variable-binding-not-highest
pool-capacity-control|descriptor-set-allocation-failed
declared-accounting-old-plan|descriptor-set-allocation-failed
reserved-pool-limit|descriptor-pool-update-after-bind-limit
reserved-overlap-limit|descriptor-pool-update-after-bind-limit
reserved-allocation-failed|descriptor-set-allocation-failed
reserved-variable-outside|descriptor-array-element-not-allocated
reserved-retirement-underflow|descriptor-pool-retirement-accounting
unsupported-view-format|null-buffer-view
texel-element-limit|null-buffer-view
missing-texel-view|buffer-payload-type-mismatch
null-buffer|null-buffer-object
stale-buffer|buffer-not-live-or-wrong-device
stale-buffer-view|buffer-view-not-live-or-wrong-device
failed-buffer-view|null-buffer-view
wrong-buffer-parent|buffer-view-parent-mismatch
index-overflow|descriptor-index-out-of-bounds
null-set|descriptor-set-not-initialized-or-live
set-handle-corrupted|descriptor-set-handle-mismatch
texel-as-storage|texel-payload-type-mismatch
buffer-usage|buffer-usage-mismatch
range-overflow|buffer-range-out-of-bounds
offset-overflow|buffer-offset-out-of-bounds
misaligned|buffer-offset-misaligned
range-limit|buffer-range-exceeds-device-limit
failed-image-view|null-image-view
null-image-view|null-image-view
stale-image|image-not-live-or-wrong-device
poison-image|image-not-live-or-wrong-device
stale-image-view|image-view-not-live-or-wrong-device
wrong-image-parent|image-view-parent-mismatch
image-parent-reused|image-view-not-live-or-wrong-device
image-as-buffer|image-payload-type-mismatch
image-usage|image-usage-mismatch
storage-layout|storage-image-layout-not-general
variable-outside|descriptor-array-element-not-allocated
immutable-sampler-destroyed|immutable-sampler-not-live
stale-sampler|sampler-not-live-or-wrong-device
sampler-as-image|image-payload-type-mismatch
wrong-device|sampler-not-live-or-wrong-device
CASES
APP_FOLDER_PATH="$test_dir" "$test_dir/record" probe-exhaustion-control >"$test_dir/probe.log" 2>&1
test "$(grep -c 'phase=allocate-return .*result=-1000069000' "$test_dir/probe.log")" -eq 5
test "$(grep -c 'phase=retired' "$test_dir/probe.log")" -eq 5
grep -q 'PASS: probe preserves exhausted-pool failure' "$test_dir/probe.log"
printf '%s\n' 'PASS: 57 invalid descriptor cases stop before unsafe Vulkan calls, with exact reasons'
