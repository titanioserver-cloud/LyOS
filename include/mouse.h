#ifndef MOUSE_H
#define MOUSE_H

#include <stdint.h>

extern int mouse_x;
extern int mouse_y;
extern uint8_t mouse_byte[3];

void mouse_init();
void mouse_handler_c();

#endif