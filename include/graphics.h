#ifndef GRAPHICS_H
#define GRAPHICS_H
#include <stdint.h>
typedef struct { int x, y, w, h; } Rect;
void init_graphics(uint32_t* fb, int w, int h);
void draw_rect_alpha(int x, int y, int w, int h, uint32_t color, uint8_t alpha);
void draw_rect_outline(int x, int y, int w, int h, uint32_t color, int thickness);
void draw_circle_alpha(int xc, int yc, int r, uint32_t color, uint8_t alpha);
void add_dirty_rect(int x, int y, int w, int h);
void update_screen();
void clear_backbuffer(uint32_t color);
void draw_char(int x, int y, char c, uint32_t color);
void draw_string(int x, int y, const char* str, uint32_t color);
#endif