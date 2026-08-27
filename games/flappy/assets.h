#ifndef ASSETS_H
#define ASSETS_H

#include <stdint.h>

// three 8x8 2bpp tiles at 0xe0: the bird facing right, wing up then glide then down
extern const uint8_t kBirdTiles[48];

// two 8x8 tiles at 0xb8: the banner's dark fill then its light border row
extern const uint8_t kPanelTiles[32];

// four tiles at 0xa0: pipe body left, body right, cap left, cap right
extern const uint8_t kPipeTiles[64];

extern const uint8_t kGroundTile[16];

// ten 8x8 sprite digits at 0xd0, '0' first; each glyph sits in an opaque badge
extern const uint8_t kDigitTiles[160];

#endif
