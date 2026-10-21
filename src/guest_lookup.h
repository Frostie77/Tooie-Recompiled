#pragma once
// Android failure provenance only. No guest addresses are inferred or repaired.
#if defined(__ANDROID__)
#include "recomp.h"
#include "runtime_diagnostics.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <limits>

namespace tooie::lookup {
struct Context {
    const uint8_t *rdram = nullptr;
    const recomp_context *registers = nullptr;
    const char *caller = nullptr;
    uint32_t line = 0;
    uint32_t thunk = 0, overlay = 0, entryOffset = 0;
    bool autoUnload = false;
};
struct Event {
    const char *operation = "none";
    uint64_t sequence = 0;
    uint32_t rom = 0, ram = 0, size = 0;
    size_t section = 0;
};
// Same guest scheduling/ownership as the function map. Fixed storage, no I/O
// on normal load/unload; successful lookups do not touch this journal at all.
struct Journal {
    std::array<Event, 32> events{};
    size_t next = 0, count = 0;
    uint64_t sequence = 0;
    void add(const char *op, uint32_t rom, uint32_t ram, uint32_t size, size_t section) {
        events[next] = {op, ++sequence, rom, ram, size, section};
        next = (next + 1) % events.size(); count = std::min(count + 1, events.size());
    }
};
inline Journal &journal() { static Journal value; return value; }
inline bool word(const uint8_t *ram, uint32_t address, uint32_t &value) {
    if (!ram || (address & 3) || address < 0x80000000U || address > 0x807FFFFCU) return false;
    std::memcpy(&value, ram + (address - 0x80000000U), 4); return true;
}
inline void words(const uint8_t *ram, const char *kind, uint32_t start) {
    char text[320]; size_t n = std::snprintf(text, sizeof(text), "kind=%s start=0x%08X words=", kind, start);
    unsigned count = 0;
    for (; count < 8 && uint64_t(start) + count * 4 <= UINT32_MAX; ++count) {
        uint32_t value; if (!word(ram, start + count * 4, value)) break;
        n += std::snprintf(text + n, sizeof(text) - n, "%s%08X", count ? "/" : "", value);
    }
    std::snprintf(text + n, sizeof(text) - n, " readable_words=%u", count);
    diagnostics::log("lookup-words", text);
}
// This snapshot is taken only after the actual map lookup has failed. Counts
// and truncation are explicit; nearest sections are observations, not owners.
template<class Sections, class Loaded, class Map, class Overlays>
void report(uint32_t target, const Context *context, const Sections &sections,
            const Loaded &loaded, const Map &functions, const Overlays &overlays) {
    const Context empty; const auto &c = context ? *context : empty;
    char text[1100];
    uint32_t lower = 0, upper = UINT32_MAX; bool hasLower = false, hasUpper = false;
    for (const auto &entry : functions) {
        const uint32_t address = uint32_t(entry.first);
        if (address < target && (!hasLower || address > lower)) { lower = address; hasLower = true; }
        if (address > target && (!hasUpper || address < upper)) { upper = address; hasUpper = true; }
    }
    std::snprintf(text, sizeof(text),
        "target=0x%08X caller=%.160s source_line=%u thunk=0x%08X overlay=0x%08X entry_offset=0x%X auto_unload=%u registers=%u guest_ra_is_raw=1 mapped_functions=%zu loaded_sections=%zu section_table_count=%zu",
        target, c.caller ? c.caller : "none", c.line, c.thunk, c.overlay, c.entryOffset, unsigned(c.autoUnload),
        unsigned(c.registers != nullptr), functions.size(), loaded.size(), sections.num_code_sections);
    diagnostics::log("lookup-failure", text);
    std::snprintf(text, sizeof(text), "registered_below_valid=%u registered_below=0x%08X registered_above_valid=%u registered_above=0x%08X", unsigned(hasLower), lower, unsigned(hasUpper), upper);
    diagnostics::log("lookup-neighbors", text);
    if (c.registers) {
        const auto &r = *c.registers;
        std::snprintf(text, sizeof(text),
            "v0=0x%08X v1=0x%08X a0=0x%08X a1=0x%08X a2=0x%08X a3=0x%08X t0=0x%08X t1=0x%08X t2=0x%08X t3=0x%08X t4=0x%08X t5=0x%08X t6=0x%08X t7=0x%08X t8=0x%08X t9=0x%08X sp=0x%08X raw_ra=0x%08X",
            uint32_t(r.r2), uint32_t(r.r3), uint32_t(r.r4), uint32_t(r.r5), uint32_t(r.r6), uint32_t(r.r7),
            uint32_t(r.r8), uint32_t(r.r9), uint32_t(r.r10), uint32_t(r.r11), uint32_t(r.r12), uint32_t(r.r13),
            uint32_t(r.r14), uint32_t(r.r15), uint32_t(r.r24), uint32_t(r.r25), uint32_t(r.r29), uint32_t(r.r31));
        diagnostics::log("lookup-registers", text);
        std::snprintf(text, sizeof(text),
            "s0=0x%08X s1=0x%08X s2=0x%08X s3=0x%08X s4=0x%08X s5=0x%08X s6=0x%08X s7=0x%08X",
            uint32_t(r.r16), uint32_t(r.r17), uint32_t(r.r18), uint32_t(r.r19), uint32_t(r.r20), uint32_t(r.r21), uint32_t(r.r22), uint32_t(r.r23));
        diagnostics::log("lookup-registers", text);
        words(c.rdram, "stack-raw", uint32_t(r.r29));
        words(c.rdram, "a0-raw", uint32_t(r.r4));
    }
    words(c.rdram, "target-raw", target);
    if (c.thunk) words(c.rdram, "thunk-raw", c.thunk);
    // Prioritize containing sections, then nearest section ranges. No guessed
    // symbol or automatically registered function is produced by this search.
    std::array<size_t, 64> order{}; size_t used = 0, containing = 0;
    auto distance = [&](size_t i) -> uint64_t {
        const auto &s = loaded[i];
        if (s.section_table_index >= sections.num_code_sections) return UINT64_MAX;
        const uint64_t begin = uint32_t(s.loaded_ram_addr), end = begin + sections.code_sections[s.section_table_index].size;
        if (target < begin) return begin - target;
        return target < end ? 0 : uint64_t(target) - end + 1;
    };
    for (size_t i = 0; i < loaded.size(); ++i) {
        containing += distance(i) == 0;
        size_t at = 0; while (at < used && distance(order[at]) <= distance(i)) ++at;
        if (at == order.size()) continue;
        const size_t last = std::min(used, order.size() - 1);
        for (size_t j = last; j > at; --j) order[j] = order[j - 1];
        order[at] = i; used = std::min(used + 1, order.size());
    }
    std::snprintf(text, sizeof(text), "containing_sections=%zu loaded_reported=%zu loaded_omitted=%zu order=containing-then-nearest", containing, used, loaded.size() - used);
    diagnostics::log("lookup-map", text);
    for (size_t i = 0; i < used; ++i) {
        const auto &l = loaded[order[i]];
        if (l.section_table_index >= sections.num_code_sections) {
            std::snprintf(text, sizeof(text), "table_index=%zu invalid_section_index=1", l.section_table_index);
        }
        else {
            const auto &s = sections.code_sections[l.section_table_index];
            const uint64_t begin = uint32_t(l.loaded_ram_addr), end = begin + s.size;
            const bool inside = target >= begin && target < end;
            const uint32_t offset = inside ? uint32_t(target - begin) : UINT32_MAX;
            uint32_t below = UINT32_MAX, above = UINT32_MAX, span = 0; bool exact = false, entryPresent = false;
            for (size_t f = 0; f < s.num_funcs; ++f) {
                const auto &entry = s.funcs[f];
                if (inside && entry.offset == offset) { exact = true; entryPresent = functions.find(int32_t(target)) != functions.end(); }
                if (inside && entry.offset <= offset && (below == UINT32_MAX || entry.offset > below)) { below = entry.offset; span = entry.rom_size; }
                if (inside && entry.offset > offset && entry.offset < above) above = entry.offset;
            }
            std::snprintf(text, sizeof(text),
                "table_index=%zu section_index=%zu rom=0x%08X static_ram=0x%08X loaded_ram=0x%08X size=0x%X functions=%zu contains=%u target_offset=0x%X declared_exact=%u map_present=%u previous_offset=0x%X previous_rom_size=0x%X next_offset=0x%X",
                l.section_table_index, s.index, s.rom_addr, s.ram_addr, uint32_t(begin), s.size, s.num_funcs,
                unsigned(inside), offset, unsigned(exact), unsigned(entryPresent), below, span, above);
        }
        diagnostics::log("lookup-section", text);
    }
    // Independent retail table versus native map. Unknown/corrupt counts are
    // capped rather than trusted as host memory extents. Headers are raw data.
    uint32_t count = 0;
    const bool hasCount = word(c.rdram, 0x80117C60, count);
    const uint32_t scan = hasCount ? std::min(count, 64U) : 0;
    std::snprintf(text, sizeof(text), "count_readable=%u count=%u entries_scanned=%u omitted=%u limit=64", unsigned(hasCount), count, scan, count - scan);
    diagnostics::log("lookup-guest-table", text);
    for (uint32_t i = 0; i < scan; ++i) {
        uint32_t header = 0, a = 0, b = 0, d = 0, indexWord = 0;
        const bool valid = word(c.rdram, 0x80126738 + i * 4, header) &&
            word(c.rdram, header, a) && uint64_t(header) + 0x30 <= 0x80800000ULL &&
            word(c.rdram, header + 8, b) && word(c.rdram, header + 0xC, d) && word(c.rdram, header + 0x2C, indexWord);
        uint64_t code = 0; uint32_t index = indexWord >> 16; size_t native = SIZE_MAX;
        if (valid) {
            code = uint64_t(header) + 0x38 + (b >> 16) * 4 + ((d >> 8) & 255);
            code = ((code + 3) & ~3ULL) + (b & 65535) * 2;
            code = ((code + 3) & ~3ULL) + (d >> 16) * 4;
            code = (code + 15) & ~15ULL;
            if (overlays.table && index && index <= overlays.len && overlays.table[index - 1] >= 0) native = size_t(overlays.table[index - 1]);
        }
        std::snprintf(text, sizeof(text), "slot=%u header=0x%08X readable=%u index=%u entry_count=%u computed_code=0x%llX code_in_rdram=%u native_table_index=%zu header_word0=0x%08X", i, header, unsigned(valid), index, b >> 16, (unsigned long long)code, unsigned(valid && code >= 0x80000000 && code < 0x80800000), native, a);
        diagnostics::log("lookup-guest-overlay", text);
    }
    const auto &j = journal();
    for (size_t i = 0; i < j.count; ++i) {
        const auto &e = j.events[(j.next + j.events.size() - j.count + i) % j.events.size()];
        std::snprintf(text, sizeof(text), "sequence=%llu operation=%s rom=0x%08X ram=0x%08X size=0x%X table_index=%zu", (unsigned long long)e.sequence, e.operation, e.rom, e.ram, e.size, e.section);
        diagnostics::log("lookup-event", text);
    }
}
}
extern "C" recomp_func_t *get_function_with_context(int32_t, const tooie::lookup::Context *);
#endif
