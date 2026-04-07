#include "../include/idt.h"
#include "../include/kernel.h"

struct idt_entry_64 idt[256];
struct idt_ptr_64 idtp;

void idt_set_gate(uint8_t num, uint64_t base, uint16_t sel, uint8_t flags) {
    idt[num].offset_low = base & 0xFFFF;
    idt[num].selector = sel;
    idt[num].ist = 0;
    idt[num].type_attr = flags;
    idt[num].offset_mid = (base >> 16) & 0xFFFF;
    idt[num].offset_high = (base >> 32) & 0xFFFFFFFF;
    idt[num].zero = 0;
}

void pic_remap() {
    uint8_t a1 = inb(0x21);
    uint8_t a2 = inb(0xA1);
    outb(0x20, 0x11); io_wait();
    outb(0xA0, 0x11); io_wait();
    outb(0x21, 0x20); io_wait();
    outb(0xA1, 0x28); io_wait();
    outb(0x21, 0x04); io_wait();
    outb(0xA1, 0x02); io_wait();
    outb(0x21, 0x01); io_wait();
    outb(0xA1, 0x01); io_wait();
    outb(0x21, a1);
    outb(0xA1, a2);
}

void idt_init() {
    idtp.limit = (sizeof(struct idt_entry_64) * 256) - 1;
    idtp.base = (uint64_t)&idt;

    pic_remap();

    idt_set_gate(32, (uint64_t)irq0_handler, 0x08, 0x8E);
    idt_set_gate(33, (uint64_t)irq1_handler, 0x08, 0x8E);
    idt_set_gate(44, (uint64_t)irq12_handler, 0x08, 0x8E);
    idt_set_gate(128, (uint64_t)isr128, 0x08, 0xEE);

    outb(0x21, 0xF8);
    outb(0xA1, 0xEF);

    __asm__ volatile("lidt %0" : : "m" (idtp));
}