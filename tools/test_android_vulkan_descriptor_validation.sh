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
