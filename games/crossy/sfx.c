#include "sfx.h"

#include <gb/gb.h>

// apu: master on, every channel to both ears, both ears at max volume
#define kNr52On 0x80U
#define kNr51All 0xFFU
#define kNr50Max 0x77U

// hop: ch1, no sweep, quiet and short so constant hopping never grates
// envelope: volume 6, period 1 -> 6/64s decay, ~0.1s
#define kHopSweep 0x00U
#define kHopDuty 0x80U
#define kHopEnvelope 0x61U
#define kHopFreqLo 0x80U
#define kHopFreqHi 0x86U // trigger plus freq 0x680: 341 hz, above the flap-style base tone

// score: ch2, bright and short, an octave-ish above the hop
#define kScoreDuty 0x80U
#define kScoreEnvelope 0xF1U
#define kScoreFreqLo 0x00U
#define kScoreFreqHi 0x87U // trigger plus freq 0x700: 512 hz

// bell: ch2, thin duty for a chime timbre, one octave above score
// caller rings this twice for the train's two-ding warning
#define kBellDuty 0x40U
#define kBellEnvelope 0xF1U
#define kBellFreqLo 0x80U
#define kBellFreqHi 0x87U // trigger plus freq 0x780: 1024 hz

// splash: ch4 noise, soft and low, decays longer than a hit so the drown lingers
// envelope: volume 10, period 5 -> 50/64s decay, ~0.78s
#define kSplashLength 0x00U
#define kSplashEnvelope 0xA5U
#define kSplashPoly 0x65U // low shift clock (s=6) gives a deep rumble, not a crash
#define kSplashTrigger 0x80U

// hit: ch4 noise, 15 bit lfsr, sharp and loud, rings out on the crash
#define kHitLength 0x00U
#define kHitEnvelope 0xF3U
#define kHitPoly 0x35U
#define kHitTrigger 0x80U

// pandocs: nr52 must be powered on before any other sound register keeps a write
void sfx_init(void) {
    NR52_REG = kNr52On;
    NR51_REG = kNr51All;
    NR50_REG = kNr50Max;
}

void sfx_hop(void) {
    NR10_REG = kHopSweep;
    NR11_REG = kHopDuty;
    NR12_REG = kHopEnvelope;
    NR13_REG = kHopFreqLo;
    NR14_REG = kHopFreqHi;
}

void sfx_splash(void) {
    NR41_REG = kSplashLength;
    NR42_REG = kSplashEnvelope;
    NR43_REG = kSplashPoly;
    NR44_REG = kSplashTrigger;
}

void sfx_hit(void) {
    NR41_REG = kHitLength;
    NR42_REG = kHitEnvelope;
    NR43_REG = kHitPoly;
    NR44_REG = kHitTrigger;
}

void sfx_score(void) {
    NR21_REG = kScoreDuty;
    NR22_REG = kScoreEnvelope;
    NR23_REG = kScoreFreqLo;
    NR24_REG = kScoreFreqHi;
}

void sfx_bell(void) {
    NR21_REG = kBellDuty;
    NR22_REG = kBellEnvelope;
    NR23_REG = kBellFreqLo;
    NR24_REG = kBellFreqHi;
}
