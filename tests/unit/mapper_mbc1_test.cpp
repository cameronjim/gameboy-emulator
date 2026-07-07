#include "mapper_mbc1.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <vector>

namespace {

// each 16kb bank's first byte is its bank number
std::vector<uint8_t> make_banked_rom(uint32_t banks) {
    std::vector<uint8_t> rom(banks * 0x4000, 0);
    for (uint32_t b = 0; b < banks; ++b) {
        rom[b * 0x4000] = static_cast<uint8_t>(b);
    }
    return rom;
}

} // namespace

TEST_CASE("mbc1_bank0_write_selects_bank1") {
    gb::MapperMbc1 mapper(make_banked_rom(4), 0);
    mapper.write_rom(0x2000, 0x00);
    REQUIRE(mapper.read_rom(0x4000) == 1);
    mapper.write_rom(0x2000, 0x02);
    REQUIRE(mapper.read_rom(0x4000) == 2);
}

TEST_CASE("mbc1_bank_masked_to_rom_size") {
    gb::MapperMbc1 mapper(make_banked_rom(4), 0);
    // bank 0x1f masks to 4 banks -> bank 3
    mapper.write_rom(0x2000, 0x1F);
    REQUIRE(mapper.read_rom(0x4000) == 3);
    mapper.write_rom(0x2000, 0x05);
    REQUIRE(mapper.read_rom(0x4000) == 1);
}

TEST_CASE("mbc1_ram_enable_gates_reads_and_writes") {
    gb::MapperMbc1 mapper(make_banked_rom(2), 0x2000);
    mapper.write_ram(0x0000, 0x42);
    REQUIRE(mapper.read_ram(0x0000) == 0xFF);
    mapper.write_rom(0x0000, 0x0A);
    mapper.write_ram(0x0000, 0x42);
    REQUIRE(mapper.read_ram(0x0000) == 0x42);
    mapper.write_rom(0x0000, 0x00);
    REQUIRE(mapper.read_ram(0x0000) == 0xFF);
}

TEST_CASE("mbc1_mode1_remaps_lower_region") {
    // 64 banks = 1mb, so bank2 selects bank 0x20 for the lower region
    gb::MapperMbc1 mapper(make_banked_rom(64), 0);
    mapper.write_rom(0x4000, 0x01);
    REQUIRE(mapper.read_rom(0x0000) == 0);
    mapper.write_rom(0x6000, 0x01);
    REQUIRE(mapper.read_rom(0x0000) == 0x20);
    // upper region composes bank2 << 5 | bank1
    mapper.write_rom(0x2000, 0x02);
    REQUIRE(mapper.read_rom(0x4000) == 0x22);
}

TEST_CASE("mbc1_external_ram_span_exposed") {
    gb::MapperMbc1 mapper(make_banked_rom(2), 0x2000);
    mapper.write_rom(0x0000, 0x0A);
    mapper.write_ram(0x0010, 0x99);
    REQUIRE(mapper.external_ram().size() == 0x2000);
    REQUIRE(mapper.external_ram()[0x0010] == 0x99);
}
