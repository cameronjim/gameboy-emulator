#include "gameboy.hpp"

#include "test_rom.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <string>
#include <vector>

TEST_CASE("rom_string_reaches_serial_sink") {
    std::vector<uint8_t> rom = make_test_rom();
    // entry: write 'O','K' through sb/sc, then halt
    const uint8_t code[] = {
        0x3E, 'O',  // ld a, 'o'
        0xE0, 0x01, // ldh (sb), a
        0x3E, 0x81, // ld a, 0x81
        0xE0, 0x02, // ldh (sc), a
        0x3E, 'K',  // ld a, 'k'
        0xE0, 0x01, // ldh (sb), a
        0x3E, 0x81, // ld a, 0x81
        0xE0, 0x02, // ldh (sc), a
        0x76,       // halt
    };
    for (size_t i = 0; i < sizeof(code); ++i) {
        rom[0x0100 + i] = code[i];
    }
    rom[0x014D] = test_rom_checksum(rom);

    gb::Gameboy gameboy;
    std::string got;
    gameboy.set_serial_sink([&got](uint8_t b) { got.push_back(static_cast<char>(b)); });
    REQUIRE(gameboy.load_rom(rom));
    gameboy.run_frame();
    REQUIRE(got == "OK");
    // one frame advances at least the dmg frame budget
    REQUIRE(gameboy.cycles() >= 70224);
}

TEST_CASE("timer_interrupt_serviced_after_ime_set") {
    std::vector<uint8_t> rom = make_test_rom();
    // entry: arm the fastest timer, enable only the timer interrupt, ei, spin
    const uint8_t code[] = {
        0x3E, 0x05, // ld a, 0x05
        0xE0, 0x07, // ldh (tac), a
        0x3E, 0x04, // ld a, timer bit
        0xE0, 0xFF, // ldh (ie), a
        0x3E, 0xFE, // ld a, 0xfe
        0xE0, 0x05, // ldh (tima), a
        0xAF,       // xor a
        0xE0, 0x0F, // ldh (if), a
        0xFB,       // ei
        0x00,       // nop
        0x18, 0xFE, // jr -2
    };
    for (size_t i = 0; i < sizeof(code); ++i) {
        rom[0x0100 + i] = code[i];
    }
    // timer vector: report by writing 'I' to the serial sink, then spin
    const uint8_t handler[] = {
        0x3E, 'I',  // ld a, 'i'
        0xE0, 0x01, // ldh (sb), a
        0x3E, 0x81, // ld a, 0x81
        0xE0, 0x02, // ldh (sc), a
        0x18, 0xFE, // jr -2
    };
    for (size_t i = 0; i < sizeof(handler); ++i) {
        rom[0x0050 + i] = handler[i];
    }
    rom[0x014D] = test_rom_checksum(rom);

    gb::Gameboy gameboy;
    std::string got;
    gameboy.set_serial_sink([&got](uint8_t b) { got.push_back(static_cast<char>(b)); });
    REQUIRE(gameboy.load_rom(rom));
    gameboy.run_frame();
    REQUIRE(got == "I");
}

TEST_CASE("load_rom_rejects_invalid_bytes") {
    gb::Gameboy gameboy;
    const std::vector<uint8_t> junk = {0x01, 0x02, 0x03};
    REQUIRE(!gameboy.load_rom(junk));
    // a gameboy without a cartridge still renders the test pattern
    gameboy.run_frame();
    REQUIRE(gameboy.cycles() == 0);
    REQUIRE(gameboy.framebuffer().size() == 23040u);
}
