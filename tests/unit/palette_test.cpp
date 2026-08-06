#include "palette.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>

TEST_CASE("colorize_black_background_white_text") {
    // shade 0 is always black, ui shade 3 is always white
    REQUIRE(colorize(0x00, 0, 5, 5, 0xFFFF) == kBlack);
    REQUIRE(colorize(0x83, 0, 5, 5, 0xFFFF) == kBlack);
    REQUIRE(colorize(0x41, 3, 5, 5, 0xFFFF) == kWhite);
    // block-range tiles not flagged as styles render as ui, not colored blocks
    REQUIRE(colorize(0x83, 3, 5, 5, 0x0000) == kWhite);
}

TEST_CASE("colorize_blocks_by_cell_and_falling_by_accent") {
    // same cell, same color; block tiles pick from the piece colors
    const uint32_t a = colorize(0x83, 2, 16, 40, 0xFFFF);
    REQUIRE(a == colorize(0x8A, 2, 17, 41, 0xFFFF));
    bool found = false;
    for (uint32_t c : kPieceColors) {
        found = found || c == a;
    }
    REQUIRE(found);
    // sprite-layer blocks are the falling piece accent
    REQUIRE(colorize(0x183, 2, 16, 40, 0xFFFF) == kFallingColor);
    // fills are darker than highlights
    REQUIRE(colorize(0x83, 1, 16, 40, 0xFFFF) != colorize(0x83, 3, 16, 40, 0xFFFF));
}
