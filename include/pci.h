#ifndef PCI_H
#define PCI_H
#include <stdint.h>

void pci_enumerate();
int pci_get_device(int index, uint16_t* vendor, uint16_t* device, uint8_t* class_c);
int pci_find_device(uint16_t vendor, uint16_t device, uint8_t* bus, uint8_t* slot, uint8_t* func);
uint32_t pci_read_bar(uint8_t bus, uint8_t slot, uint8_t func, uint8_t bar_num);
void pci_enable_bus_mastering(uint8_t bus, uint8_t slot, uint8_t func);

#endif