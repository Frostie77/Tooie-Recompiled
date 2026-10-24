#!/usr/bin/env python3
"""Exercise the real private-driver policy/JNI caller and Vulkan discovery consumer."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
PLUME = ROOT / "lib/rt64/src/contrib/plume"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--jni-include", type=Path)
    options = parser.parse_args()
    compiler = os.environ.get("CXX", "g++")
    java = Path(os.environ.get("JAVA_HOME") or Path(shutil.which("java")).resolve().parents[1])
    jni_include = options.jni_include or java / "include"
    if not (jni_include / "jni.h").is_file():
        raise RuntimeError("JDK development headers are required; use --jni-include when they are outside JAVA_HOME/include")
    environment = dict(os.environ, ASAN_OPTIONS="detect_leaks=0", UBSAN_OPTIONS="halt_on_error=1")
    vendor = json.loads((ROOT / "tools/ci/android-driver-vendor.json").read_text())
    for entry in vendor["files"]:
        data = (ROOT / entry["path"]).read_bytes()
        if hashlib.sha1(b"blob " + str(len(data)).encode() + b"\0" + data).hexdigest() != entry["git_blob_sha1"]:
            raise RuntimeError("Vendored source differs from pinned upstream: " + entry["path"])
    with tempfile.TemporaryDirectory(prefix="tooie-gpu-driver-") as temp:
        work = Path(temp)
        flags = [compiler, "-std=c++20", "-O1", "-g", "-fsanitize=address,undefined",
                 "-fno-omit-frame-pointer", "-no-pie", "-I" + str(PLUME / "contrib/Vulkan-Headers/include")]
        def build(sources, output, extra=()):
            subprocess.run(flags + list(extra) + [str(p) for p in sources] + ["-o", str(output)], check=True)
        binary = work / "policy"
        build([ROOT / "tests/android_vulkan_driver.cpp"], binary)
        subprocess.run([str(binary), str(work / "policy-files")], env=environment, check=True)
        jni = work / "jni"
        build([ROOT / "tests/android_vulkan_driver_jni.cpp", ROOT / "src/android_vulkan_driver.cpp"], jni,
              ["-I" + str(ROOT / "tests/android_gpu_headers"), "-I" + str(ROOT / "lib/adrenotools/include"),
               "-I" + str(jni_include), "-I" + str(jni_include / "linux"), "-Wl,--wrap=dlsym"])
        for fault in ("system", "success", "no-env", "no-activity", "no-files", "activity-class", "method",
                      "info", "info-class", "field", "value", "utf", "open", "symbol"):
            files = work / fault; driver = files / "vulkan-driver"; hooks = work / (fault + "-installed-native-dir")
            stage = driver / ("a" * 32)
            stage.mkdir(parents=True); hooks.mkdir()
            elf = bytearray(64); elf[:6] = b"\x7fELF\x02\x01"; elf[16] = 3; elf[18] = 183
            library = stage / "vulkan.adreno.so"; library.write_bytes(elf); library.chmod(0o400)
            for name in ("libmain_hook.so", "libhook_impl.so", "libfile_redirect_hook.so", "libgsl_alloc_hook.so"):
                (hooks / name).write_bytes(b"hook")
            if fault != "system":
                (driver / "selection.txt").write_text("schema=1\ndirectory=" + "a" * 32 +
                    "\nlibrary=vulkan.adreno.so\nmin_api=28\npackage_sha256=" + "b" * 64 +
                    "\nlibrary_sha256=" + hashlib.sha256(elf).hexdigest() + "\n")
            subprocess.run([str(jni), fault, str(files), str(hooks)], env=environment, check=True)

        source = (PLUME / "plume_vulkan.cpp").read_text()
        begin = source.index("        // Check for extensions.", source.index("VulkanDevice::VulkanDevice("))
        end = source.index("        std::unordered_set<std::string> missingRequiredExtensions", begin)
        current = source[begin:end]
        parent = (ROOT / "tests/controls/vulkan_device_extensions_6dc376a95fee.inc").read_text()
        harness = r'''
#include <algorithm>
#include <cstdio>
#include <cstring>
#include "plume_vulkan_enumeration.h"
using namespace plume;
static VkPhysicalDevice physicalDevice = {};
static int calls = 0;
static bool found = false;
static void logVulkanEnumeration(const char*, bool, VkResult, unsigned, size_t) {}
VkResult vkEnumerateDeviceExtensionProperties(VkPhysicalDevice, const char*, uint32_t* count, VkExtensionProperties* data) {
    ++calls;
    if (!data) { *count = calls < 3 ? 1 : 2; return VK_SUCCESS; }
    std::strcpy(data[0].extensionName, "VK_KHR_swapchain");
    if (*count == 1) return VK_INCOMPLETE;
    std::strcpy(data[1].extensionName, "VK_EXT_descriptor_indexing"); *count = 2; return VK_SUCCESS;
}
void probe() {
BODY
    found = std::any_of(availableExtensions.begin(), availableExtensions.end(), [](const auto& x) {
        return std::strcmp(x.extensionName, "VK_EXT_descriptor_indexing") == 0;
    });
}
int main() { probe(); return found ? 0 : 7; }
'''
        for label, body, expected in (("parent", parent, 7), ("current", current, 0)):
            cpp = work / (label + ".cpp"); cpp.write_text(harness.replace("BODY", body))
            executable = work / label
            build([cpp], executable, ["-I" + str(PLUME)])
            result = subprocess.run([str(executable)], env=environment)
            if result.returncode != expected:
                raise RuntimeError(label + " consumer control did not produce its expected runtime result")
        print("PASS: actual parent consumer misses the late extension; current consumer retries and discovers it")


if __name__ == "__main__":
    main()
