#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

struct Palette {
    std::array<uint32_t, 4> shades;
};

// shade 0 is the background, 1-2 are block fills, 3 is outlines and text
inline constexpr Palette kDarkMulti{{0xFF16161Eu, 0xFF00B8D4u, 0xFFFFB300u, 0xFFF2F2F2u}};
inline constexpr Palette kLightMulti{{0xFFF5EEDCu, 0xFF0288D1u, 0xFFE64A19u, 0xFF1A1A2Eu}};
inline constexpr Palette kLightMono{{0xFFF5EEDCu, 0xFF8FB6D4u, 0xFF33608Cu, 0xFF142033u}};
inline constexpr Palette kDarkMono{{0xFF141C2Au, 0xFF3E5C7Au, 0xFF8FB6D4u, 0xFFF5EEDCu}};
inline constexpr Palette kGreenPalette{{0xFFE0F8D0u, 0xFF88C070u, 0xFF346856u, 0xFF081820u}};
inline constexpr Palette kGrayPalette{{0xFFFFFFFFu, 0xFFAAAAAAu, 0xFF555555u, 0xFF000000u}};

struct NamedPalette {
    std::string_view name;
    const Palette* palette;
};

// cycle order for the theme key; first entry is the default
inline constexpr std::array<NamedPalette, 6> kThemes = {{
    {"dark-multi", &kDarkMulti},
    {"light-multi", &kLightMulti},
    {"dark-mono", &kDarkMono},
    {"light-mono", &kLightMono},
    {"green", &kGreenPalette},
    {"gray", &kGrayPalette},
}};

inline uint32_t map_shade(const Palette& palette, uint8_t index) {
    // out-of-range indices clamp into the four shades
    return palette.shades[index & 0x3u];
}

inline size_t palette_index_by_name(std::string_view name) {
    for (size_t i = 0; i < kThemes.size(); ++i) {
        if (kThemes[i].name == name) {
            return i;
        }
    }
    return 0;
}

inline const Palette& palette_by_name(std::string_view name) {
    return *kThemes[palette_index_by_name(name)].palette;
}
