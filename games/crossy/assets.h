#ifndef ASSETS_H
#define ASSETS_H

#include <stdint.h>

// one 8x8 tile at 0xa0: pale grass with sprigs, drawn 2x2 to fill a 16 px cell
extern const uint8_t kGrassTile[16];

// one 8x8 tile at 0xa7: the odd lane's grass, dappled a shade darker than the even lane's
extern const uint8_t kGrassAltTile[16];

// one 8x8 tile at 0xa1: dark mottled canopy, drawn 2x2 to fill a 16 px cell
extern const uint8_t kTreeTile[16];

// one 8x8 tile at 0xa2: flat asphalt, drawn 2x2 to fill a 16 px cell
extern const uint8_t kRoadTile[16];

// one 8x8 tile at 0xa3: asphalt carrying a dash of the lane's center line
extern const uint8_t kRoadStripeTile[16];

// one 8x8 tile at 0xa4: the shallow lane's water, drawn over both of its tile rows
extern const uint8_t kWaterTile[16];

// one 8x8 tile at 0xa8: the same water a shade deeper; odd water lanes take it whole
extern const uint8_t kWaterDarkTile[16];

// one 8x8 tile at 0xa5: a rail across ties, drawn 2x2 so a lane carries two rails
extern const uint8_t kRailTile[16];

// one 8x8 tile at 0xa6: the crossbuck warning light, placed once near the lane center
extern const uint8_t kRailWarnTile[16];

// six 8x8 tiles at 0xb4: three 8x16 sprites left to right, one 24x16 log with rounded ends
extern const uint8_t kLogTiles[96];

// four 8x8 tiles at 0xb0: two 8x16 sprites, the left then the right half of one 16x16 car
extern const uint8_t kCarTiles[64];

// four 8x8 tiles at 0xbc: two 8x16 sprites, the left then the right half of one 16x16 eagle
extern const uint8_t kEagleTiles[64];

// eight 8x8 tiles at 0xc0: four 8x16 sprites, nose, two carriages and tail of a 48x16 train
extern const uint8_t kTrainTiles[128];

// four 8x8 tiles at 0xe0: two 8x16 chick frames, standing then lifted with its legs out
extern const uint8_t kChickTiles[64];

// twenty 8x8 tiles at 0xc8: ten 8x16 digits, '0' first; the badge is the pair's top half
extern const uint8_t kDigitTiles[320];

#endif
