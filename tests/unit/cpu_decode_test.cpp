#include "cpu.hpp"

#include "fake_bus.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>

namespace {
#include "opcode_meta.inc"
} // namespace

static_assert(kOpcodeMeta.size() == 511, "255 unprefixed (prefix excluded) + 256 cb");

TEST_CASE("all_opcodes_match_json_length_and_cycles") {
    for (const OpcodeMeta& meta : kOpcodeMeta) {
        FakeBus bus;
        gb::Cpu cpu(bus);
        const uint16_t start = 0x0100;
        if (meta.cb) {
            bus.mem[start] = 0xCB;
            bus.mem[start + 1] = meta.opcode;
        } else {
            bus.mem[start] = meta.opcode;
        }
        const uint32_t cycles = cpu.step();
        const uint16_t delta = static_cast<uint16_t>(cpu.regs().pc - start);

        if (meta.illegal) {
            REQUIRE(cpu.status() == gb::CpuStatus::Stopped);
            continue;
        }
        REQUIRE(cpu.status() == gb::CpuStatus::Running);
        if (meta.cycles_branch == 0) {
            REQUIRE(cycles == meta.cycles);
        } else {
            REQUIRE((cycles == meta.cycles || cycles == meta.cycles_branch));
        }
        if (!meta.control) {
            REQUIRE(delta == meta.length);
        } else if (meta.cycles_branch != 0 && cycles == meta.cycles_branch) {
            // branch not taken: pc advanced past the full instruction
            REQUIRE(delta == meta.length);
        }
    }
}
