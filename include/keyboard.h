#ifndef KEYBOARD_H
#define KEYBOARD_H
#include <stdint.h>

void keyboard_init();
extern volatile char last_key;

#endif