#ifndef PAGING_H
#define PAGING_H
#include <stdint.h>

void init_paging(uint64_t fb_addr);
void map_page(uint64_t pml4_addr, uint64_t vaddr, uint64_t paddr, int is_user);
void map_mmio(uint64_t paddr, uint32_t size);
uint64_t create_address_space();
void switch_address_space(uint64_t pml4);
void map_user_memory(uint64_t pml4_addr, uint64_t vaddr, uint32_t size);
void* alloc_page();

#endif