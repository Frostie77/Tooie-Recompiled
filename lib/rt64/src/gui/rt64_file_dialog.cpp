//
// RT64
//

#include "rt64_file_dialog.h"

#include <cassert>

#ifndef __ANDROID__
#include <nfd.h>
#endif

namespace RT64 {
    // FileDialog

    std::atomic<bool> FileDialog::isOpen = false;

    void FileDialog::initialize() {
#ifndef __ANDROID__
        NFD_Init();
#endif
    }

    void FileDialog::finish() {
#ifndef __ANDROID__
        NFD_Quit();
#endif
    }

#ifndef __ANDROID__
    static std::vector<nfdnfilteritem_t> convertFilters(const std::vector<FileFilter> &filters) {
        std::vector<nfdnfilteritem_t> nfdFilters;
        for (const FileFilter &filter : filters) {
            nfdFilters.emplace_back(nfdnfilteritem_t{ filter.description.c_str(), filter.extensions.c_str() });
        }

        return nfdFilters;
    }
#endif

    std::filesystem::path FileDialog::getDirectoryPath() {
#ifdef __ANDROID__
        return {};
#else
        isOpen = true;

        std::filesystem::path path;
        nfdnchar_t *nfdPath = nullptr;
        nfdresult_t res = NFD_PickFolderN(&nfdPath, nullptr);
        if (res == NFD_OKAY) {
            path = std::filesystem::path(nfdPath);
            NFD_FreePathN(nfdPath);
        }

        isOpen = false;
        return path;
#endif
    }

    std::filesystem::path FileDialog::getOpenFilename(const std::vector<FileFilter> &filters) {
#ifdef __ANDROID__
        (void)filters;
        return {};
#else
        isOpen = true;

        std::filesystem::path path;
        nfdnchar_t *nfdPath = nullptr;
        std::vector<nfdnfilteritem_t> nfdFilters = convertFilters(filters);
        nfdresult_t res = NFD_OpenDialogN(&nfdPath, nfdFilters.data(), nfdFilters.size(), nullptr);
        if (res == NFD_OKAY) {
            path = std::filesystem::path(nfdPath);
            NFD_FreePathN(nfdPath);
        }

        isOpen = false;
        return path;
#endif
    }

    std::filesystem::path FileDialog::getSaveFilename(const std::vector<FileFilter> &filters) {
#ifdef __ANDROID__
        (void)filters;
        return {};
#else
        isOpen = true;

        std::filesystem::path path;
        nfdnchar_t *nfdPath = nullptr;
        std::vector<nfdnfilteritem_t> nfdFilters = convertFilters(filters);
        nfdresult_t res = NFD_SaveDialogN(&nfdPath, nfdFilters.data(), nfdFilters.size(), nullptr, nullptr);
        if (res == NFD_OKAY) {
            path = std::filesystem::path(nfdPath);
            NFD_FreePathN(nfdPath);
        }

        isOpen = false;
        return path;
#endif
    }
};
