#include "../include/exfat.h"
#include "../include/ide.h"

extern int strcmp(const char* s1, const char* s2);
void strcpy(char* dest, const char* src) { while (*src) *dest++ = *src++; *dest = '\0'; }
void strcat(char* dest, const char* src) { while (*dest) dest++; while (*src) *dest++ = *src++; *dest = '\0'; }

exfat_vbr_t vbr;
uint32_t root_dir_lba = 0;
uint32_t cwd_lba = 0;

void exfat_init() {
    uint8_t sector[512];
    ide_read_sectors(0, 1, sector);
    uint8_t* ptr = (uint8_t*)&vbr;
    for(int i = 0; i < 512; i++) { ptr[i] = sector[i]; }
    if (vbr.fs_name[0] != 'E' || vbr.fs_name[1] != 'X' || vbr.fs_name[2] != 'F') return;
    uint32_t sectors_per_cluster = (1 << vbr.sectors_per_cluster_shift);
    root_dir_lba = vbr.cluster_heap_offset + ((vbr.root_dir_first_cluster - 2) * sectors_per_cluster);
    cwd_lba = root_dir_lba;
}

uint32_t exfat_get_root_lba() { return root_dir_lba; }

void exfat_get_volume_label(char* out_label) {
    uint8_t cluster_buf[4096];
    uint32_t sectors_per_cluster = (1 << vbr.sectors_per_cluster_shift);
    ide_read_sectors(root_dir_lba, (sectors_per_cluster > 8) ? 8 : sectors_per_cluster, cluster_buf);
    for(int i = 0; i < 4096; i += 32) {
        if (cluster_buf[i] == 0x00) break;
        if (cluster_buf[i] == 0x83) {
            int char_count = cluster_buf[i + 1];
            for(int c = 0; c < char_count && c < 11; c++) {
                out_label[c] = cluster_buf[i + 2 + (c * 2)];
            }
            out_label[char_count] = '\0';
            return;
        }
    }
    out_label[0] = '\0';
}

void exfat_list_dir(char* out_buf) {
    uint8_t cluster_buf[4096];
    uint32_t sectors_per_cluster = (1 << vbr.sectors_per_cluster_shift);
    ide_read_sectors(cwd_lba, (sectors_per_cluster > 8) ? 8 : sectors_per_cluster, cluster_buf);
    out_buf[0] = '\0';
    int found = 0;
    for (int i = 0; i < 4096; i += 32) {
        if (cluster_buf[i] == 0x00) break;
        if (cluster_buf[i] == 0x85) {
            int secs = cluster_buf[i + 1];
            char name[128] = {0}; int n_idx = 0;
            for (int s = 1; s <= secs; s++) {
                int off = i + (s * 32);
                if (off >= 4096) break;
                if (cluster_buf[off] == 0xC1) {
                    for (int c = 0; c < 15; c++) {
                        char ch = cluster_buf[off + 2 + (c * 2)];
                        if (ch) name[n_idx++] = ch;
                    }
                }
            }
            name[n_idx] = '\0';
            if (n_idx > 0) {
                strcat(out_buf, "["); strcat(out_buf, name); strcat(out_buf, "] "); found = 1;
            }
            i += (secs * 32);
        }
    }
    if (!found) strcpy(out_buf, "(Vazio)");
}

void exfat_cd(const char* path) {
    if (path[0] == '\0' || (path[0] == '/' && path[1] == '\0')) {
        cwd_lba = root_dir_lba; return;
    }
    uint8_t cluster_buf[4096];
    uint32_t sectors_per_cluster = (1 << vbr.sectors_per_cluster_shift);
    ide_read_sectors(cwd_lba, (sectors_per_cluster > 8) ? 8 : sectors_per_cluster, cluster_buf);
    for (int i = 0; i < 4096; i += 32) {
        if (cluster_buf[i] == 0x00) break;
        if (cluster_buf[i] == 0x85) {
            int secs = cluster_buf[i + 1];
            uint32_t first_cluster = 0;
            char name[128] = {0}; int n_idx = 0;
            for (int s = 1; s <= secs; s++) {
                int off = i + (s * 32);
                if (off >= 4096) break;
                if (cluster_buf[off] == 0xC0) {
                    first_cluster = *(uint32_t*)&cluster_buf[off + 20];
                }
                if (cluster_buf[off] == 0xC1) {
                    for (int c = 0; c < 15; c++) {
                        char ch = cluster_buf[off + 2 + (c * 2)];
                        if (ch) name[n_idx++] = ch;
                    }
                }
            }
            name[n_idx] = '\0';
            if (strcmp(name, path) == 0 && first_cluster > 0) {
                cwd_lba = vbr.cluster_heap_offset + ((first_cluster - 2) * sectors_per_cluster);
                return;
            }
            i += (secs * 32);
        }
    }
}