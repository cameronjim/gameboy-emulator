#include "palette.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>

TEST_CASE("palette_mapping_covers_all_four_indices") {
    for (const NamedPalette& theme : kThemes) {
        for (uint8_t i = 0; i < 4; ++i) {
            REQUIRE(map_shade(*theme.palette, i) == theme.palette->shades[i]);
        }
    }
    // out-of-range indices stay inside the table
    REQUIRE(map_shade(kGreenPalette, 0xFF) == kGreenPalette.shades[3]);
    REQUIRE(palette_by_name("green").shades == kGreenPalette.shades);
    REQUIRE(palette_by_name("dark-multi").shades == kDarkMulti.shades);
    REQUIRE(palette_by_name("light-mono").shades == kLightMono.shades);
    // unknown names fall back to the default theme
    REQUIRE(palette_index_by_name("unknown") == 0);
    REQUIRE(kThemes[0].name == "dark-multi");
}
