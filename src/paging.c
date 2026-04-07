#include "../include/paging.h"
#include "../include/memory.h"

#define PAGE_SIZE 4096
#define PAGE_PRESENT 0x1
#define PAGE_WRITE 0x2
#define PAGE_USER 0x4

#define MAX_PAGES 4096
static uint8_t pt_mem[MAX_PAGES * PAGE_SIZE] __attribute__((aligned(PAGE_SIZE)));
static int pt_idx_count = 0;

static uint64_t* kernel_pml4;

void* alloc_page() {
    if (pt_idx_count >= MAX_PAGES) return 0;
    void* page = &pt_mem[pt_idx_count * PAGE_SIZE];
    pt_idx_count++;
    uint8_t* p = (uint8_t*)page;
    for(int i=0; i<PAGE_SIZE; i++) p[i] = 0;
    return page;
}

void map_page(uint64_t pml4_addr, uint64_t vaddr, uint64_t paddr, int is_user) {
    uint64_t* pml4 = (uint64_t*)pml4_addr;
    uint16_t pml4_idx = (vaddr >> 39) & 0x1FF;
    uint16_t pdpt_idx = (vaddr >> 30) & 0x1FF;
    uint16_t pd_idx   = (vaddr >> 21) & 0x1FF;
    uint16_t pt_idx   = (vaddr >> 12) & 0x1FF;
    
    uint64_t flags = PAGE_PRESENT | PAGE_WRITE | (is_user ? PAGE_USER : 0);

    if (!(pml4[pml4_idx] & PAGE_PRESENT)) { pml4[pml4_idx] = (uint64_t)alloc_page() | flags; }
    uint64_t* pdpt = (uint64_t*)(pml4[pml4_idx] & ~0xFFF);

    if (!(pdpt[pdpt_idx] & PAGE_PRESENT)) { pdpt[pdpt_idx] = (uint64_t)alloc_page() | flags; }
    uint64_t* pd = (uint64_t*)(pdpt[pdpt_idx] & ~0xFFF);

    if (!(pd[pd_idx] & PAGE_PRESENT)) { pd[pd_idx] = (uint64_t)alloc_page() | flags; }
    uint64_t* pt = (uint64_t*)(pd[pd_idx] & ~0xFFF);

    pt[pt_idx] = (paddr & ~0xFFF) | flags;
}

void map_user_memory(uint64_t pml4_addr, uint64_t vaddr, uint32_t size) {
    uint64_t aligned_vaddr = vaddr & ~0xFFF;
    uint32_t aligned_size = (size + 0xFFF) & ~0xFFF;
    for (uint64_t i = 0; i < aligned_size; i += PAGE_SIZE) {
        void* phys_page = alloc_page();
        if (phys_page) {
            map_page(pml4_addr, aligned_vaddr + i, (uint64_t)phys_page, 1);
        }
    }
}

void init_paging(uint64_t fb_addr) {
    uint64_t cr4;
    __asm__ volatile("mov %%cr4, %0" : "=r"(cr4));
    cr4 &= ~((1ULL << 20) | (1ULL << 21));
    __asm__ volatile("mov %0, %%cr4" :: "r"(cr4));

    kernel_pml4 = (uint64_t*)alloc_page();
    
    for (uint64_t i = 0; i < 0x20000000; i += PAGE_SIZE) {
        map_page((uint64_t)kernel_pml4, i, i, 1);
    }
    
    if (fb_addr) {
        uint64_t fb_aligned = fb_addr & ~0xFFF;
        for (uint64_t i = 0; i < 0x00800000; i += PAGE_SIZE) { 
            map_page((uint64_t)kernel_pml4, fb_aligned + i, fb_aligned + i, 1);
        }
    }

    switch_address_space((uint64_t)kernel_pml4);
}

void map_mmio(uint64_t paddr, uint32_t size) {
    uint64_t aligned = paddr & ~0xFFF;
    uint32_t aligned_size = (size + 0xFFF) & ~0xFFF;
    for (uint64_t i = 0; i < aligned_size; i += PAGE_SIZE) {
        map_page((uint64_t)kernel_pml4, aligned + i, aligned + i, 1);
    }
    __asm__ volatile("mov %%cr3, %%rax\n mov %%rax, %%cr3" ::: "rax", "memory");
}

uint64_t create_address_space() {
    uint64_t* new_pml4 = (uint64_t*)alloc_page();
    for(int i=0; i<512; i++) {
        new_pml4[i] = kernel_pml4[i];
    }
    return (uint64_t)new_pml4;
}

void switch_address_space(uint64_t pml4) {
    __asm__ volatile("mov %0, %%cr3" :: "r"(pml4));
}