#include "../include/memory.h"

typedef struct mem_block {
    size_t size;
    int free;
    struct mem_block* next;
    uint32_t magic;
} mem_block_t;

#define BLOCK_MAGIC 0x12345678
#define ALIGNMENT 16

static mem_block_t* heap_head = NULL;

static size_t align_size(size_t size) {
    return (size + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
}

void memory_init(uint64_t heap_start, uint64_t heap_size) {
    heap_head = (mem_block_t*)heap_start;
    heap_head->size = heap_size - sizeof(mem_block_t);
    heap_head->free = 1;
    heap_head->next = NULL;
    heap_head->magic = BLOCK_MAGIC;
}

void* kmalloc(size_t size) {
    if (size == 0) return NULL;
    
    size_t aligned_size = align_size(size);
    mem_block_t* current = heap_head;
    
    while (current != NULL) {
        if (current->free && current->size >= aligned_size) {
            if (current->size >= aligned_size + sizeof(mem_block_t) + ALIGNMENT) {
                mem_block_t* new_block = (mem_block_t*)((uint8_t*)current + sizeof(mem_block_t) + aligned_size);
                new_block->size = current->size - aligned_size - sizeof(mem_block_t);
                new_block->free = 1;
                new_block->next = current->next;
                new_block->magic = BLOCK_MAGIC;
                
                current->size = aligned_size;
                current->next = new_block;
            }
            current->free = 0;
            return (void*)((uint8_t*)current + sizeof(mem_block_t));
        }
        current = current->next;
    }
    return NULL;
}

void kfree(void* ptr) {
    if (!ptr) return;
    
    mem_block_t* block = (mem_block_t*)((uint8_t*)ptr - sizeof(mem_block_t));
    if (block->magic != BLOCK_MAGIC) return;
    
    block->free = 1;
    
    mem_block_t* current = heap_head;
    while (current != NULL) {
        if (current->free && current->next != NULL && current->next->free) {
            current->size += sizeof(mem_block_t) + current->next->size;
            current->next = current->next->next;
        } else {
            current = current->next;
        }
    }
}

size_t get_used_memory() {
    size_t used = 0;
    mem_block_t* current = heap_head;
    while (current != NULL) {
        if (!current->free) {
            used += current->size + sizeof(mem_block_t);
        }
        current = current->next;
    }
    return used;
}