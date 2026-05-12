#include "../include/e1000.h"
#include "../include/pci.h"
#include "../include/paging.h"

struct e1000_rx_desc {
    uint64_t addr;
    uint16_t length;
    uint16_t checksum;
    uint8_t status;
    uint8_t errors;
    uint16_t special;
} __attribute__((packed));

struct e1000_tx_desc {
    uint64_t addr;
    uint16_t length;
    uint8_t cso;
    uint8_t cmd;
    uint8_t status;
    uint8_t css;
    uint16_t special;
} __attribute__((packed));

uint64_t e1000_mem_base = 0;
uint8_t mac_addr[6];

__attribute__((aligned(16))) struct e1000_rx_desc rx_descs[32];
__attribute__((aligned(16))) struct e1000_tx_desc tx_descs[8];

uint8_t rx_bufs[32][8192];
uint8_t tx_bufs[8][8192];
int tx_tail = 0;
int rx_cur = 0;

static inline void mmio_write32(uint64_t addr, uint32_t val) {
    *((volatile uint32_t*)addr) = val;
}

static inline uint32_t mmio_read32(uint64_t addr) {
    return *((volatile uint32_t*)addr);
}

void e1000_rx_init() {
    uint64_t rx_ptr = (uint64_t)rx_descs;
    for(int i = 0; i < 32; i++) {
        rx_descs[i].addr = (uint64_t)rx_bufs[i];
        rx_descs[i].status = 0;
    }
    mmio_write32(e1000_mem_base + 0x2800, (uint32_t)(rx_ptr & 0xFFFFFFFF));
    mmio_write32(e1000_mem_base + 0x2804, (uint32_t)(rx_ptr >> 32));
    mmio_write32(e1000_mem_base + 0x2808, 32 * 16);
    mmio_write32(e1000_mem_base + 0x2810, 0);
    mmio_write32(e1000_mem_base + 0x2818, 31);
    mmio_write32(e1000_mem_base + 0x0100, (1 << 1) | (1 << 2) | (1 << 3) | (1 << 4) | (1 << 15));
}

void e1000_tx_init() {
    uint64_t tx_ptr = (uint64_t)tx_descs;
    for(int i = 0; i < 8; i++) {
        tx_descs[i].addr = (uint64_t)tx_bufs[i];
        tx_descs[i].cmd = 0;
        tx_descs[i].status = 1;
    }
    mmio_write32(e1000_mem_base + 0x3800, (uint32_t)(tx_ptr & 0xFFFFFFFF));
    mmio_write32(e1000_mem_base + 0x3804, (uint32_t)(tx_ptr >> 32));
    mmio_write32(e1000_mem_base + 0x3808, 8 * 16);
    mmio_write32(e1000_mem_base + 0x3810, 0);
    mmio_write32(e1000_mem_base + 0x3818, 0);
    mmio_write32(e1000_mem_base + 0x0400, (1 << 1) | (1 << 3));
}

void e1000_enable() {
    uint32_t ctrl = mmio_read32(e1000_mem_base + 0x0000);
    mmio_write32(e1000_mem_base + 0x0000, ctrl | (1 << 6));
}

void init_e1000() {
    uint8_t bus, slot, func;
    if (!pci_find_device(0x8086, 0x100E, &bus, &slot, &func)) return;

    uint32_t bar0 = pci_read_bar(bus, slot, func, 0);
    e1000_mem_base = (uint64_t)(bar0 & ~15); 

    map_mmio(e1000_mem_base, 0x20000);

    pci_enable_bus_mastering(bus, slot, func);

    uint32_t mac_low = mmio_read32(e1000_mem_base + 0x5400);
    uint32_t mac_high = mmio_read32(e1000_mem_base + 0x5404);

    mac_addr[0] = (mac_low >> 0) & 0xFF;
    mac_addr[1] = (mac_low >> 8) & 0xFF;
    mac_addr[2] = (mac_low >> 16) & 0xFF;
    mac_addr[3] = (mac_low >> 24) & 0xFF;
    mac_addr[4] = (mac_high >> 0) & 0xFF;
    mac_addr[5] = (mac_high >> 8) & 0xFF;

    e1000_enable();
    e1000_rx_init();
    e1000_tx_init();
}

uint8_t* get_mac_address() {
    return mac_addr;
}

void e1000_send_packet(void* data, uint16_t len) {
    uint8_t* dest = (uint8_t*)tx_bufs[tx_tail];
    uint8_t* src = (uint8_t*)data;
    for(int i = 0; i < len; i++) dest[i] = src[i];
    
    tx_descs[tx_tail].length = len;
    tx_descs[tx_tail].cmd = 0x09; 
    tx_descs[tx_tail].status = 0;
    
    tx_tail = (tx_tail + 1) % 8;
    mmio_write32(e1000_mem_base + 0x3818, tx_tail);
}

int e1000_receive_packet(void* buf) {
    if((rx_descs[rx_cur].status & 0x01) == 0) return 0; 
    
    uint8_t* dest = (uint8_t*)buf;
    uint8_t* src = (uint8_t*)rx_bufs[rx_cur];
    uint16_t len = rx_descs[rx_cur].length;
    
    for(int i = 0; i < len; i++) dest[i] = src[i];
    
    rx_descs[rx_cur].status = 0;
    uint32_t old_cur = rx_cur;
    rx_cur = (rx_cur + 1) % 32;
    mmio_write32(e1000_mem_base + 0x2818, old_cur);
    
    return len;
}

int e1000_has_packet() {
    return (rx_descs[rx_cur].status & 0x01) != 0;
}