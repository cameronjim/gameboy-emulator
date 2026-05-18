#include "mapper_rom.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <utility>
#include <vector>

TEST_CASE("rom_read_masks_to_size") {
    std::vector<uint8_t> rom(0x4000, 0);
    rom[0x0000] = 0xAA;
    rom[0x1234] = 0x55;
    const gb::MapperRom mapper(std::move(rom));
    REQUIRE(mapper.read_rom(0x1234) == 0x55);
    REQUIRE(mapper.read_rom(0x4000) == 0xAA);
}

TEST_CASE("rom_writes_ignored_and_ram_reads_open_bus") {
    std::vector<uint8_t> rom(0x8000, 0x11);
    gb::MapperRom mapper(std::move(rom));
    mapper.write_rom(0x0100, 0x99);
    REQUIRE(mapper.read_rom(0x0100) == 0x11);
    mapper.write_ram(0x0000, 0x42);
    REQUIRE(mapper.read_ram(0x0000) == 0xFF);
}
