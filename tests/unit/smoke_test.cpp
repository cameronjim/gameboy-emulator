#include "gameboy.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <span>

TEST_CASE("framebuffer_has_expected_size_and_range") {
    gb::Gameboy gameboy;
    gameboy.run_frame();
    const std::span<const uint8_t> fb = gameboy.framebuffer();
    REQUIRE(fb.size() == gb::kLcdWidth * gb::kLcdHeight);
    REQUIRE(fb.size() == 23040u);
    const bool in_range = std::all_of(fb.begin(), fb.end(), [](uint8_t v) { return v <= 3; });
    REQUIRE(in_range);
}
