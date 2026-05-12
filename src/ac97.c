#include "../include/ac97.h"
#include "../include/pci.h"
#include "../include/kernel.h"
#include "../include/memory.h"

uint32_t ac97_nam_bar = 0;
uint32_t ac97_nabm_bar = 0;

typedef struct {
    uint32_t buffer_addr;
    uint16_t length;
    uint16_t flags;
} __attribute__((packed)) ac97_bdl_entry_t;

ac97_bdl_entry_t* bdl;

static inline void outw_ac97(uint16_t port, uint16_t val) {
    __asm__ volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}

static inline void outl_ac97(uint16_t port, uint32_t val) {
    __asm__ volatile ("outl %0, %1" : : "a"(val), "Nd"(port));
}

void ac97_init() {
    uint8_t bus, slot, func;
    if (pci_find_device(0x8086, 0x2415, &bus, &slot, &func)) {
        ac97_nam_bar = pci_read_bar(bus, slot, func, 0) & ~1;
        ac97_nabm_bar = pci_read_bar(bus, slot, func, 1) & ~1;
        pci_enable_bus_mastering(bus, slot, func);

        outb(ac97_nabm_bar + 0x2C, 2);
        for (int i = 0; i < 10000; i++) io_wait();
        outb(ac97_nabm_bar + 0x2C, 1);
        for (int i = 0; i < 10000; i++) io_wait();
        outb(ac97_nabm_bar + 0x2C, 0);

        outw_ac97(ac97_nam_bar + 0x02, 0x0808);
        outw_ac97(ac97_nam_bar + 0x18, 0x0808);

        bdl = (ac97_bdl_entry_t*)kmalloc(32 * sizeof(ac97_bdl_entry_t));
        outl_ac97(ac97_nabm_bar + 0x10, (uint32_t)(uint64_t)bdl);
    }
}

void ac97_play(uint8_t* buffer, uint32_t length) {
    if (!ac97_nam_bar) return;
    
    outb(ac97_nabm_bar + 0x1B, 0);

    uint32_t samples = length / 2;
    uint32_t offset = 0;
    int idx = 0;

    while (samples > 0 && idx < 32) {
        uint32_t chunk = (samples > 65534) ? 65534 : samples;
        bdl[idx].buffer_addr = (uint32_t)(uint64_t)(buffer + offset);
        bdl[idx].length = chunk;
        bdl[idx].flags = 0;
        
        samples -= chunk;
        offset += chunk * 2;
        idx++;
    }

    if (idx > 0) {
        bdl[idx - 1].flags = 0x8000;
        outb(ac97_nabm_bar + 0x15, idx - 1);
        outb(ac97_nabm_bar + 0x1B, 1);
    }
}

void ac97_stop() {
    if (!ac97_nam_bar) return;
    outb(ac97_nabm_bar + 0x1B, 0);
}