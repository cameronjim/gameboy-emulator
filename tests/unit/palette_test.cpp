#include "palette.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>

TEST_CASE("colorize_background_grid_and_white_text") {
    // shade 0 renders the faint beveled grid, ui shade 3 is always white
    REQUIRE(colorize(0x00, 0, 3, 3, 0xFFFF, 0xFF) == kGridFace);
    REQUIRE(colorize(0x00, 0, 0, 3, 0xFFFF, 0xFF) == kGridLight);
    REQUIRE(colorize(0x00, 0, 3, 7, 0xFFFF, 0xFF) == kGridDark);
    REQUIRE(colorize(0x83, 0, 3, 3, 0xFFFF, 0xFF) == kGridFace);
    REQUIRE(colorize(0x41, 3, 5, 5, 0xFFFF, 0xFF) == kWhite);
    // block-range tiles not flagged as styles render as ui, not colored blocks
    REQUIRE(colorize(0x83, 3, 5, 5, 0x0000, 0xFF) == kWhite);
}

TEST_CASE("colorize_blocks_by_slot_identity") {
    // a piece's slot decides its color, independent of position; probe cell centers
    REQUIRE(colorize(0x80, 2, 3, 3, 0xFFFF, 0xFF) == kPieceColors[0]);
    REQUIRE(colorize(0x84, 2, 11, 35, 0xFFFF, 0xFF) == kPieceColors[4]);
    REQUIRE(colorize(0x86, 2, 3, 3, 0xFFFF, 0xFF) == kPieceColors[6]);
    // slots beyond seven wrap
    REQUIRE(colorize(0x87, 2, 3, 3, 0xFFFF, 0xFF) == kPieceColors[0]);
}

TEST_CASE("colorize_bevel_edges_within_cell") {
    const uint32_t center = colorize(0x80, 2, 3, 3, 0xFFFF, 0xFF);
    const uint32_t top = colorize(0x80, 2, 3, 0, 0xFFFF, 0xFF);
    const uint32_t bottom = colorize(0x80, 2, 3, 7, 0xFFFF, 0xFF);
    REQUIRE(center == kPieceColors[0]);
    REQUIRE(top != center);
    REQUIRE(bottom != center);
    REQUIRE(top != bottom);
}

TEST_CASE("colorize_falling_piece_follows_tracked_slot") {
    // sprite pixels take the tracked falling slot's color
    REQUIRE(colorize(0x102, 2, 3, 3, 0xFFFF, 4) == kPieceColors[4]);
    REQUIRE(colorize(0x102, 2, 3, 3, 0xFFFF, 0) == kPieceColors[0]);
    // unknown falling slot renders neutral
    REQUIRE(colorize(0x102, 2, 3, 3, 0xFFFF, 0xFF) == kUnknownPiece);
}
