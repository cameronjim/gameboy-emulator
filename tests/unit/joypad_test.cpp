#include "joypad.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>

TEST_CASE("joypad_active_low_and_group_select") {
    gb::Joypad joypad;
    joypad.set_button(gb::Button::Right, true);
    joypad.set_button(gb::Button::Start, true);
    // directions selected: right (bit 0) reads 0
    joypad.write(0x20);
    REQUIRE(joypad.read() == 0xEE);
    // actions selected: start (bit 3) reads 0
    joypad.write(0x10);
    REQUIRE(joypad.read() == 0xD7);
    joypad.set_button(gb::Button::Right, false);
    joypad.write(0x20);
    REQUIRE(joypad.read() == 0xEF);
}

TEST_CASE("unselected_joypad_bits_read_one") {
    gb::Joypad joypad;
    joypad.set_button(gb::Button::A, true);
    joypad.set_button(gb::Button::Down, true);
    // nothing selected: low nibble all ones
    joypad.write(0x30);
    REQUIRE(joypad.read() == 0xFF);
}
