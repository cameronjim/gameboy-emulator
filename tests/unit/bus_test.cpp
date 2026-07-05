#include "bus.hpp"

#include "interrupts.hpp"
#include "mapper_rom.hpp"
#include "serial.hpp"
#include "timer.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <utility>
#include <vector>

namespace {

struct Rig {
    gb::Serial serial;
    gb::InterruptLine irq;
    gb::Timer timer{irq};
    gb::Ppu ppu{irq};
    gb::Joypad joypad;
    gb::Bus bus{serial, timer, ppu, joypad, irq};
};

} // namespace

TEST_CASE("timer_registers_route_through_bus") {
    Rig rig;
    rig.bus.write8(gb::kRegTma, 0x42);
    REQUIRE(rig.bus.read8(gb::kRegTma) == 0x42);
    rig.bus.write8(gb::kRegTac, 0x05);
    REQUIRE(rig.bus.read8(gb::kRegTac) == 0xFD);
    rig.bus.write8(gb::kRegDiv, 0x99);
    REQUIRE(rig.bus.read8(gb::kRegDiv) == 0x00);
}

TEST_CASE("echo_ram_mirrors_wram_both_directions") {
    Rig rig;
    rig.bus.write8(0xC123, 0x42);
    REQUIRE(rig.bus.read8(0xE123) == 0x42);
    rig.bus.write8(0xF000, 0x99);
    REQUIRE(rig.bus.read8(0xD000) == 0x99);
}

TEST_CASE("unusable_region_reads_ff_ignores_writes") {
    Rig rig;
    rig.bus.write8(0xFEA0, 0x12);
    REQUIRE(rig.bus.read8(0xFEA0) == 0xFF);
    REQUIRE(rig.bus.read8(0xFEFF) == 0xFF);
}

TEST_CASE("unmapped_io_reads_ff") {
    Rig rig;
    REQUIRE(rig.bus.read8(0xFF10) == 0xFF);
    REQUIRE(rig.bus.read8(0xFF4C) == 0xFF);
    REQUIRE(rig.bus.read8(0xFF7F) == 0xFF);
}

TEST_CASE("if_upper_bits_read_ones") {
    Rig rig;
    // pandocs power-up value
    REQUIRE(rig.bus.read8(gb::kRegIf) == 0xE1);
    rig.bus.write8(gb::kRegIf, 0x00);
    REQUIRE(rig.bus.read8(gb::kRegIf) == 0xE0);
    rig.bus.write8(gb::kRegIf, 0xFF);
    REQUIRE(rig.bus.read8(gb::kRegIf) == 0xFF);
}

TEST_CASE("serial_sink_receives_bytes_on_sc_81") {
    Rig rig;
    std::vector<uint8_t> got;
    rig.serial.set_sink([&got](uint8_t b) { got.push_back(b); });
    rig.bus.write8(gb::kRegSb, 'H');
    rig.bus.write8(gb::kRegSc, 0x81);
    rig.bus.write8(gb::kRegSb, 'i');
    rig.bus.write8(gb::kRegSc, 0x80);
    REQUIRE(got == std::vector<uint8_t>{'H'});
}

TEST_CASE("oam_dma_copies_160_bytes") {
    Rig rig;
    for (uint16_t i = 0; i < 0xA0; ++i) {
        rig.bus.write8(static_cast<uint16_t>(0xC000 + i), static_cast<uint8_t>(i + 1));
    }
    rig.bus.write8(0xFF46, 0xC0);
    for (uint16_t i = 0; i < 0xA0; ++i) {
        REQUIRE(rig.bus.read8(static_cast<uint16_t>(0xFE00 + i)) == static_cast<uint8_t>(i + 1));
    }
    REQUIRE(rig.bus.read8(0xFF46) == 0xC0);
}

TEST_CASE("read16_is_little_endian") {
    Rig rig;
    rig.bus.write8(0xC000, 0x34);
    rig.bus.write8(0xC001, 0x12);
    REQUIRE(rig.bus.read16(0xC000) == 0x1234);
    rig.bus.write16(0xC100, 0xBEEF);
    REQUIRE(rig.bus.read8(0xC100) == 0xEF);
    REQUIRE(rig.bus.read8(0xC101) == 0xBE);
}

TEST_CASE("rom_region_routes_through_mapper") {
    Rig rig;
    REQUIRE(rig.bus.read8(0x0000) == 0xFF);
    std::vector<uint8_t> rom(0x8000, 0);
    rom[0x0000] = 0x11;
    rom[0x7FFF] = 0x22;
    gb::MapperRom mapper(std::move(rom));
    rig.bus.attach_mapper(mapper);
    REQUIRE(rig.bus.read8(0x0000) == 0x11);
    REQUIRE(rig.bus.read8(0x7FFF) == 0x22);
    REQUIRE(rig.bus.read8(0xA000) == 0xFF);
}

TEST_CASE("hram_and_ie_are_readable_writable") {
    Rig rig;
    rig.bus.write8(0xFF80, 0x5A);
    REQUIRE(rig.bus.read8(0xFF80) == 0x5A);
    rig.bus.write8(0xFFFE, 0xA5);
    REQUIRE(rig.bus.read8(0xFFFE) == 0xA5);
    rig.bus.write8(gb::kRegIe, 0x1F);
    REQUIRE(rig.bus.read8(gb::kRegIe) == 0x1F);
}
