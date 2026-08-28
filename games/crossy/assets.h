#ifndef ASSETS_H
#define ASSETS_H

#include <stdint.h>

// one 8x8 tile at 0xa0: pale grass with sprigs, drawn 2x2 to fill a 16 px cell
extern const uint8_t kGrassTile[16];

// one 8x8 tile at 0xa1: dark mottled canopy, drawn 2x2 to fill a 16 px cell
extern const uint8_t kTreeTile[16];

// one 8x8 tile at 0xa2: flat asphalt, drawn 2x2 to fill a 16 px cell
extern const uint8_t kRoadTile[16];

// one 8x8 tile at 0xa3: asphalt carrying a dash of the lane's center line
extern const uint8_t kRoadStripeTile[16];

// one 8x8 tile at 0xa4: pale water rippled with dark waves, drawn 2x2 to fill a 16 px cell
extern const uint8_t kWaterTile[16];

// one 8x8 sprite at 0xc4: a dark slab of log, drawn three across to make one 24x8 log
extern const uint8_t kLogTile[16];

// two 8x8 sprites at 0xc0: the front then the rear half of one 16x8 car
extern const uint8_t kCarTiles[32];

// two 8x8 sprites at 0xcc: the left then the right wing of one 16x8 eagle
extern const uint8_t kEagleTiles[32];

// two 8x8 sprites at 0xe0: the chick standing, then lifted with its legs out
extern const uint8_t kChickTiles[32];

// ten 8x8 sprite digits at 0xd0, '0' first; each glyph sits in an opaque badge
extern const uint8_t kDigitTiles[160];

#endif
