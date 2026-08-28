#ifndef CROSSY_H
#define CROSSY_H

// tuning constants live here so logic files never carry magic numbers

// identity bgp; gbdk's font clears to index 0, so a blank screen is white
#define kTitleBgp 0xE4U

// the visible screen is 20x18 cells; text lines are centered across the 20
#define kScreenCols 20U
#define kTitleTextY 6U
#define kPromptTextY 10U
#define kBestTextY 12U

#endif
