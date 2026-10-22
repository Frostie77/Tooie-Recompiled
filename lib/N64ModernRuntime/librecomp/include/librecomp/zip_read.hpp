#pragma once

#include <array>
#include <cstdint>
#include <vector>
#include <miniz.h>
#include <zstd.h>

namespace recomp::mods::zip {
inline constexpr uint16_t Zstandard = 93;
inline constexpr uint64_t MaxEntryBytes = 256ull * 1024 * 1024;

inline bool supported(const mz_zip_archive_file_stat& stat) {
    // Match miniz's encryption/patch restrictions when extending its methods.
    return stat.m_is_supported || (stat.m_method == Zstandard &&
        !stat.m_is_encrypted && !(stat.m_bit_flag & (1u | 32u | 64u)));
}

// Used by the real mod handle, including memory-backed embedded archives.
// miniz still owns directory parsing, ZIP64 offsets and raw-data bounds.
inline bool read(mz_zip_archive* archive, mz_uint index, std::vector<char>& output) {
    output.clear();
    mz_zip_archive_file_stat stat{};
    if (!mz_zip_reader_file_stat(archive, index, &stat) || !supported(stat) ||
        stat.m_uncomp_size > MaxEntryBytes || stat.m_comp_size > MaxEntryBytes ||
        stat.m_comp_size > archive->m_archive_size)
        return false;
    if (stat.m_method != Zstandard) {
        output.resize(static_cast<size_t>(stat.m_uncomp_size));
        if (mz_zip_reader_extract_to_mem(archive, index, output.data(), output.size(), 0)) return true;
        output.clear();
        return false;
    }

    // Reject disagreements before asking miniz for opaque compressed bytes.
    if (stat.m_comp_size == 0) return false;
    std::array<uint8_t, 30> header{};
    if (!archive->m_pRead || archive->m_pRead(archive->m_pIO_opaque,
            stat.m_local_header_ofs, header.data(), header.size()) != header.size() ||
        header[0] != 'P' || header[1] != 'K' || header[2] != 3 || header[3] != 4 ||
        (header[8] | (uint16_t(header[9]) << 8)) != Zstandard ||
        (header[6] | (uint16_t(header[7]) << 8)) != stat.m_bit_flag)
        return false;
    std::vector<char> compressed(static_cast<size_t>(stat.m_comp_size));
    if (!mz_zip_reader_extract_to_mem(archive, index, compressed.data(), compressed.size(), MZ_ZIP_FLAG_COMPRESSED_DATA))
        return false;
    // Use the same whole-buffer decoder as RT64, including valid concatenated
    // frames. Its destination is bounded by the declared and capped ZIP size.
    output.resize(static_cast<size_t>(stat.m_uncomp_size));
    const size_t decoded = ZSTD_decompress(output.data(), output.size(), compressed.data(), compressed.size());
    if (ZSTD_isError(decoded) || decoded != output.size() ||
        mz_crc32(MZ_CRC32_INIT, reinterpret_cast<const mz_uint8*>(output.data()), output.size()) != stat.m_crc32) {
        output.clear();
        return false;
    }
    return true;
}
} // namespace recomp::mods::zip
