#include "palette.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>

TEST_CASE("colorize_black_background_white_text") {
    // shade 0 is always black, ui shade 3 is always white
    REQUIRE(colorize(0x00, 0, 0xFFFF, 0xFF) == kBlack);
    REQUIRE(colorize(0x83, 0, 0xFFFF, 0xFF) == kBlack);
    REQUIRE(colorize(0x41, 3, 0xFFFF, 0xFF) == kWhite);
    // block-range tiles not flagged as styles render as ui, not colored blocks
    REQUIRE(colorize(0x83, 3, 0x0000, 0xFF) == kWhite);
}

TEST_CASE("colorize_blocks_by_slot_identity") {
    // a piece's slot decides its color, independent of position or shade
    REQUIRE(colorize(0x80, 2, 0xFFFF, 0xFF) == kPieceColors[0]);
    REQUIRE(colorize(0x84, 2, 0xFFFF, 0xFF) == kPieceColors[4]);
    REQUIRE(colorize(0x86, 2, 0xFFFF, 0xFF) == kPieceColors[6]);
    // slots beyond seven wrap
    REQUIRE(colorize(0x87, 2, 0xFFFF, 0xFF) == kPieceColors[0]);
    // fills darker than highlights, same hue family
    REQUIRE(colorize(0x80, 1, 0xFFFF, 0xFF) != colorize(0x80, 3, 0xFFFF, 0xFF));
}

TEST_CASE("colorize_falling_piece_follows_tracked_slot") {
    // sprite pixels take the tracked falling slot's color
    REQUIRE(colorize(0x102, 2, 0xFFFF, 4) == kPieceColors[4]);
    REQUIRE(colorize(0x102, 2, 0xFFFF, 0) == kPieceColors[0]);
    // unknown falling slot renders neutral
    REQUIRE(colorize(0x102, 2, 0xFFFF, 0xFF) == kUnknownPiece);
}
