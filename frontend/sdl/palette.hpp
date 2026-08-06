#pragma once

#include <array>
#include <cstdint>

// the one look: black background, thin white ui, tetris-colored blocks
inline constexpr std::array<uint32_t, 7> kPieceColors = {
    0xFF00BCD4u, 0xFFFFC107u, 0xFF9C27B0u, 0xFF4CAF50u, 0xFFF44336u, 0xFF2196F3u, 0xFFFF9800u,
};
inline constexpr uint32_t kFallingColor = 0xFFFF9800u;
inline constexpr uint32_t kBlack = 0xFF000000u;
inline constexpr uint32_t kWhite = 0xFFFFFFFFu;

inline uint32_t scale_rgb(uint32_t c, uint32_t num, uint32_t den) {
    const uint32_t r = ((c >> 16) & 0xFF) * num / den;
    const uint32_t g = ((c >> 8) & 0xFF) * num / den;
    const uint32_t b = (c & 0xFF) * num / den;
    return 0xFF000000u | (r << 16) | (g << 8) | b;
}

inline uint32_t lighten_rgb(uint32_t c, uint32_t num, uint32_t den) {
    const uint32_t r = ((c >> 16) & 0xFF) + (255 - ((c >> 16) & 0xFF)) * num / den;
    const uint32_t g = ((c >> 8) & 0xFF) + (255 - ((c >> 8) & 0xFF)) * num / den;
    const uint32_t b = (c & 0xFF) + (255 - (c & 0xFF)) * num / den;
    return 0xFF000000u | (r << 16) | (g << 8) | b;
}

// id: low byte tile index, bit 8 sprite; block_mask marks tile slots 0x80-0x8f
// whose live vram content is one of the game's block styles
inline uint32_t colorize(uint16_t id, uint8_t shade, uint32_t x, uint32_t y, uint16_t block_mask) {
    if ((shade & 0x3u) == 0) {
        return kBlack;
    }
    const uint8_t tile = static_cast<uint8_t>(id & 0xFF);
    const bool sprite = (id & 0x100) != 0;
    if (sprite || (tile >= 0x80 && tile <= 0x8F && ((block_mask >> (tile - 0x80)) & 1u) != 0)) {
        // sprites (the falling piece, cursors) get the accent, locked blocks a per-cell color
        const uint32_t c =
            sprite ? kFallingColor : kPieceColors[((x / 8) * 7 + (y / 8) * 13) % kPieceColors.size()];
        switch (shade & 0x3u) {
        case 1:
            return scale_rgb(c, 5, 9);
        case 2:
            return c;
        default:
            return lighten_rgb(c, 5, 9);
        }
    }
    switch (shade & 0x3u) {
    case 1:
        return 0xFF4C4C55u;
    case 2:
        return 0xFF9C9CA8u;
    default:
        return kWhite;
    }
}
