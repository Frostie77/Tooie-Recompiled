#include "librecomp/mods.hpp"
#include "rt64_filesystem_zip.h"
#include <iostream>
#include <stdexcept>

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    recomp::mods::ModOpenError error;
    recomp::mods::ZipModFileHandle archive(std::filesystem::path(argv[1]), error);
    RT64::FileSystemZip textures(std::filesystem::path(argv[1]), "");
    if (error != recomp::mods::ModOpenError::Good || !textures.isOpen()) return 2;
    size_t files = 0, zstd = 0;
    uint64_t bytes = 0;
    for (mz_uint i = 0; i < mz_zip_reader_get_num_files(archive.archive.get()); ++i) {
        mz_zip_archive_file_stat stat{};
        if (!mz_zip_reader_file_stat(archive.archive.get(), i, &stat)) return 2;
        if (stat.m_is_directory) continue;
        std::vector<char> name(mz_zip_reader_get_filename(archive.archive.get(), i, nullptr, 0));
        if (name.empty() || !mz_zip_reader_get_filename(archive.archive.get(), i, name.data(), name.size())) return 2;
        bool readable = false;
        auto original = archive.read_file(name.data(), readable);
        if (!readable || original.size() != stat.m_uncomp_size) {
            std::cerr << "Archive consumer could not read " << name.data() << '\n';
            return 2;
        }
        if (!textures.exists(name.data()) || textures.getSize(name.data()) != original.size()) return 2;
        if (!original.empty()) {
            std::vector<uint8_t> loaded(original.size());
            if (!textures.load(name.data(), loaded.data(), loaded.size()) ||
                !std::equal(loaded.begin(), loaded.end(), original.begin(),
                    [](uint8_t a, char b) { return a == static_cast<uint8_t>(b); })) return 2;
        }
        ++files;
        zstd += stat.m_method == 93;
        bytes += original.size();
    }
    if (!zstd || !textures.exists("rt64.json")) return 2;
    std::cout << "PASS: " << files << " archive files (" << zstd << " Zstandard), " << bytes
              << " decoded bytes: production mod reader and RT64 filesystem agree; no GPU/rendering claim\n";
}
