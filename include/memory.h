#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>
#include <stddef.h>

void memory_init(uint64_t heap_start, uint64_t heap_size);
void* kmalloc(size_t size);
void kfree(void* ptr);
size_t get_used_memory();

#endif