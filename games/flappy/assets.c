#include "assets.h"

// low byte then high byte per row, leftmost pixel in bit 7
const uint8_t kBirdTile[16] = {
    0x00, 0x38, // ..222...
    0x38, 0x44, // .21112..
    0x7C, 0x8A, // 2111312.
    0x7D, 0x83, // 21111123
    0x1D, 0xE3, // 22211123
    0x1C, 0x62, // .221112.
    0x18, 0x24, // ..2112..
    0x00, 0x18, // ...22...
};
