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

// gbdk's ibm font lands ascii 0x20-0x7f on tiles 0x00-0x5f
#define kFontFirstChar 0x20U
#define kFontFirstTile 0x00U

// clear of the font tiles (0-95) and the score digits reserved at 0xD0-0xD9
#define kBirdTileId 0xE0U
#define kDigitTileId 0xD0U
#define kDigitCount 10U
// oam coords are offset by 8,16 from the screen; screen x stays 40
#define kBirdOamX 48U
#define kBirdOamYOffset 16U
#define kBirdScreenX 40U
#define kBirdSizePx 8U

// world tile ids; the sky is the font's space glyph so it matches a cleared screen
#define kSkyTileId 0x00U
#define kPipeBodyLeftTileId 0xA0U
#define kPipeBodyRightTileId 0xA1U
#define kPipeCapLeftTileId 0xA2U
#define kPipeCapRightTileId 0xA3U
#define kGroundTileId 0xB0U

// the bg map is a 32 column ring; columns are rewritten as they scroll off the left
#define kMapCols 32U
#define kMapRows 18U  // 16 sky rows plus the ground
#define kPlayRows 16U // sky rows above the ground
#define kGroundTopPx 128U

#define kPipeWidthCols 2U    // 16 px
#define kPipeSpacingCols 12U // 96 px between pipe pairs
#define kPipeGapRows 6U      // 48 px of clear air
#define kFirstPipeCol 24U    // first pipe pair starts off the right edge
#define kGapTopMin 1U        // one row of pipe always shows above the gap
#define kGapTopMax 9U        // 9 + 6 = 15, so one row always shows below it too

#define kPipeWidthPx 16U
#define kPipeSpacingPx 96U
#define kPipeGapPx 48U
#define kFirstPipeWorldX 192U // kFirstPipeCol * 8

// lcg full period mod 256: multiplier is 1 mod 4 and the increment is odd
#define kRngMul 37U
#define kRngAdd 1U

// hud: three digit sprites top-center, oam coords offset by 8,16 from the screen
#define kHudFirstSprite 1U
#define kHudDigits 3U
#define kHudOamY 32U       // screen y 16
#define kHudCenterOamX 88U // screen x 80; the run is centered by half a digit per digit
#define kDigitWidthPx 8U
#define kScoreMax 999U // three digits is all the hud and the banner can show

// window banner sits over the bottom 56 px; wx 7 puts its left edge at screen x 0
#define kWinX 7U
#define kWinY 88U
#define kWinCols 20U
#define kWinRows 7U
#define kOverTextX 5U
#define kOverTextY 0U
#define kScoreTextX 5U
#define kScoreTextY 1U
#define kBestTextX 6U // one right of score so both numbers start in the same column
#define kBestTextY 2U
#define kOverPromptX 4U
#define kOverPromptY 3U  // clear of the ground row where a fallen bird comes to rest
#define kWinGroundRow 5U // (128 - kWinY) / 8, so the banner's ground lines up with the bg's

// 8.8 fixed point: 256 units is one pixel
#define kFixedShift 8
#define kBirdStartY 15360 // 60.0 px
#define kBirdCeilingY 0   // 0.0 px
#define kBirdFloorY 30720 // 120.0 px: bird bottom on the ground, and 8.8 y stays inside int16_t
#define kGravityVy 51     // +0.20 px/frame^2
#define kFlapVy (-563)    // -2.20 px/frame
#define kTerminalVy 896   // +3.50 px/frame

// battery sram, mbc1 bank 0: 4 magic bytes then the best score little endian
#define kSramBase 0xA000U
#define kSaveMagic0 'F'
#define kSaveMagic1 'L'
#define kSaveMagic2 'P'
#define kSaveMagic3 'Y'
#define kSaveBestOffset 4U

// apu: master on, every channel to both ears, both ears at max volume
#define kNr52On 0x80U
#define kNr51All 0xFFU
#define kNr50Max 0x77U

// flap: ch1, 50% duty, fast downward sweep, envelope decays in ~0.23 s
#define kFlapSweep 0x1EU
#define kFlapDuty 0x80U
#define kFlapEnvelope 0xF1U
#define kFlapFreqLo 0x00U
#define kFlapFreqHi 0x86U // trigger plus freq 0x600: 256 hz

// score: ch2, an octave above the flap and just as short
#define kScoreDuty 0x80U
#define kScoreEnvelope 0xF1U
#define kScoreFreqLo 0x00U
#define kScoreFreqHi 0x87U // trigger plus freq 0x700: 512 hz

// hit: ch4 noise, 15 bit lfsr, slower decay so the crash rings out
#define kHitLength 0x00U
#define kHitEnvelope 0xF3U
#define kHitPoly 0x35U
#define kHitTrigger 0x80U

#endif
