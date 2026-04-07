#include "../include/pci.h"

typedef struct {
    uint8_t bus, slot, func;
    uint16_t vendor, device;
    uint8_t class_code, subclass;
} PCIDevice;

PCIDevice pci_devices[32];
int pci_device_count = 0;
int pci_scanned = 0;

static inline void outl_pci(uint16_t port, uint32_t val) {
    __asm__ volatile ("outl %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint32_t inl_pci(uint16_t port) {
    uint32_t ret;
    __asm__ volatile ("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

uint32_t pci_read_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address = (uint32_t)(((uint32_t)bus << 16) | ((uint32_t)slot << 11) | 
                       ((uint32_t)func << 8) | (offset & 0xFC) | 0x80000000);
    outl_pci(0xCF8, address);
    return inl_pci(0xCFC);
}

uint16_t pci_read_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t dword = pci_read_dword(bus, slot, func, offset);
    return (uint16_t)((dword >> ((offset & 2) * 8)) & 0xFFFF);
}

void pci_check_device(uint8_t bus, uint8_t slot, uint8_t func) {
    uint16_t vendor = pci_read_word(bus, slot, func, 0);
    if (vendor == 0xFFFF) return;

    uint16_t device = pci_read_word(bus, slot, func, 2);
    uint16_t class_reg = pci_read_word(bus, slot, func, 0x0A);
    uint8_t class_code = (class_reg >> 8) & 0xFF;
    uint8_t subclass = class_reg & 0xFF;

    if (pci_device_count < 32) {
        pci_devices[pci_device_count].bus = bus;
        pci_devices[pci_device_count].slot = slot;
        pci_devices[pci_device_count].func = func;
        pci_devices[pci_device_count].vendor = vendor;
        pci_devices[pci_device_count].device = device;
        pci_devices[pci_device_count].class_code = class_code;
        pci_devices[pci_device_count].subclass = subclass;
        pci_device_count++;
    }
}

void pci_enumerate() {
    pci_device_count = 0;
    for (int bus = 0; bus < 8; bus++) {
        for (int slot = 0; slot < 32; slot++) {
            uint16_t vendor = pci_read_word(bus, slot, 0, 0);
            if (vendor != 0xFFFF) {
                pci_check_device(bus, slot, 0);
                uint8_t header_type = pci_read_word(bus, slot, 0, 0x0E) & 0xFF;
                if (header_type & 0x80) {
                    for (int func = 1; func < 8; func++) {
                        if (pci_read_word(bus, slot, func, 0) != 0xFFFF) {
                            pci_check_device(bus, slot, func);
                        }
                    }
                }
            }
        }
    }
    pci_scanned = 1;
}

int pci_get_device(int index, uint16_t* vendor, uint16_t* device, uint8_t* class_c) {
    if (!pci_scanned) pci_enumerate(); 
    if (index == -1) return pci_device_count;
    if (index < 0 || index >= pci_device_count) return -1;
    if (vendor) *vendor = pci_devices[index].vendor;
    if (device) *device = pci_devices[index].device;
    if (class_c) *class_c = pci_devices[index].class_code;
    return 0;
}

int pci_find_device(uint16_t vendor, uint16_t device, uint8_t* bus, uint8_t* slot, uint8_t* func) {
    if (!pci_scanned) pci_enumerate();
    for(int i = 0; i < pci_device_count; i++) {
        if(pci_devices[i].vendor == vendor && pci_devices[i].device == device) {
            if(bus) *bus = pci_devices[i].bus;
            if(slot) *slot = pci_devices[i].slot;
            if(func) *func = pci_devices[i].func;
            return 1;
        }
    }
    return 0;
}

uint32_t pci_read_bar(uint8_t bus, uint8_t slot, uint8_t func, uint8_t bar_num) {
    uint8_t offset = 0x10 + (bar_num * 4);
    return pci_read_dword(bus, slot, func, offset);
}

void pci_enable_bus_mastering(uint8_t bus, uint8_t slot, uint8_t func) {
    uint16_t cmd = pci_read_word(bus, slot, func, 0x04);
    uint32_t address = (uint32_t)(((uint32_t)bus << 16) | ((uint32_t)slot << 11) | ((uint32_t)func << 8) | 0x04 | 0x80000000);
    outl_pci(0xCF8, address);
    outl_pci(0xCFC, (uint32_t)(cmd | 0x0004));
}