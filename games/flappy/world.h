#ifndef WORLD_H
#define WORLD_H

#include <stdint.h>

void world_init(void);
void world_scroll(void);
uint8_t world_kills(uint8_t bird_top_px);

#endif
