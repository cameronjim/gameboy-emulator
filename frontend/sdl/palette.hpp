#pragma once

#include <array>
#include <cstdint>
#include <string_view>

struct Palette {
    std::array<uint32_t, 4> shades;
};

inline constexpr Palette kGreenPalette{{0xFFE0F8D0u, 0xFF88C070u, 0xFF346856u, 0xFF081820u}};
inline constexpr Palette kGrayPalette{{0xFFFFFFFFu, 0xFFAAAAAAu, 0xFF555555u, 0xFF000000u}};

inline uint32_t map_shade(const Palette& palette, uint8_t index) {
    // out-of-range indices clamp into the four shades
    return palette.shades[index & 0x3u];
}

inline const Palette& palette_by_name(std::string_view name) {
    return name == "gray" ? kGrayPalette : kGreenPalette;
}
