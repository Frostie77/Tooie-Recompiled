#pragma once

#include <functional>
#include <filesystem>
#include <vector>
#include <optional>
#include <list>
#include <string>

namespace recompui {
    namespace file {
        std::filesystem::path get_app_folder_path();
        std::filesystem::path get_program_path();
        std::filesystem::path get_asset_path(const char* asset);
        void open_file_dialog(std::function<void(bool success, const std::filesystem::path& path)> callback);
        void open_file_dialog_multiple(std::function<void(bool success, const std::list<std::filesystem::path>& paths)> callback);
        void show_error_message_box(const char *title, const char *message);
#if defined(__ANDROID__)
        void start_file_dialog_callbacks();
        bool poll_file_dialog_callbacks();
        void cancel_file_dialog_callbacks();
#endif
    
        // Apple specific methods that usually require Objective-C. Implemented in support_apple.mm.
        #ifdef __APPLE__
        namespace apple {
            void dispatch_on_ui_thread(std::function<void()> func);
            std::optional<std::filesystem::path> get_application_support_directory();
            std::filesystem::path get_bundle_resource_directory();
            std::filesystem::path get_bundle_directory();
        }
        #endif
    }
}
