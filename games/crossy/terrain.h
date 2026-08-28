#ifndef TERRAIN_H
#define TERRAIN_H

#include <stdint.h>

// fills the 32 row ring with grass, generates the first visible lanes, parks the camera on lane 0
void terrain_init(uint8_t seed);

// rewrites one cached lane's two rows, undoing anything drawn over them; vblank only
void terrain_redraw_lane(uint16_t lane);

// 1 when a tree stands on that cell; only lanes still held by the ring may be asked
uint8_t terrain_blocked(uint16_t lane, uint8_t col);

// 1 when that lane is asphalt: no trees, but traffic
uint8_t terrain_is_road(uint16_t lane);

// 1 when that lane is open water: no trees, and nothing to stand on but a log
uint8_t terrain_is_water(uint16_t lane);

// 1 when that lane is a train track: clear ground between sweeps, lethal during one
uint8_t terrain_is_track(uint16_t lane);

// screen x of the sweeping train's left edge, or kTrainOffX when that lane has no train out
int16_t terrain_train_x(uint16_t lane);

// 1 when the train of that track lane overlaps a chick centered on center_x
uint8_t terrain_train_hit(uint16_t lane, uint8_t center_x);

// one frame of every visible track lane's phase machine; reads 1 when a warning bell is due
// writes the blinking warning cell, so it must be called inside vblank
uint8_t terrain_tick_tracks(void);

// screen y of the lane's top row, taken from scy so a hop's slide is included
uint8_t terrain_lane_screen_y(uint16_t lane);

uint16_t terrain_cam_lane(void);

// counts one play frame and reads 1 once a whole creep interval has passed without an advance
uint8_t terrain_creep_due(void);

// moves the camera one lane forward and streams the single lane that just came into range
void terrain_advance(void);

// scy for the current camera lane, offset by the hop's remaining slide in px
void terrain_apply_scy(uint8_t slide_px);

#endif
