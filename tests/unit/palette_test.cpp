#include "palette.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>

TEST_CASE("palette_mapping_covers_all_four_indices") {
    for (uint8_t i = 0; i < 4; ++i) {
        REQUIRE(map_shade(kGreenPalette, i) == kGreenPalette.shades[i]);
        REQUIRE(map_shade(kGrayPalette, i) == kGrayPalette.shades[i]);
    }
    // out-of-range indices stay inside the table
    REQUIRE(map_shade(kGreenPalette, 0xFF) == kGreenPalette.shades[3]);
    REQUIRE(palette_by_name("gray").shades == kGrayPalette.shades);
    REQUIRE(palette_by_name("green").shades == kGreenPalette.shades);
    REQUIRE(palette_by_name("unknown").shades == kGreenPalette.shades);
}
