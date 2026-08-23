#include "palette.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>

TEST_CASE("colorize_background_grid_and_white_text") {
    // shade 0 renders the faint beveled grid in game, ui shade 3 is always white
    REQUIRE(colorize(0x00, 0, 3, 3, 0xFFFF, 0xFFFF) == kGridFace);
    REQUIRE(colorize(0x00, 0, 0, 3, 0xFFFF, 0xFFFF) == kGridLight);
    REQUIRE(colorize(0x00, 0, 3, 7, 0xFFFF, 0xFFFF) == kGridDark);
    REQUIRE(colorize(0x83, 0, 3, 3, 0xFFFF, 0xFFFF) == kGridFace);
    REQUIRE(colorize(0x41, 3, 5, 5, 0xFFFF, 0xFFFF) == kWhite);
    // block-range tiles not flagged as styles render as ui, not colored blocks
    REQUIRE(colorize(0x83, 3, 5, 5, 0x0000, 0x0000) == kWhite);
}

TEST_CASE("colorize_menu_background_is_flat") {
    // no block bank loaded means a menu screen: no grid anywhere
    REQUIRE(colorize(0x00, 0, 3, 3, 0x0000, 0x0000) == kBlack);
    REQUIRE(colorize(0x00, 0, 0, 3, 0x0000, 0x0000) == kBlack);
    REQUIRE(colorize(0x00, 0, 3, 7, 0x0000, 0x0000) == kBlack);
}

TEST_CASE("colorize_blocks_by_slot_identity") {
    // a piece's slot decides its color, independent of position; probe cell centers
    REQUIRE(colorize(0x80, 2, 3, 3, 0xFFFF, 0xFFFF) == kPieceColors[0]);
    REQUIRE(colorize(0x84, 2, 11, 35, 0xFFFF, 0xFFFF) == kPieceColors[4]);
    REQUIRE(colorize(0x86, 2, 3, 3, 0xFFFF, 0xFFFF) == kPieceColors[6]);
    // slots beyond seven wrap
    REQUIRE(colorize(0x87, 2, 3, 3, 0xFFFF, 0xFFFF) == kPieceColors[0]);
}

TEST_CASE("colorize_bevel_edges_within_cell") {
    const uint32_t center = colorize(0x80, 2, 3, 3, 0xFFFF, 0xFFFF);
    const uint32_t top = colorize(0x80, 2, 3, 0, 0xFFFF, 0xFFFF);
    const uint32_t bottom = colorize(0x80, 2, 3, 7, 0xFFFF, 0xFFFF);
    REQUIRE(center == kPieceColors[0]);
    REQUIRE(top != center);
    REQUIRE(bottom != center);
    REQUIRE(top != bottom);
}

TEST_CASE("colorize_falling_piece_by_sprite_tile") {
    // the falling piece's sprite tile mirrors its block slot, so it wears its
    // own color from the first frame and never changes on lock
    REQUIRE(colorize(0x102, 2, 3, 3, 0xFFFF, 0xFFFF) == kPieceColors[2]);
    REQUIRE(colorize(0x100, 2, 3, 3, 0xFFFF, 0xFFFF) == kPieceColors[0]);
    REQUIRE(colorize(0x106, 2, 3, 3, 0xFFFF, 0xFFFF) == kPieceColors[6]);
    // falling and locked color agree for the same slot
    REQUIRE(colorize(0x104, 2, 3, 3, 0xFFFF, 0xFFFF) == colorize(0x84, 2, 3, 3, 0xFFFF, 0xFFFF));
    // sprites outside the block bank render as ui shades
    REQUIRE(colorize(0x142, 3, 3, 3, 0xFFFF, 0xFFFF) == kWhite);
    REQUIRE(colorize(0x102, 3, 3, 3, 0x0000, 0x0000) == kWhite);
    // low-tile ui sprites whose vram slot is not a style keep their own art
    REQUIRE(colorize(0x104, 3, 3, 3, 0xFFFF, 0x0000) == kWhite);
    // a sprite reusing the bg block bank is a block (the editor's tile row)
    REQUIRE(colorize(0x180, 2, 3, 3, 0xFFFF, 0x0000) == kPieceColors[0]);
}
