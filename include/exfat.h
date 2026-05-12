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

#define EXFAT_MAX_NAME 256
#define EXFAT_MAX_FILES 256
#define EXFAT_MAX_PATH 512

#define ENTRY_TYPE_FILE 0
#define ENTRY_TYPE_DIR 1

typedef struct {
    char name[EXFAT_MAX_NAME];
    uint32_t first_cluster;
    uint64_t size;
    int is_dir;
    int is_valid;
} exfat_entry_t;

void exfat_init();
uint32_t exfat_get_root_lba();
void exfat_get_volume_label(char* out_label);
void exfat_list_dir(char* out_buf);
void exfat_cd(const char* path);

// Novas funcoes de escrita
int exfat_read_cluster(uint32_t cluster, uint8_t* buffer);
int exfat_write_cluster(uint32_t cluster, uint8_t* buffer);
uint32_t exfat_alloc_cluster();
void exfat_free_cluster(uint32_t cluster);
uint32_t exfat_get_fat_entry(uint32_t cluster);
void exfat_set_fat_entry(uint32_t cluster, uint32_t value);
int exfat_create_file(const char* name);
int exfat_create_dir(const char* name);
int exfat_delete(const char* name);
int exfat_write_file(const char* name, const uint8_t* data, uint32_t size);
int exfat_read_file(const char* name, uint8_t* buffer, uint32_t max_size);
uint32_t exfat_get_file_size(const char* name);
void exfat_getcwd(char* out);
int exfat_stat(const char* name, exfat_entry_t* out);

// Explorador - listagem detalhada
int exfat_list_entries(exfat_entry_t* entries, int max_count);

#endif