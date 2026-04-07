#include "../include/keyboard.h"
#include "../include/kernel.h"

volatile char last_key = 0;
int shift_pressed = 0;
int caps_lock = 0;

const char scancode_lower[] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0, ' '
};

const char scancode_upper[] = {
    0, 27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*', 0, ' '
};

void keyboard_handler_c() {
    uint8_t scancode = inb(0x60);
    
    if (scancode == 0x2A || scancode == 0x36) {
        shift_pressed = 1;
    } else if (scancode == 0xAA || scancode == 0xB6) {
        shift_pressed = 0;
    } else if (scancode == 0x3A) {
        caps_lock = !caps_lock;
    } else if (!(scancode & 0x80)) {
        if (scancode < sizeof(scancode_lower)) {
            char c = scancode_lower[scancode];
            if (c >= 'a' && c <= 'z') {
                if (shift_pressed ^ caps_lock) c -= 32;
            } else if (shift_pressed) {
                c = scancode_upper[scancode];
            }
            last_key = c;
        }
    }
}