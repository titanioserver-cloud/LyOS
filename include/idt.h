#ifndef IDT_H
#define IDT_H

#include <stdint.h>

struct idt_entry_64 {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t ist;
    uint8_t type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t zero;
} __attribute__((packed));

struct idt_ptr_64 {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

void idt_init();
void idt_set_gate(uint8_t num, uint64_t base, uint16_t sel, uint8_t flags);
extern void irq0_handler();
extern void irq1_handler();
extern void irq12_handler();
extern void isr128();

#endif