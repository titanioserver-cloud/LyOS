#ifndef IDE_H
#define IDE_H

#include <stdint.h>

void ide_init();
void ide_read_sectors(uint32_t lba, uint8_t sector_count, uint8_t* buffer);
void ide_write_sectors(uint32_t lba, uint8_t sector_count, uint8_t* buffer);

#endif