#include "../include/graphics.h"
#include "../include/font.h"
#include "../include/mouse.h"

uint32_t* front_buffer;
uint32_t back_buffer[1280 * 720];
Rect dirty_rects[300];
int dirty_count = 0;
int screen_w, screen_h;

static inline uint8_t inb_vga(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static void wait_vsync() {
    while (inb_vga(0x3DA) & 8);
    while (!(inb_vga(0x3DA) & 8));
}

void init_graphics(uint32_t* fb, int w, int h) {
    front_buffer = fb;
    screen_w = w;
    screen_h = h;
}

void add_dirty_rect(int x, int y, int w, int h) {
    if (w <= 0 || h <= 0) return;
    int ax1 = x; int ay1 = y;
    int ax2 = x + w; int ay2 = y + h;

    for (int i = 0; i < dirty_count; i++) {
        int bx1 = dirty_rects[i].x; int by1 = dirty_rects[i].y;
        int bx2 = dirty_rects[i].x + dirty_rects[i].w; int by2 = dirty_rects[i].y + dirty_rects[i].h;

        if (ax1 <= bx2 && ax2 >= bx1 && ay1 <= by2 && ay2 >= by1) {
            int nx1 = (ax1 < bx1) ? ax1 : bx1; int ny1 = (ay1 < by1) ? ay1 : by1;
            int nx2 = (ax2 > bx2) ? ax2 : bx2; int ny2 = (ay2 > by2) ? ay2 : by2;
            dirty_rects[i].x = nx1; dirty_rects[i].y = ny1;
            dirty_rects[i].w = nx2 - nx1; dirty_rects[i].h = ny2 - ny1;
            return;
        }
    }

    if (dirty_count < 300) { dirty_rects[dirty_count++] = (Rect){x, y, w, h}; } 
    else {
        int bx1 = dirty_rects[0].x; int by1 = dirty_rects[0].y;
        int bx2 = dirty_rects[0].x + dirty_rects[0].w; int by2 = dirty_rects[0].y + dirty_rects[0].h;
        int nx1 = (ax1 < bx1) ? ax1 : bx1; int ny1 = (ay1 < by1) ? ay1 : by1;
        int nx2 = (ax2 > bx2) ? ax2 : bx2; int ny2 = (ay2 > by2) ? ay2 : by2;
        dirty_rects[0].x = nx1; dirty_rects[0].y = ny1;
        dirty_rects[0].w = nx2 - nx1; dirty_rects[0].h = ny2 - ny1;
    }
}

void draw_buffer_fast(int x, int y, int w, int h, uint32_t* buffer) {
    if (x < 0 || y < 0 || x + w > screen_w || y + h > screen_h) return;
    for (int i = 0; i < h; i++) {
        uint32_t* dst = &back_buffer[(y + i) * screen_w + x];
        uint32_t* src = &buffer[i * w];
        int count = w;
        while (count >= 8) {
            __asm__ volatile ("vmovups (%0), %%ymm0\n" "vmovups %%ymm0, (%1)\n" : : "r"(src), "r"(dst) : "ymm0", "memory");
            src += 8; dst += 8; count -= 8;
        }
        while (count > 0) { *dst++ = *src++; count--; }
    }
    add_dirty_rect(x, y, w, h);
}

void draw_rect_alpha(int x, int y, int w, int h, uint32_t color, uint8_t alpha) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > screen_w) w = screen_w - x;
    if (y + h > screen_h) h = screen_h - y;
    if (w <= 0 || h <= 0) return;
    
    if (alpha == 255) {
        __asm__ volatile (
            "vmovd %0, %%xmm0\n"
            "vpshufd $0, %%xmm0, %%xmm0\n"
            "vinsertf128 $1, %%xmm0, %%ymm0, %%ymm0\n"
            : : "r"(color) : "xmm0", "ymm0"
        );
        for (int i = y; i < y + h; i++) {
            uint32_t* dst = &back_buffer[i * screen_w + x];
            int count = w;
            while (count >= 8) {
                __asm__ volatile ("vmovups %%ymm0, (%0)\n" : : "r"(dst) : "memory");
                dst += 8; count -= 8;
            }
            while (count > 0) { *dst++ = color; count--; }
        }
    } else {
        uint32_t rb = color & 0xFF00FF; uint32_t g = color & 0x00FF00;
        for (int i = y; i < y + h; i++) {
            uint32_t* dst = &back_buffer[i * screen_w + x];
            for (int j = 0; j < w; j++) {
                uint32_t d_rb = dst[j] & 0xFF00FF; uint32_t d_g = dst[j] & 0x00FF00;
                uint32_t res_rb = (d_rb + (((rb - d_rb) * alpha) >> 8)) & 0xFF00FF;
                uint32_t res_g = (d_g + (((g - d_g) * alpha) >> 8)) & 0x00FF00;
                dst[j] = res_rb | res_g;
            }
        }
    }
    add_dirty_rect(x, y, w, h);
}

void draw_rect_outline(int x, int y, int w, int h, uint32_t color, int thickness) {
    draw_rect_alpha(x, y, w, thickness, color, 255);
    draw_rect_alpha(x, y + h - thickness, w, thickness, color, 255);
    draw_rect_alpha(x, y, thickness, h, color, 255);
    draw_rect_alpha(x + w - thickness, y, thickness, h, color, 255);
}

