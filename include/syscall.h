#ifndef SYSCALL_H
#define SYSCALL_H
#include <stdint.h>
#include <stddef.h>
void sys_draw_rect_alpha(int x, int y, int w, int h, uint32_t color, uint8_t alpha);
void sys_draw_rect_outline(int x, int y, int w, int h, uint32_t color, int thickness);
void sys_draw_string(int x, int y, const char* str, uint32_t color);
void sys_update_screen();
void sys_clear_backbuffer(uint32_t color);
int sys_get_mouse_x();
int sys_get_mouse_y();
int sys_get_mouse_btn();
void* sys_malloc(size_t size);
void sys_free(void* ptr);
char sys_get_key();
size_t sys_get_used_memory();
int sys_get_num_tasks();
void sys_draw_circle_alpha(int xc, int yc, int r, uint32_t color, uint8_t alpha);
#endif