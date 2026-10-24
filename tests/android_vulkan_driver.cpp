#include "../src/android_vulkan_driver_policy.h"
#include "../lib/rt64/src/contrib/plume/plume_vulkan_enumeration.h"
#include <cassert>
#include <cstring>
#include <iostream>

namespace fs = std::filesystem;
using namespace tooie::android::gpu;
int main(int argc, char** argv) {
    assert(argc == 2);
    const fs::path root = argv[1], hooks = root / "actual-installed-native-dir";
    fs::create_directories(hooks);
    const std::string uuid(32, 'a'), hash(64, 'b');
    const auto directory = root / uuid, library = directory / "vulkan.adreno.so";
    fs::create_directories(directory);
    std::string elf(64, '\0'); elf.replace(0, 6, "\x7f" "ELF\x02\x01", 6); elf[16] = 3; elf[18] = char(183);
    auto reset = [&] {
        if (fs::is_symlink(library)) fs::remove(library);
        if (fs::exists(library)) fs::permissions(library, fs::perms::owner_all);
        std::ofstream(library, std::ios::binary) << elf;
        fs::permissions(library, fs::perms::owner_read);
        for (const auto* name : {"libmain_hook.so", "libhook_impl.so", "libfile_redirect_hook.so", "libgsl_alloc_hook.so"})
            std::ofstream(hooks / name) << "hook";
        auto text = "schema=1\ndirectory=" + uuid + "\nlibrary=vulkan.adreno.so\nmin_api=28\npackage_sha256=" + hash +
            "\nlibrary_sha256=" + picosha2::hash256_hex_string(elf) + "\n";
        if (fs::is_symlink(root / "selection.txt")) fs::remove(root / "selection.txt");
        std::ofstream(root / "selection.txt") << text;
        return text;
    };
    int opened = 0, symbols = 0, directoryCalls = 0;
    Platform platform{[&] { ++directoryCalls; return hooks.string(); },
        [&](const std::string& cache, const std::string& actualHooks, const std::string& selected, const std::string& name) -> void* {
            ++opened; assert(actualHooks == hooks); assert(selected == directory.string() + "/");
            assert(name == "vulkan.adreno.so" && fs::is_directory(cache)); return reinterpret_cast<void*>(1);
        }, [&](void* handle, const char* name) -> void* {
            ++symbols; assert(handle == reinterpret_cast<void*>(1) && std::string(name) == "vkGetInstanceProcAddr");
            return reinterpret_cast<void*>(2);
        }};
    assert(selectDriver(root, 31, platform).mode == Mode::System && !opened && !directoryCalls);
    auto config = reset();
    auto selection = selectDriver(root, 31, platform);
    assert(selection.mode == Mode::Custom && opened == 1 && symbols == 1);
    assert(selection.librarySha256 == picosha2::hash256_hex_string(elf) && selection.packageSha256 == hash);
    assert(picosha2::hash256_hex_string(std::string("abc")) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    auto rejected = [&](const std::string& expected) {
        auto old = opened; auto value = selectDriver(root, 31, platform);
        assert(value.mode == Mode::Invalid && value.error == expected && opened == old);
    };
    auto changed = [&](const std::string& from, const std::string& to) {
        auto text = reset(); auto at = text.find(from); assert(at != std::string::npos); text.replace(at, from.size(), to);
        std::ofstream(root / "selection.txt") << text;
    };
    changed("schema=1", "schema=2"); rejected("selection-fields");
    changed("directory=" + uuid, "directory=../elsewhere"); rejected("selection-fields");
    changed("library=vulkan.adreno.so", "library=../driver.so"); rejected("selection-fields");
    changed("min_api=28", "min_api=32"); rejected("driver-min-api");
    changed("min_api=28", "min_api=028"); rejected("selection-min-api");
    changed("min_api=28", "min_api=9"); rejected("selection-min-api");
    changed("schema=1", "schema=1\nschema=1"); rejected("selection-malformed-or-duplicate");
    changed("schema=1", "schema=1\nunknown=x"); rejected("selection-fields");
    changed("library_sha256=" + picosha2::hash256_hex_string(elf), "library_sha256=" + hash); rejected("driver-sha256-mismatch");
    reset(); assert(selectDriver(root, 27, platform).error == "custom-driver-requires-android-9");
    reset(); fs::permissions(library, fs::perms::owner_all); rejected("driver-file-is-writable");
    reset(); fs::remove(library); fs::create_symlink(hooks / "libmain_hook.so", library); rejected("driver-file");
    reset(); fs::remove(root / "selection.txt"); fs::create_symlink(root / "absent", root / "selection.txt"); rejected("selection-not-a-regular-file");
    reset(); fs::remove(hooks / "libhook_impl.so"); rejected("packaged-loader-hook-missing");
    reset(); auto noPath = platform; noPath.nativeLibraryDirectory = [] { return std::string(); };
    assert(selectDriver(root, 31, noPath).error == "native-library-directory-unavailable");
    auto noLoad = platform; noLoad.open = [](const auto&, const auto&, const auto&, const auto&) -> void* { return nullptr; };
    assert(selectDriver(root, 31, noLoad).error == "custom-loader-open-failed");
    auto noEntry = platform; noEntry.symbol = [](void*, const char*) -> void* { return nullptr; };
    assert(selectDriver(root, 31, noEntry).error == "custom-loader-entry-point-missing");
    int stock = 0, custom = 0, entries = 0;
    auto dispatch = [&](const Selection& value, bool valid = true) {
        return initialize(value, [&] { ++stock; return true; }, [&](void* address) {
            assert(address == reinterpret_cast<void*>(2)); ++custom;
        }, [&] { ++entries; return valid; });
    };
    assert(dispatch({}) && stock == 1 && custom == 0);
    assert(dispatch(selection) && stock == 1 && custom == 1 && entries == 1);
    assert(!dispatch(selection, false) && stock == 1 && custom == 2);
    assert(!dispatch(invalid("no")) && stock == 1 && custom == 2);
    assert(!acceptBackend(Mode::Custom, false, 18, 18));
    assert(!acceptBackend(Mode::Custom, true, 8, 18));
    assert(acceptBackend(Mode::Custom, true, 18, 18));
    assert(acceptBackend(Mode::System, false, 8, 18));
    assert(!acceptBackend(Mode::Invalid, true, 18, 18));

    std::vector<VkExtensionProperties> extensions;
    VkResult result; unsigned attempts; int calls = 0;
    auto grow = [&](uint32_t* count, VkExtensionProperties* data) {
        ++calls;
        if (!data) { *count = calls < 3 ? 1 : 2; return VK_SUCCESS; }
        std::strcpy(data[0].extensionName, "VK_KHR_swapchain");
        if (*count == 1) return VK_INCOMPLETE;
        std::strcpy(data[1].extensionName, "VK_EXT_descriptor_indexing"); *count = 2; return VK_SUCCESS;
    };
    assert(plume::enumerateVulkanProperties(grow, extensions, result, attempts));
    assert(attempts == 2 && extensions.size() == 2 && std::string(extensions[1].extensionName) == "VK_EXT_descriptor_indexing");
    assert(!plume::enumerateVulkanProperties([](uint32_t*, VkExtensionProperties*) { return VK_ERROR_DEVICE_LOST; }, extensions, result, attempts));
    assert(extensions.empty() && result == VK_ERROR_DEVICE_LOST);
    assert(!plume::enumerateVulkanProperties([](uint32_t* count, VkExtensionProperties*) { *count = 1; return VK_INCOMPLETE; }, extensions, result, attempts));
    assert(attempts == 8 && extensions.empty() && result == VK_INCOMPLETE);
    assert(!plume::enumerateVulkanProperties([](uint32_t* count, VkExtensionProperties*) { *count = 5000; return VK_SUCCESS; }, extensions, result, attempts));
    assert(result == VK_ERROR_OUT_OF_HOST_MEMORY);
    assert(plume::enumerateVulkanProperties([](uint32_t* count, VkExtensionProperties*) { *count = 0; return VK_SUCCESS; }, extensions, result, attempts));
    assert(extensions.empty());
    fs::remove_all(root);
    std::cout << "PASS: private driver policy, digest, loader dispatch, actual-backend qualification and bounded Vulkan enumeration\n";
}
