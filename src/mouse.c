#include "../include/mouse.h"
#include "../include/kernel.h"

int mouse_x = 640;
int mouse_y = 360;
uint8_t mouse_byte[3];
int mouse_cycle = 0;

void mouse_wait(uint8_t a_type) {
    uint32_t timeout = 100000;
    if (a_type == 0) {
        while (timeout--) {
            if ((inb(0x64) & 1) == 1) return;
        }
    } else {
        while (timeout--) {
            if ((inb(0x64) & 2) == 0) return;
        }
    }
}

void mouse_write(uint8_t write) {
    mouse_wait(1);
    outb(0x64, 0xD4);
    mouse_wait(1);
    outb(0x60, write);
}

uint8_t mouse_read() {
    mouse_wait(0);
    return inb(0x60);
}

void mouse_init() {
    uint8_t status;
    mouse_wait(1);
    outb(0x64, 0xA8);
    mouse_wait(1);
    outb(0x64, 0x20);
    mouse_wait(0);
    status = (inb(0x60) | 2);
    mouse_wait(1);
    outb(0x64, 0x60);
    mouse_wait(1);
    outb(0x60, status);
    mouse_write(0xF6);
    mouse_read();
    mouse_write(0xF4);
    mouse_read();
}

void mouse_handler_c() {
    uint8_t status = inb(0x64);
    if (!(status & 0x20)) return;

    mouse_byte[mouse_cycle++] = inb(0x60);

    if (mouse_cycle == 3) {
        mouse_cycle = 0;

        if ((mouse_byte[0] & 0x80) || (mouse_byte[0] & 0x40)) return;

        int delta_x = mouse_byte[1];
        int delta_y = mouse_byte[2];

        if (mouse_byte[0] & 0x10) delta_x |= 0xFFFFFF00;
        if (mouse_byte[0] & 0x20) delta_y |= 0xFFFFFF00;

        mouse_x += delta_x;
        mouse_y -= delta_y;

        if (mouse_x < 0) mouse_x = 0;
        if (mouse_y < 0) mouse_y = 0;
        if (mouse_x > 1279) mouse_x = 1279;
        if (mouse_y > 719) mouse_y = 719;
    }
}