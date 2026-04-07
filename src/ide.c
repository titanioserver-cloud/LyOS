#include "../include/ide.h"
#include "../include/kernel.h"

// Funções para ler/escrever 16 bits (WORD) nas portas de I/O
static inline void outw(uint16_t port, uint16_t val) {
    __asm__ volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    __asm__ volatile ("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// Espera até o disco estar pronto para transferir dados
static void ide_wait() {
    // Porta 0x1F7 é o registo de Status. 
    // 0x80 = BSY (Busy), 0x08 = DRQ (Data Request Ready)
    while ((inb(0x1F7) & 0x80) || !(inb(0x1F7) & 0x08));
}

void ide_init() {
    // Mais tarde, podemos expandir isto para detetar os discos automaticamente via PCI.
    // Por enquanto, assumimos o Primary Master (0x1F0).
}

// Lê setores do disco usando LBA28
void ide_read_sectors(uint32_t lba, uint8_t sector_count, uint8_t* buffer) {
    outb(0x1F6, 0xE0 | ((lba >> 24) & 0x0F)); // 0xE0 = Master Drive + LBA mode
    outb(0x1F2, sector_count);                // Quantos setores ler
    outb(0x1F3, (uint8_t)lba);                // LBA Low
    outb(0x1F4, (uint8_t)(lba >> 8));         // LBA Mid
    outb(0x1F5, (uint8_t)(lba >> 16));        // LBA High
    outb(0x1F7, 0x20);                        // Comando: Ler com Retry (0x20)

    uint16_t* ptr = (uint16_t*)buffer;
    for (int i = 0; i < sector_count; i++) {
        ide_wait(); // Espera que o disco tenha o setor pronto
        for (int j = 0; j < 256; j++) {     // 256 words = 512 bytes por setor
            *ptr++ = inw(0x1F0);
        }
    }
}

// Escreve setores no disco
void ide_write_sectors(uint32_t lba, uint8_t sector_count, uint8_t* buffer) {
    outb(0x1F6, 0xE0 | ((lba >> 24) & 0x0F));
    outb(0x1F2, sector_count);
    outb(0x1F3, (uint8_t)lba);
    outb(0x1F4, (uint8_t)(lba >> 8));
    outb(0x1F5, (uint8_t)(lba >> 16));
    outb(0x1F7, 0x30);                        // Comando: Escrever com Retry (0x30)

    uint16_t* ptr = (uint16_t*)buffer;
    for (int i = 0; i < sector_count; i++) {
        ide_wait();
        for (int j = 0; j < 256; j++) {
            outw(0x1F0, *ptr++);
        }
        // No final de cada setor, dar um "Flush Cache" para garantir que gravou (opcional, mas seguro)
        outb(0x1F7, 0xE7);
    }
}