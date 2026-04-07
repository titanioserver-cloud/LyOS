#include "../include/gdt.h"

struct gdt_entry gdt[7];
struct gdt_ptr gdtp;
struct tss_entry tss;

void gdt_set_gate(int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt[num].base_low = (base & 0xFFFF);
    gdt[num].base_middle = (base >> 16) & 0xFF;
    gdt[num].base_high = (base >> 24) & 0xFF;
    gdt[num].limit_low = (limit & 0xFFFF);
    gdt[num].granularity = (limit >> 16) & 0x0F;
    gdt[num].granularity |= gran & 0xF0;
    gdt[num].access = access;
}

void write_tss(int num, uint16_t ss0, uint64_t esp0) {
    uint64_t base = (uint64_t)&tss;
    uint32_t limit = sizeof(tss);

    gdt_set_gate(num, base, limit, 0x89, 0x00);
    gdt[num + 1].limit_low = (base >> 32) & 0xFFFF;
    gdt[num + 1].base_low = (base >> 48) & 0xFFFF;

    for(int i=0; i<sizeof(tss); i++) ((uint8_t*)&tss)[i] = 0;

    tss.rsp0 = esp0;
    tss.iopb_offset = sizeof(tss);
}

void update_tss_rsp0(uint64_t rsp0) {
    tss.rsp0 = rsp0;
}

void gdt_init() {
    gdtp.limit = (sizeof(struct gdt_entry) * 7) - 1;
    gdtp.base = (uint64_t)&gdt;

    gdt_set_gate(0, 0, 0, 0, 0);
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xAF);
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF);
    gdt_set_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xAF);
    gdt_set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF);

    static uint8_t initial_kernel_stack[8192] __attribute__((aligned(16)));
    write_tss(5, 0x10, (uint64_t)&initial_kernel_stack[8192]);

    __asm__ volatile("lgdt %0" : : "m"(gdtp));
    __asm__ volatile("ltr %%ax" : : "a"(0x28));
}