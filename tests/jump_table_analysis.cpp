// The four dispatch instructions are from the captured bsfirstp consumer.
// Table entries in this generator-only control are constructed, not a ROM dump.
#include "rabbitizer.hpp"
#include "recompiler/context.h"
#include "analysis.h"
#include <cstdlib>
#include <cstdio>
#include <sstream>
#include <string>

#define CHECK(v) do { if (!(v)) { std::fprintf(stderr, "jump-table failure line=%d: %s\n", __LINE__, #v); std::exit(3); } } while (0)

static void run(uint32_t tableOffset, bool relocated, bool addiu, bool parent) {
    constexpr uint32_t base = 0x80800000, romBase = 0x20000;
    constexpr uint32_t start = base + 0x59EC, target = base + 0x5AE0;
    N64Recomp::Context context{};
    context.sections.resize(109);
    auto &section = context.sections[108];
    section.ram_addr = base; section.rom_addr = romBase;
    section.size = 0xA3C0; section.relocatable = relocated; section.executable = true;
    context.rom.resize(romBase + 0x20000);
    const uint32_t table = base + tableOffset;
    const uint32_t high = relocated ? ((tableOffset + 0x8000) >> 16) : ((table + 0x8000) >> 16);
    const uint32_t low = tableOffset & 0xFFFF;
    std::vector<uint32_t> words((0x5AF0 - 0x59EC) / 4, 0);
    words[0] = 0x3C010000 | high; // lui $at, %hi(table)
    words[1] = 0x002F0821;        // addu $at, $at, $t7
    if (addiu) {
        words[2] = 0x24210000 | low; // addiu $at,$at,%lo(table)
        words[3] = 0x8C2F0000;       // lw $t7,0($at)
        words[4] = 0x01E00008;       // jr $t7
    } else {
        words[2] = 0x8C2F0000 | low; // lw $t7,%lo(table)($at)
        words[3] = 0x01E00008;       // jr $t7 (captured 808059F8)
    }
    words[(target - start) / 4] = 0x24020073; // constructed observable case
    words[(target - start) / 4 + 1] = 0x03E00008; // jr $ra
    if (relocated) {
        section.relocs = {
            {start, tableOffset, 0, 108, N64Recomp::RelocType::R_MIPS_HI16, false},
            {start + 8, tableOffset, 0, 108, N64Recomp::RelocType::R_MIPS_LO16, false}
        };
    }
    auto put = [&](uint32_t address, uint32_t value) {
        for (int j = 0; j < 4; ++j) context.rom[address+j] = value >> (24-8*j);
    };
    for (uint32_t i = 0; i < 47; ++i) {
        // Repeated arms and the captured last slot; no invented USA slot map.
        put(romBase + tableOffset + i*4, relocated ? target-base : target);
    }
    std::vector<rabbitizer::InstructionCpu> instructions;
    for (size_t i = 0; i < words.size(); ++i) {
        instructions.emplace_back(words[i], start + i*4);
        put(romBase + start-base + i*4, words[i]);
        words[i] = byteswap(words[i]);
    }
    context.functions.emplace_back(start, romBase+start-base, words, "dispatch_control", 108);
    context.functions_by_vram[start] = {0};
    N64Recomp::FunctionStats stats{};
    CHECK(N64Recomp::analyze_function(context, context.functions[0], instructions, stats));
    const bool lost = parent && relocated && low >= 0x8000;
    CHECK(stats.jump_tables.size() == (lost ? 0U : 1U));
    std::ostringstream generated;
    std::vector<std::vector<uint32_t>> statics(context.sections.size());
    CHECK(N64Recomp::recompile_function(context, 0, generated, statics, false));
    const std::string output = generated.str();
    CHECK((output.find("LOOKUP_FUNC(ctx->r15)") != std::string::npos) == lost);
    CHECK((output.find("case 46: goto L_80805AE0") != std::string::npos) == !lost);
    if (!lost) {
        CHECK(stats.jump_tables[0].vram == table);
        CHECK(stats.jump_tables[0].rom == romBase + tableOffset);
        CHECK(stats.jump_tables[0].entries.size() == 47);
        CHECK(stats.jump_tables[0].entries.back() == target);
        CHECK(output.find("L_80805AE0:") != std::string::npos);
        CHECK(output.find("default: switch_error") != std::string::npos);
    }
}

int main(int argc, char **argv) {
    const bool parent = argc > 1 && std::string(argv[1]) == "parent";
    for (uint32_t offset : {0x7FFCU, 0x8000U, 0xA494U, 0xFFFCU, 0x10000U}) {
        for (bool relocated : {false, true}) for (bool addiu : {false, true}) run(offset, relocated, addiu, parent);
    }
    std::puts(parent ? "PASS: exact parent loses signed-low relocated tables and emits external lookup; low-positive and nonrelocated controls remain intact"
                     : "PASS: 20 real analyzer/emitter controls preserve signed HI16/LO16 carry, data-after-code tables, all 47 constructed repeated slots and strict switch errors");
}
