#include "../src/android_vulkan_driver_policy.h"
#include <cassert>
#include <fstream>
#include <iostream>
int main(int argc,char** argv) {
    assert(argc==2);
    namespace fs=std::filesystem;
    const fs::path root=argv[1],hooks=root/"fixture-hooks";fs::create_directory(hooks);
    for(const auto* name:{"libmain_hook.so","libhook_impl.so","libfile_redirect_hook.so","libgsl_alloc_hook.so"}) std::ofstream(hooks/name)<<"hook";
    int opens=0;
    tooie::android::gpu::Platform platform{[&]{return hooks.string();},
        [&](const auto&,const auto& native,const auto& directory,const auto& library)->void* {
            ++opens;assert(native==hooks.string()&&directory.back()=='/'&&fs::is_regular_file(fs::path(directory)/library));return reinterpret_cast<void*>(1);
        },[](void*,const char*)->void* {return reinterpret_cast<void*>(2);}};
    auto selected=tooie::android::gpu::selectDriver(root,31,platform);
    assert(selected.mode==tooie::android::gpu::Mode::Custom&&opens==1&&selected.librarySha256.size()==64);
    fs::rename(root/"selection.txt",root/"native-reset-history.txt");
    auto system=tooie::android::gpu::selectDriver(root,31,platform);
    assert(system.mode==tooie::android::gpu::Mode::System&&opens==1);
    std::cout<<"PASS: native loader consumes the Java importer's exact protected files and selection; reset selects System Default\n";
}