void draw_circle_alpha(int xc, int yc, int r, uint32_t color, uint8_t alpha) {
    int x = 0, y = r; int d = 3 - 2 * r;
    while (y >= x) {
        draw_rect_alpha(xc - x, yc - y, 2 * x, 1, color, alpha); draw_rect_alpha(xc - x, yc + y, 2 * x, 1, color, alpha);
        draw_rect_alpha(xc - y, yc - x, 2 * y, 1, color, alpha); draw_rect_alpha(xc - y, yc + x, 2 * y, 1, color, alpha);
        x++; if (d > 0) { y--; d = d + 4 * (x - y) + 10; } else { d = d + 4 * x + 6; }
    }
}

void draw_char(int x, int y, char c, uint32_t color) {
    if (c < 32 || c > 126) c = 32;
    const uint16_t* bitmap = font_16x16[c - 32];
    for (int i = 0; i < 16; i++) {
        uint16_t row = bitmap[i];
        for (int j = 0; j < 16; j++) {
            if (row & (1 << (15 - j))) {
                int px = x + j; int py = y + i;
                if (px >= 0 && px < screen_w && py >= 0 && py < screen_h) back_buffer[py * screen_w + px] = color;
            }
        }
    }
    add_dirty_rect(x, y, 16, 16);
}

void draw_string(int x, int y, const char* str, uint32_t color) {
    while (*str) {
        if (*str == '\n') { y += 22; x = x; } else { draw_char(x, y, *str, color); x += 10; }
        str++;
    }
}

void draw_rect_in_buffer(uint32_t* buf, int buf_w, int x, int y, int w, int h, uint32_t color) {
    for (int i = 0; i < h; i++) { for (int j = 0; j < w; j++) { buf[(y + i) * buf_w + (x + j)] = color; } }
}

void draw_string_in_buffer(uint32_t* buf, int buf_w, int x, int y, const char* str, uint32_t color) {
    int cur_x = x;
    while (*str) {
        if (*str == '\n') { y += 22; cur_x = x; } else {
            const uint16_t* bitmap = font_16x16[*str - 32];
            for (int i = 0; i < 16; i++) {
                uint16_t row = bitmap[i];
                for (int j = 0; j < 16; j++) { if (row & (1 << (15 - j))) buf[(y + i) * buf_w + (cur_x + j)] = color; }
            }
            cur_x += 10;
        }
        str++;
    }
}

void update_screen() {
    int mx = mouse_x;
    int my = mouse_y;
    static int old_mx = -1, old_my = -1;
    int mouse_moved = (mx != old_mx || my != old_my);

    if (dirty_count == 0 && !mouse_moved) return;
    
    // OTIMIZAÇÃO CRUCIAL: Só espera o V-Sync se formos redesenhar elementos grandes (janelas). 
    // O cursor sozinho desenha-se instantaneamente sem esperar.
    if (dirty_count > 0) {
        wait_vsync();
    }
    
    for (int r = 0; r < dirty_count; r++) {
        Rect rect = dirty_rects[r];
        if (rect.x < 0) { rect.w += rect.x; rect.x = 0; }
        if (rect.y < 0) { rect.h += rect.y; rect.y = 0; }
        if (rect.x + rect.w > screen_w) rect.w = screen_w - rect.x;
        if (rect.y + rect.h > screen_h) rect.h = screen_h - rect.y;

        for (int i = rect.y; i < rect.y + rect.h; i++) {
            if (i < 0 || i >= screen_h) continue;
            uint32_t* src = &back_buffer[i * screen_w + rect.x];
            uint32_t* dst = &front_buffer[i * screen_w + rect.x];
            int count = rect.w;
            while (count >= 8) {
                __asm__ volatile ("vmovups (%0), %%ymm0\n" "vmovups %%ymm0, (%1)\n" : : "r"(src), "r"(dst) : "ymm0", "memory");
                src += 8; dst += 8; count -= 8;
            }
            while (count > 0) { *dst++ = *src++; count--; }
        }
    }

    if (old_mx >= 0) {
        for (int i = 0; i < 15; i++) {
            if (old_my + i >= screen_h) continue;
            for (int j = 0; j < 15; j++) {
                if (old_mx + j >= screen_w) continue;
                front_buffer[(old_my + i) * screen_w + (old_mx + j)] = back_buffer[(old_my + i) * screen_w + (old_mx + j)];
            }
        }
    }

    for (int i = 0; i < 12; i++) {
        if (my + i >= screen_h) continue;
        for (int j = 0; j < 12; j++) {
            if (mx + j >= screen_w) continue;
            uint32_t color = 0xCDD6F4;
            if (i == 0 || j == 0 || i == 11 || j == 11) color = 0x11111B;
            front_buffer[(my + i) * screen_w + (mx + j)] = color;
        }
    }

    old_mx = mx;
    old_my = my;
    dirty_count = 0;
}

void clear_backbuffer(uint32_t color) {
    uint32_t* ptr = back_buffer;
    int count = screen_w * screen_h;
    __asm__ volatile (
        "vmovd %0, %%xmm0\n"
        "vpshufd $0, %%xmm0, %%xmm0\n"
        "vinsertf128 $1, %%xmm0, %%ymm0, %%ymm0\n"
        : : "r"(color) : "xmm0", "ymm0"
    );
    while (count >= 8) {
        __asm__ volatile ("vmovups %%ymm0, (%0)\n" : : "r"(ptr) : "memory");
        ptr += 8; count -= 8;
    }
    while (count > 0) { *ptr++ = color; count--; }
    add_dirty_rect(0, 0, screen_w, screen_h);
}