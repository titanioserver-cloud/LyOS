#ifndef AC97_H
#define AC97_H

#include <stdint.h>

void ac97_init();
void ac97_play(uint8_t* buffer, uint32_t length);
void ac97_stop();

#endif