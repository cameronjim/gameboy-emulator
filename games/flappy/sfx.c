#include "sfx.h"

#include "flappy.h"

#include <gb/gb.h>

// pandocs: nr52 must be powered on before any other sound register keeps a write
void sfx_init(void) {
    NR52_REG = kNr52On;
    NR51_REG = kNr51All;
    NR50_REG = kNr50Max;
}

void sfx_flap(void) {
    NR10_REG = kFlapSweep;
    NR11_REG = kFlapDuty;
    NR12_REG = kFlapEnvelope;
    NR13_REG = kFlapFreqLo;
    NR14_REG = kFlapFreqHi;
}

void sfx_score(void) {
    NR21_REG = kScoreDuty;
    NR22_REG = kScoreEnvelope;
    NR23_REG = kScoreFreqLo;
    NR24_REG = kScoreFreqHi;
}

void sfx_hit(void) {
    NR41_REG = kHitLength;
    NR42_REG = kHitEnvelope;
    NR43_REG = kHitPoly;
    NR44_REG = kHitTrigger;
}
