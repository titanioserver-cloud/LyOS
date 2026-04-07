#include "../include/elf.h"
#include "../include/lib.h"
#include "../include/task.h"

int load_elf(uint8_t* file_data) {
    Elf64_Ehdr* ehdr = (Elf64_Ehdr*)file_data;
    Elf64_Phdr* phdr = (Elf64_Phdr*)(file_data + ehdr->e_phoff);
    for(int i = 0; i < ehdr->e_phnum; i++) {
        if(phdr[i].p_type == 1) { 
            memcpy((void*)phdr[i].p_vaddr, file_data + phdr[i].p_offset, phdr[i].p_filesz);
            memset((void*)(phdr[i].p_vaddr + phdr[i].p_filesz), 0, phdr[i].p_memsz - phdr[i].p_filesz);
        }
    }
    return create_task((void(*)())ehdr->e_entry);
}