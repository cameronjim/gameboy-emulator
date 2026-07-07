#include "mapper_mbc3.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <vector>

namespace {

std::vector<uint8_t> make_banked_rom(uint32_t banks) {
    std::vector<uint8_t> rom(banks * 0x4000, 0);
    for (uint32_t b = 0; b < banks; ++b) {
        rom[b * 0x4000] = static_cast<uint8_t>(b);
    }
    return rom;
}

} // namespace

TEST_CASE("mbc3_seven_bit_rom_bank") {
    gb::MapperMbc3 mapper(make_banked_rom(128), 0);
    mapper.write_rom(0x2000, 0x7F);
    REQUIRE(mapper.read_rom(0x4000) == 0x7F);
    mapper.write_rom(0x2000, 0x00);
    REQUIRE(mapper.read_rom(0x4000) == 1);
}

TEST_CASE("mbc3_ram_vs_rtc_select") {
    gb::MapperMbc3 mapper(make_banked_rom(2), 0x8000);
    mapper.write_rom(0x0000, 0x0A);
    mapper.write_rom(0x4000, 0x02);
    mapper.write_ram(0x0000, 0x55);
    REQUIRE(mapper.read_ram(0x0000) == 0x55);
    // bank 0 is a different 8kb page
    mapper.write_rom(0x4000, 0x00);
    REQUIRE(mapper.read_ram(0x0000) == 0x00);
    // rtc seconds register
    mapper.set_rtc_seconds(42);
    mapper.write_rom(0x4000, 0x08);
    REQUIRE(mapper.read_ram(0x0000) == 42);
}

TEST_CASE("mbc3_latch_on_00_01") {
    gb::MapperMbc3 mapper(make_banked_rom(2), 0);
    mapper.write_rom(0x0000, 0x0A);
    mapper.set_rtc_seconds(10);
    // live rtc moves; the latched copy only updates on a 00->01 write
    mapper.write_rom(0x4000, 0x08);
    mapper.write_ram(0x0000, 25);
    REQUIRE(mapper.read_ram(0x0000) == 10);
    mapper.write_rom(0x6000, 0x00);
    mapper.write_rom(0x6000, 0x01);
    REQUIRE(mapper.read_ram(0x0000) == 25);
}
