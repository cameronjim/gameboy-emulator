#ifndef ASSETS_H
#define ASSETS_H

#include <stdint.h>

// one 8x8 tile at 0xa0: pale grass with sprigs, drawn 2x2 to fill a 16 px cell
extern const uint8_t kGrassTile[16];

// one 8x8 tile at 0xa1: dark mottled canopy, drawn 2x2 to fill a 16 px cell
extern const uint8_t kTreeTile[16];

// two 8x8 sprites at 0xe0: the chick standing, then lifted with its legs out
extern const uint8_t kChickTiles[32];

// ten 8x8 sprite digits at 0xd0, '0' first; each glyph sits in an opaque badge
extern const uint8_t kDigitTiles[160];

#endif
