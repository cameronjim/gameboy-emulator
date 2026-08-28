#ifndef CROSSY_H
#define CROSSY_H

// tuning constants live here so logic files never carry magic numbers

// identity bgp; gbdk's font clears to index 0, so a blank screen is white
#define kTitleBgp 0xE4U
// identity obp; the chick's colors 1-3 stay distinct against the grass
#define kChickObp 0xE4U

// the visible screen is 20x18 cells; text lines are centered across the 20
#define kScreenCols 20U
#define kTitleTextY 6U
#define kPromptTextY 10U
#define kBestTextY 12U

// the grid: a lane is 16 px (2 tile rows), a column is 16 px (2 tile cols)
#define kCellPx 16U
#define kGridCols 10U
#define kMaxCol 9U

// bg terrain tile ids, per the milestone's tile contract
#define kGrassTileId 0xA0U
#define kTreeTileId 0xA1U
#define kRoadTileId 0xA2U
#define kRoadStripeTileId 0xA3U
#define kWaterTileId 0xA4U

// a lane is one of three kinds; the kind is cached per ring slot
#define kLaneGrass 0U
#define kLaneRoad 1U
#define kLaneWater 2U

// gbdk's ibm font lands ascii 0x20-0x7f on tiles 0x00-0x5f
#define kFontFirstChar 0x20U
#define kFontFirstTile 0x00U
#define kTileBytes 16U

// an inverted copy of the font's first 64 glyphs: light strokes on a solid dark cell
#define kInvFontFirstTile 0x60U
#define kInvFontTiles 64U
// inverted space is a solid dark cell, so it is both the popup's fill and its blank glyph
#define kPopupFillTileId kInvFontFirstTile

// sprites: the chick at rest and in flight, then the ten hud digits
#define kChickTileId 0xE0U
#define kChickHopTileId 0xE1U
#define kChickFrames 2U
#define kDigitTileId 0xD0U
#define kDigitCount 10U

// the bg ring is 32 rows tall, so 16 lanes are buffered and scy wraps for free
#define kMapRows 32U
#define kRingLanes 16U
#define kRingLaneMask 15U

// screen y of the camera lane's top row; 9 lanes fit on screen, 6 of them ahead
#define kCamLaneScreenY 96U
#define kLanesAhead 6U
#define kInitLanes 7U // lanes 0..6 are visible the frame the run starts
// the chick may trail the camera by two lanes and still be fully on screen
#define kMaxLanesBehind 2U

// the chick is one 8x8 sprite centered in its 16 px cell
#define kChickCellInset 4U
#define kChickHalfPx 4U
// on water the chick takes the lane's top half, clear of the log's scanlines entirely
#define kChickWaterInset 0U
#define kChickSpawnCol 4U
// on the title the chick stands one cell lower, clear of the BEST line
#define kHoverChickScreenY 116U
// oam coords are offset by 8,16 from the screen
#define kOamXOffset 8U
#define kOamYOffset 16U
#define kSpritePx 8U

// a hop slides 16 px over 8 frames
#define kHopSlidePx 16
#define kHopStepPx 2
// one cell of travel in the chick's 8.8 pixel x
#define kCellFixed 0x1000U

// generation: the first lanes are plain so the run always starts clear
#define kPlainLanes 3U
#define kMaxTreesPerLane 4U
// lanes alternate grass with a danger chunk; danger lanes never carry trees
#define kRoadChunkMin 1U
#define kRoadChunkSpan 3U
// water clusters tighter than road: three logs a mover is twice the sprite cost
#define kWaterChunkMin 1U
#define kWaterChunkSpan 2U
// two grass lanes minimum keeps the visible danger lanes inside the oam budget
#define kGrassChunkMin 2U
#define kGrassChunkSpan 2U
// the guaranteed-open column wanders at most two columns and never hugs an edge
#define kGapWanderSpan 5U // rng % 5 gives -2..+2 after the bias
#define kGapWanderBias 2
#define kGapMin 1
#define kGapMax 8

// lcg full period mod 256: multiplier is 1 mod 4 and the increment is odd
#define kRngMul 37U
#define kRngAdd 1U
// bit 7 runs the lcg's full period; bit 0 merely alternates
#define kRngTopBit 0x80U

// hud: three digit sprites top-center, oam coords offset by 8,16 from the screen
#define kChickSprite 0U
#define kHudFirstSprite 1U
#define kHudDigits 3U
#define kHudOamY 24U       // screen y 8
#define kHudCenterOamX 88U // screen x 80; the run is centered by half a digit per digit
#define kDigitWidthPx 8U
#define kScoreMax 999U // three digit sprites is all the hud can show

// movers: cars and logs ride one 256 px wrapping track per lane, y centered in the lane
#define kMoverLaneInset 4U
// two movers share a lane's speed half a lap apart, so their gap never closes
#define kMoversPerLane 2U
#define kMoverPhase 0x8000U
// the track doubles as oam x, so a mover slides off either edge before it wraps
#define kMoverDrawLimit 176U
// worst case in nine visible lanes is five water lanes: water chunks cap at 2, grass at 2
#define kMoverFirstSprite 4U
#define kMoverSprites 30U
// 34 of 40 oam, and a water lane's 6 log parts plus 3 hud digits plus the chick is the dmg's 10
// so the hud stays at screen y 8, where only the top lane's movers ever share its scanlines

// cars: 16 px of two 8x8 sprites in tile id order
#define kCarTileId 0xC0U
#define kCarTileCount 2U
#define kCarSprites 2U
#define kCarHalfPx 8U
// 8.8 px per frame: 0.5 to 1.0 in five steps
#define kCarSpeedMin 128U
#define kCarSpeedStep 32U
#define kCarSpeedSteps 5U
// centers closer than this collide: 8 px of car plus 4 px of chick, minus a little mercy
#define kCarHitPx 10

// logs: 24 px of three 8x8 sprites, all the same tile
#define kLogTileId 0xC4U
#define kLogTileCount 1U
#define kLogSprites 3U
#define kLogHalfPx 12U
// the lane's lower half: dmg's lower oam x wins, so a log level with the chick would bury its rider
#define kLogLaneInset 8U
// 8.8 px per frame: 0.41 to 0.78 in four steps, every one a whole 32nd of a pixel
#define kLogSpeedMin 104U
#define kLogSpeedStep 32U
#define kLogSpeedSteps 4U
// the chick rides while its center is within 12 px of a log's
#define kLogRidePx 12
// 8.8 left x 152, so the chick's center reaches 156: the last px a ride survives
#define kRideMaxFixed 0x9800U

// game over popup: a band of bg cells over the frozen world, no window layer involved
#define kPopupTopRow 5U // rows 5..11 of 18 center the four text lines
#define kPopupRows 7U
// scx is always zero here, so the band is exactly the visible columns wide
#define kPopupCols 20U
#define kPopupRowsPerFrame 2U // 40 cells is a comfortable vblank budget
#define kPopupOverRow 1U
#define kPopupScoreRow 2U
#define kPopupBestRow 3U
#define kPopupPromptRow 5U
// dead input after a death so a panic hop cannot skip the popup
#define kOverLockoutFrames 20U

// battery sram, mbc1 bank 0: 4 magic bytes then the best score little endian
#define kSramBase 0xA000U
#define kSaveMagic0 'C'
#define kSaveMagic1 'R'
#define kSaveMagic2 'S'
#define kSaveMagic3 'Y'
#define kSaveBestOffset 4U

#endif
