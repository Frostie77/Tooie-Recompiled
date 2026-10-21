#!/usr/bin/env bash
set -euo pipefail
r=$(cd "$(dirname "$0")/.." && pwd)
t=$(mktemp -d)
trap 'rm -rf "$t"' EXIT
: "${GLSLC:?Set GLSLC to glslc (NDK shader-tools works)}"
"$GLSLC" --target-env=vulkan1.2 -fshader-stage=comp "$r/tests/android_vulkan_descriptor_validation.comp" -o "$t/descriptor.spv"
if [ -n "${SPIRV_VAL:-}" ]; then "$SPIRV_VAL" --target-env vulkan1.2 "$t/descriptor.spv"; fi
python3 - "$t" <<'PY'
import struct,sys
from pathlib import Path
p=Path(sys.argv[1]);data=(p/'descriptor.spv').read_bytes();v=struct.unpack('<%dI'%(len(data)//4),data)
(p/'descriptor-test.spirv.h').write_text('static const uint32_t DescriptorTest[] = {'+','.join(hex(x) for x in v)+'};\n')
PY
p="$r/lib/rt64/src/contrib/plume"
"${CXX:-clang++}" -std=c++17 -D__ANDROID__ -DNDEBUG -DHLSL_CPU -D__PRFCHWINTRIN_H \
 -I"$t" -I"$r/tests/android_headers" -I"$p" -I"$p/contrib/volk" -I"$p/contrib/Vulkan-Headers/include" -I"$p/contrib/VulkanMemoryAllocator/include" \
 -I"$r/lib/rt64/src" -I"$r/lib/rt64/src/contrib/hlslpp/include" \
 "$p/plume_vulkan.cpp" "$r/tests/android_vulkan_descriptor_validation.cpp" -ldl -o "$t/validation"
MESA_SHADER_CACHE_DIR="$t/cache" "$t/validation"

"$GLSLC" --target-env=vulkan1.2 -fshader-stage=comp "$r/tests/android_vulkan_descriptor_pool_validation.comp" -o "$t/pool.spv"
if [ -n "${SPIRV_VAL:-}" ]; then "$SPIRV_VAL" --target-env vulkan1.2 "$t/pool.spv"; fi
python3 - "$t" <<'PY'
import struct,sys
from pathlib import Path
p=Path(sys.argv[1]);data=(p/'pool.spv').read_bytes();v=struct.unpack('<%dI'%(len(data)//4),data)
(p/'descriptor-pool-test.spirv.h').write_text('static const uint32_t DescriptorPoolTest[] = {'+','.join(hex(x) for x in v)+'};\n')
PY
"${CXX:-clang++}" -std=c++17 -D__ANDROID__ -DNDEBUG -DHLSL_CPU -D__PRFCHWINTRIN_H \
 -I"$t" -I"$r/tests/android_headers" -I"$p" -I"$p/contrib/volk" -I"$p/contrib/Vulkan-Headers/include" -I"$p/contrib/VulkanMemoryAllocator/include" \
 -I"$r/lib/rt64/src" -I"$r/lib/rt64/src/contrib/hlslpp/include" \
 "$p/plume_vulkan.cpp" "$r/tests/android_vulkan_descriptor_pool_validation.cpp" -ldl -o "$t/pool-validation"
APP_FOLDER_PATH="$t" MESA_SHADER_CACHE_DIR="$t/cache" "$t/pool-validation"
APP_FOLDER_PATH="$t" MESA_SHADER_CACHE_DIR="$t/cache" "$t/pool-validation" declared-pool
rm -f "$t/banjotooi-debug.log"
APP_FOLDER_PATH="$t" MESA_SHADER_CACHE_DIR="$t/cache" "$t/pool-validation" qualified
grep -q 'Descriptor pool policy: .*policy=declared-bound qualification=adreno-740-proprietary-declared-bound .*reservedUAB=8192' "$t/banjotooi-debug.log"
grep -q 'Descriptor allocation binding: .*type=2 declared=8192 requested=1 .*reservedType=8192' "$t/banjotooi-debug.log"
grep -q 'Descriptor pool policy: .*reservedUAB=8192 liveUAB=8192' "$t/banjotooi-debug.log"
if grep -q 'intentional diagnostic stop' "$t/banjotooi-debug.log"; then echo 'FAIL: normal path entered probe';exit 1;fi
rm -f "$t/banjotooi-debug.log"
APP_FOLDER_PATH="$t" MESA_SHADER_CACHE_DIR="$t/cache" "$t/pool-validation" core-only
grep -Eq 'Descriptor indexing discovery: pid=[0-9]+ tid=[0-9]+ .*extension=0 queried=1 enabled=1' "$t/banjotooi-debug.log"
grep -q 'Descriptor allocation binding: .*type=2 declared=8192 requested=1 immutable=0 flags=0xD' "$t/banjotooi-debug.log"
grep -q 'Descriptor pool capacity: .*type=2 capacity=1 flags=0x2 maxSets=1' "$t/banjotooi-debug.log"
while read -r type count; do
 grep -q "Descriptor pool capacity: .*type=$type capacity=$count flags=0x0 maxSets=1" "$t/banjotooi-debug.log"
done <<'COMMON'
0 18
6 2
7 19
5 4
3 22
2 1
COMMON
printf '%s\n' 'PASS: PID-tagged core feature discovery, exact variable counts and production common-pool capacities logged before allocation'

"${CXX:-clang++}" -std=c++17 -D__ANDROID__ -DNDEBUG -DTOOIE_VULKAN_DESCRIPTOR_ALLOCATION_PROBE=1 \
 -I"$r/tests/android_headers" -I"$p" -I"$p/contrib/volk" -I"$p/contrib/Vulkan-Headers/include" -I"$p/contrib/VulkanMemoryAllocator/include" \
 "$p/plume_vulkan.cpp" "$r/tests/android_vulkan_descriptor_probe_entry.cpp" -ldl -o "$t/probe-entry"
APP_FOLDER_PATH="$t" MESA_SHADER_CACHE_DIR="$t/cache" "$t/probe-entry" >"$t/probe.log" 2>&1
grep -q 'probe complete: .*allSucceeded=1 exit=0; intentional diagnostic stop' "$t/probe.log"
grep -q 'production-path control retired' "$t/banjotooi-debug.log"
test "$(grep -c 'phase=allocate-return .*result=0' "$t/probe.log")" -eq 5
if APP_FOLDER_PATH="$t" MESA_SHADER_CACHE_DIR="$t/cache" "$t/probe-entry" failure-control >"$t/probe-failure.log" 2>&1; then
 echo 'FAIL: probe concealed injected allocation failure' >&2;exit 1
else
 test "$?" -eq 1
fi
grep -q 'case=production phase=allocate-return .*result=-1000069000' "$t/probe-failure.log"
grep -q 'probe complete: .*allSucceeded=0 exit=1; intentional diagnostic stop' "$t/probe-failure.log"
test "$(grep -c 'phase=retired' "$t/probe-failure.log")" -eq 5
if grep -q 'VALIDATION:' "$t/probe.log" "$t/probe-failure.log"; then cat "$t/probe.log" "$t/probe-failure.log";exit 1;fi
if APP_FOLDER_PATH="$t" MESA_SHADER_CACHE_DIR="$t/cache" "$t/probe-entry" device-lost >"$t/probe-lost.log" 2>&1; then
 echo 'FAIL: probe concealed injected device loss' >&2;exit 1
else
 test "$?" -eq 1
fi
grep -q 'case=fixed-one phase=allocate-return .*result=-4' "$t/probe-lost.log"
test "$(grep -c 'phase=retired' "$t/probe-lost.log")" -eq 1
if grep -q 'VALIDATION:' "$t/probe-lost.log"; then cat "$t/probe-lost.log";exit 1;fi
printf '%s\n' 'PASS: production early probe executes five cases before raster setup; injected failure remains fatal despite later successful controls; zero validation errors'
