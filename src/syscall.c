#include "../include/syscall.h"
#include "../include/graphics.h"
#include "../include/mouse.h"
#include "../include/memory.h"
#include "../include/keyboard.h"
#include "../include/task.h"
#include "../include/pci.h"
#include "../include/e1000.h"
#include "../include/net.h"
#include "../include/exfat.h"

extern uint8_t mouse_byte[3];
extern uint64_t do_syscall(uint64_t sysno, uint64_t p1, uint64_t p2, uint64_t p3, uint64_t p4, uint64_t p5, uint64_t p6);
extern int sys_exec_impl(const char* filename);
extern void draw_buffer_fast(int x, int y, int w, int h, uint32_t* buffer);
extern void draw_string_in_buffer(uint32_t* buf, int buf_w, int x, int y, const char* str, uint32_t color);
extern void draw_rect_in_buffer(uint32_t* buf, int buf_w, int x, int y, int w, int h, uint32_t color);

struct regs { uint64_t rax, rdi, rsi, rdx, rcx, r8, r9; };

static inline void outb_local(uint16_t port, uint8_t val) { __asm__ volatile ( "outb %0, %1" : : "a"(val), "Nd"(port) ); }
static inline uint8_t inb_local(uint16_t port) { uint8_t ret; __asm__ volatile ( "inb %1, %0" : "=a"(ret) : "Nd"(port) ); return ret; }

int get_update_in_progress_flag() { outb_local(0x70, 0x0A); return (inb_local(0x71) & 0x80); }
uint8_t get_rtc_register(int reg) { outb_local(0x70, reg); return inb_local(0x71); }

void* shared_buffers[16] = {0};

void syscall_handler(struct regs* r) {
    switch(r->rax) {
        case 1: draw_rect_alpha(r->rdi, r->rsi, r->rdx, r->rcx, r->r8, r->r9); break;
        case 2: draw_rect_outline(r->rdi, r->rsi, r->rdx, r->rcx, r->r8, r->r9); break;
        case 3: draw_string(r->rdi, r->rsi, (const char*)r->rdx, r->rcx); break;
        case 4: update_screen(); break;
        case 5: clear_backbuffer(r->rdi); break;
        case 6: r->rax = mouse_x; break;
        case 7: r->rax = mouse_y; break;
        case 8: r->rax = (uint64_t)kmalloc((size_t)r->rdi); break;
        case 9: kfree((void*)r->rdi); break;
        case 10: r->rax = mouse_byte[0] & 0x01; break;
        case 11: r->rax = last_key; last_key = 0; break;
        case 12: r->rax = get_used_memory(); break;
        case 13: r->rax = get_num_tasks(); break;
        case 14: kill_task((int)r->rdi); break;
        case 15: r->rax = send_msg((int)r->rdi, (int)r->rsi, (int)r->rdx, (int)r->rcx); break;
        case 16: r->rax = recv_msg((int*)r->rdi, (int*)r->rsi, (int*)r->rdx); break;
        case 17: r->rax = sys_exec_impl((const char*)r->rdi); break;
        case 18: {
            while(get_update_in_progress_flag());
            uint8_t s = get_rtc_register(0x00);
            uint8_t m = get_rtc_register(0x02);
            uint8_t h = get_rtc_register(0x04);
            int* hr = (int*)r->rdi; int* min = (int*)r->rsi; int* sec = (int*)r->rdx;
            if(hr) *hr = (h & 0x0F) + ((h / 16) * 10);
            if(min) *min = (m & 0x0F) + ((m / 16) * 10);
            if(sec) *sec = (s & 0x0F) + ((s / 16) * 10);
            break;
        }
        case 19: __asm__ volatile("sti; hlt"); break;
        case 20: draw_circle_alpha(r->rdi, r->rsi, r->rdx, r->rcx, r->r8); break;
        case 21: draw_buffer_fast(r->rdi, r->rsi, r->rdx, r->rcx, (uint32_t*)r->r8); break;
        case 22: draw_string_in_buffer((uint32_t*)r->rdi, (int)r->rsi, (int)r->rdx, (int)r->rcx, (const char*)r->r8, (uint32_t)r->r9); break;
        case 23: {
            uint32_t w = (uint32_t)(r->r8 >> 32);
            uint32_t h = (uint32_t)(r->r8 & 0xFFFFFFFF);
            draw_rect_in_buffer((uint32_t*)r->rdi, (int)r->rsi, (int)r->rdx, (int)r->rcx, (int)w, (int)h, (uint32_t)r->r9);
            break;
        }
        case 24: r->rax = pci_get_device((int)r->rdi, (uint16_t*)r->rsi, (uint16_t*)r->rdx, (uint8_t*)r->rcx); break;
        case 25: {
            uint8_t* mac = get_mac_address();
            uint8_t* usr_buf = (uint8_t*)r->rdi;
            for(int i=0; i<6; i++) usr_buf[i] = mac[i];
            break;
        }
        case 26: e1000_send_packet((void*)r->rdi, (uint16_t)r->rsi); break;
        case 27: r->rax = e1000_receive_packet((void*)r->rdi); break;
        case 28: net_send_arp((uint8_t*)r->rdi); break;
        case 29: net_send_icmp((uint8_t*)r->rdi, (uint8_t*)r->rsi); break;
        case 30: net_send_dhcp(); break;
        case 31: net_send_dns((const char*)r->rdi); break;
        case 32: {
            uint8_t* ip = (uint8_t*)r->rdi;
            uint16_t sport = (r->rsi >> 16) & 0xFFFF;
            uint16_t dport = r->rsi & 0xFFFF;
            uint32_t seq = (r->rdx >> 32) & 0xFFFFFFFF;
            uint32_t ack = r->rdx & 0xFFFFFFFF;
            uint8_t flags = (r->rcx >> 32) & 0xFF;
            uint16_t dlen = r->rcx & 0xFFFF;
            const char* data = (const char*)r->r8;
            net_send_tcp(ip, sport, dport, seq, ack, flags, data, dlen);
            break;
        }
        case 33: exfat_get_volume_label((char*)r->rdi); break;
        case 34: exfat_list_dir((char*)r->rdi); break;
        case 35: exfat_cd((const char*)r->rdi); break;
        case 36: 
            if(r->rdi < 16) {
                if(!shared_buffers[r->rdi]) shared_buffers[r->rdi] = kmalloc((size_t)r->rsi); 
                r->rax = (uint64_t)shared_buffers[r->rdi]; 
            } else {
                r->rax = 0;
            }
            break;
        case 37: 
            if(r->rdi < 16) r->rax = (uint64_t)shared_buffers[r->rdi]; 
            else r->rax = 0;
            break;
        case 38: r->rax = tcp_connect((uint8_t*)r->rdi, (uint16_t)r->rsi); break;
        case 39: tcp_send((int)r->rdi, (const char*)r->rsi, (int)r->rdx); break;
        case 40: r->rax = tcp_recv((int)r->rdi, (char*)r->rsi, (int)r->rdx); break;
    }
}