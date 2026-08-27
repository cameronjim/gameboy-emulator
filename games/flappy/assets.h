#ifndef ASSETS_H
#define ASSETS_H

#include <stdint.h>

// one 8x8 2bpp tile: the bird, facing right
extern const uint8_t kBirdTile[16];

// four tiles at 0xa0: pipe body left, body right, cap left, cap right
extern const uint8_t kPipeTiles[64];

extern const uint8_t kGroundTile[16];

// ten 8x8 sprite digits at 0xd0, '0' first
extern const uint8_t kDigitTiles[160];

#endif
