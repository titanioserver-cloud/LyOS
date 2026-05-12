#include "../include/kernel.h"
#include "../include/idt.h"
#include "../include/gdt.h"
#include "../include/mouse.h"
#include "../include/graphics.h"
#include "../include/task.h"
#include "../include/memory.h"
#include "../include/keyboard.h"
#include "../include/tar.h"
#include "../include/elf.h"
#include "../include/paging.h"
#include "../include/pci.h"
#include "../include/e1000.h"
#include "../include/ide.h"
#include "../include/exfat.h"
#include "../include/net.h"
#include "../include/ac97.h"

struct mb2_tag { uint32_t type; uint32_t size; };
struct mb2_tag_fb { uint32_t type; uint32_t size; uint64_t addr; uint32_t pitch; uint32_t w; uint32_t h; uint8_t bpp; };
struct mb2_tag_module { uint32_t type; uint32_t size; uint32_t mod_start; uint32_t mod_end; char string[]; };

uint8_t* initrd_ptr_global = 0;

int sys_exec_impl(const char* filename) {
    if (!initrd_ptr_global) return -1;
    uint8_t* elf_data = tar_find_file(initrd_ptr_global, filename);
    if (elf_data) return load_elf(elf_data);
    return -1;
}

void bg_process() {
    int dhcp_attempts = 0;
    while (1) { 
        net_poll();
        dhcp_attempts++;
        if (dhcp_attempts == 300) {
            dhcp_request();
        } else if (dhcp_attempts == 600) {
            dhcp_request();
        } else if (dhcp_attempts == 900) {
            dhcp_request();
        }
        __asm__ volatile ("mov $19, %%rax\n int $0x80" ::: "rax");
    } 
}

void itoa(int num, char* str) {
    int i = 0; 
    if (num == 0) { str[i++] = '0'; str[i] = '\0'; return; }
    while (num != 0) { str[i++] = (num % 10) + '0'; num = num / 10; }
    str[i] = '\0'; 
    int start = 0; int end = i - 1;
    while (start < end) { char temp = str[start]; str[start] = str[end]; str[end] = temp; start++; end--; }
}

void kernel_main(uint64_t mb_addr) {
    struct mb2_tag* tag = (struct mb2_tag*)(mb_addr + 8);
    uint32_t* fb_ptr = 0;
    while (tag->type != 0) {
        if (tag->type == 8) fb_ptr = (uint32_t*)(uint64_t)((struct mb2_tag_fb*)tag)->addr;
        else if (tag->type == 3) initrd_ptr_global = (uint8_t*)(uint64_t)((struct mb2_tag_module*)tag)->mod_start;
        tag = (struct mb2_tag*)(((uint8_t*)tag) + ((tag->size + 7) & ~7));
    }

    gdt_init();
    memory_init(0x01000000, 0x5DC00000);
    init_paging((uint64_t)fb_ptr);

    init_graphics(fb_ptr, 1280, 720);
    idt_init(); 
    mouse_init(); 
    tasking_init();

    pci_enumerate();
    init_e1000();
    net_init();
    ac97_init();
    ide_init();
    exfat_init();
    
    char lba_str[32];
    itoa(exfat_get_root_lba(), lba_str);

    char disk_label[32] = "Nenhum";
    exfat_get_volume_label(disk_label);

    create_task(bg_process);

    if (initrd_ptr_global) {
        if (tar_find_file(initrd_ptr_global, "gui.elf")) {
            load_elf(tar_find_file(initrd_ptr_global, "gui.elf"));
        } else { 
            clear_backbuffer(0x0000AA); draw_string(20, 20, "PANIC: gui.elf nao encontrado!", 0xFFFFFF); update_screen(); 
        }
    } else {
        clear_backbuffer(0xAA0000); draw_string(20, 20, "PANIC: initrd.tar falhou!", 0xFFFFFF); update_screen();
    }
    
    draw_rect_alpha(10, 10, 350, 70, 0x000000, 200);
    draw_string(20, 20, "Root Dir LBA: ", 0xFFFFFF);
    draw_string(140, 20, lba_str, 0xA6E3A1);
    draw_string(20, 40, "Nome do Disco: ", 0xFFFFFF);
    draw_string(150, 40, disk_label, 0xF9E2AF);
    update_screen();
    
    __asm__ volatile("sti");
    while(1) { __asm__ volatile("hlt"); }
}