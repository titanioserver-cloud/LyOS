#ifndef EXFAT_H
#define EXFAT_H
#include <stdint.h>

typedef struct {
    uint8_t  jump_boot[3];
    char     fs_name[8];
    uint8_t  must_be_zero[53];
    uint64_t partition_offset;
    uint64_t volume_length;
    uint32_t fat_offset;
    uint32_t fat_length;
    uint32_t cluster_heap_offset;
    uint32_t cluster_count;
    uint32_t root_dir_first_cluster;
    uint32_t volume_serial_number;
    uint16_t file_system_revision;
    uint16_t volume_flags;
    uint8_t  bytes_per_sector_shift;
    uint8_t  sectors_per_cluster_shift;
    uint8_t  number_of_fats;
    uint8_t  drive_select;
    uint8_t  percent_in_use;
    uint8_t  reserved[7];
    uint8_t  boot_code[390];
    uint16_t boot_signature;
} __attribute__((packed)) exfat_vbr_t;

void exfat_init();
uint32_t exfat_get_root_lba();
void exfat_get_volume_label(char* out_label);
void exfat_list_dir(char* out_buf);
void exfat_cd(const char* path);

#endif