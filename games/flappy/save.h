#ifndef SAVE_H
#define SAVE_H

#include <stdint.h>

void save_init(void);
uint16_t save_best(void);
void save_record(uint16_t score);

#endif
