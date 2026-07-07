#include "cartridge.hpp"

#include "test_rom.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace {

std::vector<uint8_t> make_rom(uint8_t type = 0x00, uint8_t rom_size_byte = 0x00, size_t file_size = 0x8000) {
    return make_test_rom(type, rom_size_byte, file_size);
}

} // namespace

TEST_CASE("valid_rom_only_header_parses") {
    const std::vector<uint8_t> rom = make_rom();
    const std::optional<gb::Cartridge> cart = gb::Cartridge::parse(rom);
    REQUIRE(cart.has_value());
    REQUIRE(cart->title() == "TETRIS");
    REQUIRE(cart->type() == gb::CartType::RomOnly);
    REQUIRE(cart->rom_size() == 0x8000u);
    REQUIRE(cart->ram_size() == 0u);
}

TEST_CASE("title_trims_trailing_nulls") {
    const std::vector<uint8_t> rom = make_rom();
    const std::optional<gb::Cartridge> cart = gb::Cartridge::parse(rom);
    REQUIRE(cart.has_value());
    REQUIRE(cart->title().size() == 6);
}

TEST_CASE("declared_size_mismatch_rejects") {
    // header claims 64kb over a 32kb file
    const std::vector<uint8_t> rom = make_rom(0x00, 0x01, 0x8000);
    std::string reason;
    REQUIRE(!gb::Cartridge::parse(rom, &reason).has_value());
    REQUIRE(reason == "rom size mismatch");
}

TEST_CASE("bad_checksum_rejects") {
    std::vector<uint8_t> rom = make_rom();
    rom[0x014D] ^= 0xFF;
    std::string reason;
    REQUIRE(!gb::Cartridge::parse(rom, &reason).has_value());
    REQUIRE(reason == "header checksum mismatch");
}

TEST_CASE("unknown_cartridge_type_rejected") {
    std::string reason;
    REQUIRE(!gb::Cartridge::parse(make_rom(0x2A), &reason).has_value());
    REQUIRE(reason == "unknown cartridge type");
}

TEST_CASE("mbc_types_accepted_with_battery_flag") {
    const std::optional<gb::Cartridge> mbc1 = gb::Cartridge::parse(make_rom(0x01, 0x01, 0x10000));
    REQUIRE(mbc1.has_value());
    REQUIRE(mbc1->type() == gb::CartType::Mbc1);
    REQUIRE(!mbc1->has_battery());

    const std::optional<gb::Cartridge> mbc1b = gb::Cartridge::parse(make_rom(0x03, 0x01, 0x10000));
    REQUIRE(mbc1b.has_value());
    REQUIRE(mbc1b->type() == gb::CartType::Mbc1);
    REQUIRE(mbc1b->has_battery());

    const std::optional<gb::Cartridge> mbc3 = gb::Cartridge::parse(make_rom(0x13, 0x01, 0x10000));
    REQUIRE(mbc3.has_value());
    REQUIRE(mbc3->type() == gb::CartType::Mbc3);
    REQUIRE(mbc3->has_battery());
}

TEST_CASE("truncated_file_rejects") {
    for (size_t size : {size_t{0}, size_t{0x100}, size_t{0x14F}}) {
        const std::vector<uint8_t> rom(size, 0);
        std::string reason;
        REQUIRE(!gb::Cartridge::parse(rom, &reason).has_value());
        REQUIRE(reason == "file smaller than header");
    }
}
