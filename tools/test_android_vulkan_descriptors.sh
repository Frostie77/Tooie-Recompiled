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
 if APP_FOLDER_PATH="$test_dir" "$test_dir/record" "$mode" >"$test_dir/failure.log" 2>&1; then
  echo "FAIL: accepted $mode" >&2;exit 1
 else
  test "$?" -eq 1 || { cat "$test_dir/failure.log";exit 1; }
 fi
 grep -q "Descriptor rejected: reason=$reason" "$test_dir/failure.log" || { cat "$test_dir/failure.log";exit 1; }
done <<'CASES'
invalid-range|invalid-descriptor-range-type-or-count
layout-failed|descriptor-layout-creation-failed
pool-failed|descriptor-pool-creation-failed
allocation-failed|descriptor-set-allocation-failed
allocation-null|descriptor-set-allocation-failed
variable-too-large|invalid-descriptor-range-or-variable-count
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
printf '%s\n' 'PASS: 38 invalid descriptor cases stop before vkUpdateDescriptorSets, with exact reasons'
