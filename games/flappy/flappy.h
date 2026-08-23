#ifndef FLAPPY_H
#define FLAPPY_H

// tuning constants live here so logic files never carry magic numbers

// identity bgp; gbdk's font clears to index 0, so a blank screen is white
#define kTitleBgp 0xE4U
// identity obp; the bird's colors 1-3 stay distinct against the white sky
#define kBirdObp 0xE4U

#define kTitleTextX 7U
#define kTitleTextY 6U
#define kPromptTextX 4U
#define kPromptTextY 10U

// clear of the font tiles (0-95) and the score digits reserved at 0xD0-0xD9
#define kBirdTileId 0xE0U
// oam coords are offset by 8,16 from the screen; screen x stays 40
#define kBirdOamX 48U
#define kBirdOamYOffset 16U

// 8.8 fixed point: 256 units is one pixel
#define kFixedShift 8
#define kBirdStartY 15360 // 60.0 px
#define kBirdCeilingY 0   // 0.0 px
#define kBirdFloorY 30720 // 120.0 px; keeping y under 128 px keeps the 8.8 math inside int16_t
#define kGravityVy 51     // +0.20 px/frame^2
#define kFlapVy (-563)    // -2.20 px/frame
#define kTerminalVy 896   // +3.50 px/frame

#endif
