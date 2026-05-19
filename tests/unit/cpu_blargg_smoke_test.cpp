#include "cpu.hpp"

#include "fake_bus.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <vector>

// runs when GB_BLARGG_ROM points at a blargg rom; ci skips until milestone 05 fetches roms
TEST_CASE("blargg_rom_executes_without_unknown_opcode") {
    const char* path = std::getenv("GB_BLARGG_ROM");
    if (path == nullptr) {
        SKIP("GB_BLARGG_ROM not set");
    }
    std::ifstream file(path, std::ios::binary);
    REQUIRE(file.good());
    const std::vector<uint8_t> rom((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    REQUIRE(rom.size() >= 0x8000);

    FakeBus bus;
    for (size_t i = 0; i < rom.size() && i < 0x8000; ++i) {
        bus.mem[i] = rom[i];
    }
    gb::Cpu cpu(bus);
    for (uint32_t i = 0; i < 500000; ++i) {
        cpu.step();
        if (cpu.status() != gb::CpuStatus::Running) {
            break;
        }
    }
    INFO("trap pc " << cpu.trap_pc() << " opcode " << static_cast<int>(cpu.trap_opcode()));
    REQUIRE(cpu.status() == gb::CpuStatus::Running);
}
