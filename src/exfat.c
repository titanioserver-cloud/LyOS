#include "../include/exfat.h"
#include "../include/ide.h"
#include "../include/lib.h"
#include <stddef.h>

extern int strcmp(const char* s1, const char* s2);
void strcpy(char* dest, const char* src) { while (*src) *dest++ = *src++; *dest = '\0'; }
void strcat(char* dest, const char* src) { while (*dest) dest++; while (*src) *dest++ = *src++; *dest = '\0'; }
int str_len(const char* s) { int l=0; while(s[l]) l++; return l; }
void* malloc(size_t s);

exfat_vbr_t vbr;
uint32_t root_dir_lba = 0;
uint32_t cwd_lba = 0;
uint32_t cwd_cluster = 0;
uint32_t sectors_per_cluster = 0;
uint32_t bytes_per_cluster = 0;
uint8_t curr_path[EXFAT_MAX_PATH] = "/";
char vbr_fs_name[9] = "NONE";
int exfat_ready = 0;

void exfat_init() {
    uint8_t sector[512];
    ide_read_sectors(0, 1, sector);
    uint8_t* ptr = (uint8_t*)&vbr;
    for(int i = 0; i < 512; i++) { ptr[i] = sector[i]; }
    if (vbr.fs_name[0] != 'E' || vbr.fs_name[1] != 'X' || vbr.fs_name[2] != 'F') { exfat_ready = 0; return; }
    sectors_per_cluster = (1 << vbr.sectors_per_cluster_shift);
    bytes_per_cluster = sectors_per_cluster * 512;
    root_dir_lba = vbr.cluster_heap_offset + ((vbr.root_dir_first_cluster - 2) * sectors_per_cluster);
    cwd_lba = root_dir_lba;
    cwd_cluster = vbr.root_dir_first_cluster;
    exfat_ready = 1;
    for(int i=0; i<8; i++) vbr_fs_name[i] = vbr.fs_name[i];
    vbr_fs_name[8] = 0;
}

uint32_t exfat_get_root_lba() { return root_dir_lba; }

uint32_t exfat_cluster_to_lba(uint32_t cluster) {
    return vbr.cluster_heap_offset + ((cluster - 2) * sectors_per_cluster);
}

int exfat_read_cluster(uint32_t cluster, uint8_t* buffer) {
    if (!exfat_ready || cluster < 2) return 0;
    uint32_t lba = exfat_cluster_to_lba(cluster);
    uint32_t secs = sectors_per_cluster;
    if (secs > 8) secs = 8;
    ide_read_sectors(lba, secs, buffer);
    return 1;
}

int exfat_write_cluster(uint32_t cluster, uint8_t* buffer) {
    if (!exfat_ready || cluster < 2) return 0;
    uint32_t lba = exfat_cluster_to_lba(cluster);
    uint32_t secs = sectors_per_cluster;
    if (secs > 8) secs = 8;
    ide_write_sectors(lba, secs, buffer);
    return 1;
}

uint32_t exfat_get_fat_entry(uint32_t cluster) {
    if (!exfat_ready || cluster < 2) return 0xFFFFFFFF;
    uint32_t fat_lba = vbr.fat_offset;
    uint32_t fat_offset_bytes = cluster * 4;
    uint32_t fat_sector = fat_lba + (fat_offset_bytes / 512);
    uint32_t fat_offset_in_sector = fat_offset_bytes % 512;
    uint8_t sector[512];
    ide_read_sectors(fat_sector, 1, sector);
    return *(uint32_t*)&sector[fat_offset_in_sector];
}

void exfat_set_fat_entry(uint32_t cluster, uint32_t value) {
    if (!exfat_ready || cluster < 2) return;
    uint32_t fat_lba = vbr.fat_offset;
    uint32_t fat_offset_bytes = cluster * 4;
    uint32_t fat_sector = fat_lba + (fat_offset_bytes / 512);
    uint32_t fat_offset_in_sector = fat_offset_bytes % 512;
    uint8_t sector[512];
    ide_read_sectors(fat_sector, 1, sector);
    *(uint32_t*)&sector[fat_offset_in_sector] = value;
    ide_write_sectors(fat_sector, 1, sector);
}

uint32_t exfat_alloc_cluster() {
    if (!exfat_ready) return 0;
    for (uint32_t c = 2; c < vbr.cluster_count + 2; c++) {
        uint32_t val = exfat_get_fat_entry(c);
        if (val == 0) {
            exfat_set_fat_entry(c, 0xFFFFFFFF);
            uint8_t buf[4096];
            memset(buf, 0, bytes_per_cluster > 4096 ? 4096 : bytes_per_cluster);
            exfat_write_cluster(c, buf);
            return c;
        }
    }
    return 0;
}

void exfat_free_cluster(uint32_t cluster) {
    if (!exfat_ready || cluster < 2) return;
    uint32_t next = exfat_get_fat_entry(cluster);
    exfat_set_fat_entry(cluster, 0);
    if (next >= 2 && next < 0xFFFFFFF0) exfat_free_cluster(next);
}

void exfat_get_volume_label(char* out_label) {
    uint8_t cluster_buf[4096];
    exfat_read_cluster(vbr.root_dir_first_cluster, cluster_buf);
    for(int i = 0; i < bytes_per_cluster; i += 32) {
        if (cluster_buf[i] == 0x00) break;
        if (cluster_buf[i] == 0x83) {
            int char_count = cluster_buf[i + 1];
            for(int c = 0; c < char_count && c < 11; c++) out_label[c] = cluster_buf[i + 2 + (c * 2)];
            out_label[char_count] = '\0';
            return;
        }
    }
    out_label[0] = '\0';
}

static void read_entry_name(uint8_t* cluster_buf, int start_offset, int secs, char* out_name) {
    int n_idx = 0;
    for (int s = 1; s <= secs; s++) {
        int off = start_offset + (s * 32);
        if (cluster_buf[off] == 0xC1) {
            for (int c = 0; c < 15; c++) {
                char ch = cluster_buf[off + 2 + (c * 2)];
                if (ch && n_idx < EXFAT_MAX_NAME-1) out_name[n_idx++] = ch;
            }
        }
    }
    out_name[n_idx] = '\0';
}

static uint32_t read_entry_cluster(uint8_t* cluster_buf, int start_offset, int secs) {
    for (int s = 1; s <= secs; s++) {
        int off = start_offset + (s * 32);
        if (cluster_buf[off] == 0xC0) return *(uint32_t*)&cluster_buf[off + 20];
    }
    return 0;
}

static uint64_t read_entry_size(uint8_t* cluster_buf, int start_offset, int secs) {
    for (int s = 1; s <= secs; s++) {
        int off = start_offset + (s * 32);
        if (cluster_buf[off] == 0xC0) {
            uint64_t size_low = *(uint32_t*)&cluster_buf[off + 24];
            uint64_t size_high = *(uint32_t*)&cluster_buf[off + 28];
            return size_low | (size_high << 32);
        }
    }
    return 0;
}

static void read_dir_entries(uint32_t cluster, uint8_t* buf) {
    exfat_read_cluster(cluster, buf);
    // Follow FAT chain for additional clusters (simple version)
    uint32_t next = exfat_get_fat_entry(cluster);
    int offset = bytes_per_cluster;
    while (next >= 2 && next < 0xFFFFFFF0 && offset < 65536) {
        exfat_read_cluster(next, buf + offset);
        offset += bytes_per_cluster;
        next = exfat_get_fat_entry(next);
    }
}

void exfat_list_dir(char* out_buf) {
    uint8_t cluster_buf[65536];
    read_dir_entries(cwd_cluster, cluster_buf);
    out_buf[0] = '\0';
    int found = 0;
    for (int i = 0; i < 65536; i += 32) {
        if (cluster_buf[i] == 0x00) break;
        if (cluster_buf[i] == 0x85) {
            int secs = cluster_buf[i + 1];
            char name[EXFAT_MAX_NAME];
            read_entry_name(cluster_buf, i, secs, name);
            if (str_len(name) > 0) {
                // Check if directory
                int is_dir = 0;
                for (int s = 1; s <= secs; s++) {
                    int off = i + (s * 32);
                    if (cluster_buf[off] == 0xC0 && (cluster_buf[off+1] & 0x03) == 1) {
                        is_dir = 1;
                    }
                }
                if (is_dir) {
                    strcat(out_buf, "["); strcat(out_buf, name); strcat(out_buf, "] ");
                } else {
                    // For files, use {name:size} format
                    uint64_t size = read_entry_size(cluster_buf, i, secs);
                    // Convert size to string manually (no libc)
                    char size_str[32];
                    char tmp[32];
                    int si = 0;
                    uint64_t s = size;
                    if (s == 0) { tmp[si++] = '0'; }
                    while (s > 0) { tmp[si++] = '0' + (s % 10); s /= 10; }
                    tmp[si] = '\0';
                    int a = 0, b = si - 1;
                    while (a < b) { char t = tmp[a]; tmp[a] = tmp[b]; tmp[b] = t; a++; b--; }
                    strcpy(size_str, tmp);
                    
                    strcat(out_buf, "{"); strcat(out_buf, name); strcat(out_buf, ":");
                    strcat(out_buf, size_str); strcat(out_buf, "} ");
                }
                found = 1;
            }
            i += (secs * 32);
        }
    }
    if (!found) strcpy(out_buf, "(Vazio)");
}

int exfat_list_entries(exfat_entry_t* entries, int max_count) {
    uint8_t cluster_buf[65536];
    read_dir_entries(cwd_cluster, cluster_buf);
    int count = 0;
    for (int i = 0; i < 65536 && count < max_count; i += 32) {
        if (cluster_buf[i] == 0x00) break;
        if (cluster_buf[i] == 0x85) {
            int secs = cluster_buf[i + 1];
            char name[EXFAT_MAX_NAME];
            read_entry_name(cluster_buf, i, secs, name);
            uint32_t first_cluster = read_entry_cluster(cluster_buf, i, secs);
            uint64_t size = read_entry_size(cluster_buf, i, secs);
            
            if (str_len(name) > 0) {
                strcpy(entries[count].name, name);
                entries[count].first_cluster = first_cluster;
                entries[count].size = size;
                entries[count].is_dir = 0;
                entries[count].is_valid = 1;
                // Check if dir by seeing entry type
                for (int s = 1; s <= secs; s++) {
                    int off = i + (s * 32);
                    if (cluster_buf[off] == 0xC0 && (cluster_buf[off+1] & 0x03) == 1) {
                        entries[count].is_dir = 1;
                    }
                }
                count++;
            }
            i += (secs * 32);
        }
    }
    return count;
}

uint32_t exfat_get_file_size(const char* name) {
    uint8_t cluster_buf[65536];
    read_dir_entries(cwd_cluster, cluster_buf);
    for (int i = 0; i < 65536; i += 32) {
        if (cluster_buf[i] == 0x00) break;
        if (cluster_buf[i] == 0x85) {
            int secs = cluster_buf[i + 1];
            char entry_name[EXFAT_MAX_NAME];
            read_entry_name(cluster_buf, i, secs, entry_name);
            if (strcmp(entry_name, name) == 0) {
                return read_entry_size(cluster_buf, i, secs);
            }
            i += (secs * 32);
        }
    }
    return 0;
}

int exfat_read_file(const char* name, uint8_t* buffer, uint32_t max_size) {
    uint8_t cluster_buf[65536];
    read_dir_entries(cwd_cluster, cluster_buf);
    for (int i = 0; i < 65536; i += 32) {
        if (cluster_buf[i] == 0x00) break;
        if (cluster_buf[i] == 0x85) {
            int secs = cluster_buf[i + 1];
            char entry_name[EXFAT_MAX_NAME];
            read_entry_name(cluster_buf, i, secs, entry_name);
            if (strcmp(entry_name, name) == 0) {
                uint32_t cluster = read_entry_cluster(cluster_buf, i, secs);
                uint64_t size = read_entry_size(cluster_buf, i, secs);
                if (size > max_size) size = max_size;
                uint32_t remaining = size;
                int buf_offset = 0;
                while (cluster >= 2 && cluster < 0xFFFFFFF0 && remaining > 0) {
                    uint8_t tmp[4096];
                    exfat_read_cluster(cluster, tmp);
                    uint32_t copy_size = remaining < bytes_per_cluster ? remaining : bytes_per_cluster;
                    if (copy_size > 4096) copy_size = 4096;
                    for (uint32_t k = 0; k < copy_size; k++) buffer[buf_offset + k] = tmp[k];
                    buf_offset += copy_size;
                    remaining -= copy_size;
                    cluster = exfat_get_fat_entry(cluster);
                }
                return size;
            }
            i += (secs * 32);
        }
    }
    return -1;
}

int exfat_create_file(const char* name) {
    uint8_t cluster_buf[65536];
    read_dir_entries(cwd_cluster, cluster_buf);
    
    // Check if already exists
    for (int i = 0; i < 65536; i += 32) {
        if (cluster_buf[i] == 0x00) break;
        if (cluster_buf[i] == 0x85) {
            int secs = cluster_buf[i + 1];
            char entry_name[EXFAT_MAX_NAME];
            read_entry_name(cluster_buf, i, secs, entry_name);
            if (strcmp(entry_name, name) == 0) return 0; // Already exists
            i += (secs * 32);
        }
    }
    
    // Allocate a cluster for the file
    uint32_t new_cluster = exfat_alloc_cluster();
    if (!new_cluster) return 0;
    
    // Find free space in directory
    int free_offset = -1;
    for (int i = 0; i < 65536; i += 32) {
        if (cluster_buf[i] == 0x00) { free_offset = i; break; }
        if (cluster_buf[i] == 0x85) {
            int secs = cluster_buf[i + 1];
            i += (secs * 32);
        }
    }
    if (free_offset < 0) { exfat_free_cluster(new_cluster); return 0; }
    
    // Need secs for name length
    int name_len = str_len(name);
    int secs_needed = (name_len + 14) / 15;
    
    // Write directory entry
    memset(cluster_buf + free_offset, 0, (secs_needed + 1) * 32);
    cluster_buf[free_offset] = 0x85; // File entry
    cluster_buf[free_offset + 1] = secs_needed;
    
    // Write stream extension entry
    int stream_off = free_offset + 32;
    cluster_buf[stream_off] = 0xC0; // Stream extension
    cluster_buf[stream_off + 1] = 0x01; // No FAT chain, non-directory
    *(uint32_t*)&cluster_buf[stream_off + 20] = new_cluster; // First cluster
    *(uint64_t*)&cluster_buf[stream_off + 24] = 0; // Size = 0
    
    // Write file name entries
    for (int s = 1; s <= secs_needed; s++) {
        int fn_off = free_offset + (s + 1) * 32;
        cluster_buf[fn_off] = 0xC1; // File name entry
        cluster_buf[fn_off + 1] = (s == 1) ? 0x00 : 0x40; // First or continuation
        for (int c = 0; c < 15; c++) {
            int idx = (s - 1) * 15 + c;
            if (idx < name_len) {
                cluster_buf[fn_off + 2 + c*2] = name[idx];
            } else {
                cluster_buf[fn_off + 2 + c*2] = 0;
                cluster_buf[fn_off + 2 + c*2 + 1] = 0;
            }
        }
    }
    
    // Write back to directory
    exfat_write_cluster(cwd_cluster, cluster_buf);
    return 1;
}

int exfat_create_dir(const char* name) {
    (void)name;
    // Simplified - just creates empty directory entry
    uint8_t cluster_buf[65536];
    read_dir_entries(cwd_cluster, cluster_buf);
    
    uint32_t new_cluster = exfat_alloc_cluster();
    if (!new_cluster) return 0;
    
    int free_offset = -1;
    for (int i = 0; i < 65536; i += 32) {
        if (cluster_buf[i] == 0x00) { free_offset = i; break; }
        if (cluster_buf[i] == 0x85) {
            int secs = cluster_buf[i + 1];
            i += (secs * 32);
        }
    }
    if (free_offset < 0) { exfat_free_cluster(new_cluster); return 0; }
    
    int name_len = str_len(name);
    int secs_needed = (name_len + 14) / 15;
    
    memset(cluster_buf + free_offset, 0, (secs_needed + 1) * 32);
    cluster_buf[free_offset] = 0x85;
    cluster_buf[free_offset + 1] = secs_needed;
    
    int stream_off = free_offset + 32;
    cluster_buf[stream_off] = 0xC0;
    cluster_buf[stream_off + 1] = 0x03; // Directory
    *(uint32_t*)&cluster_buf[stream_off + 20] = new_cluster;
    *(uint64_t*)&cluster_buf[stream_off + 24] = 0;
    
    for (int s = 1; s <= secs_needed; s++) {
        int fn_off = free_offset + (s + 1) * 32;
        cluster_buf[fn_off] = 0xC1;
        cluster_buf[fn_off + 1] = (s == 1) ? 0x00 : 0x40;
        for (int c = 0; c < 15; c++) {
            int idx = (s - 1) * 15 + c;
            if (idx < name_len) {
                cluster_buf[fn_off + 2 + c*2] = name[idx];
            }
        }
    }
    
    exfat_write_cluster(cwd_cluster, cluster_buf);
    return 1;
}

int exfat_delete(const char* name) {
    uint8_t cluster_buf[65536];
    read_dir_entries(cwd_cluster, cluster_buf);
    
    for (int i = 0; i < 65536; i += 32) {
        if (cluster_buf[i] == 0x00) break;
        if (cluster_buf[i] == 0x85) {
            int secs = cluster_buf[i + 1];
            char entry_name[EXFAT_MAX_NAME];
            read_entry_name(cluster_buf, i, secs, entry_name);
            
            if (strcmp(entry_name, name) == 0) {
                uint32_t cluster = read_entry_cluster(cluster_buf, i, secs);
                // Free clusters
                if (cluster >= 2) exfat_free_cluster(cluster);
                // Mark as deleted
                for (int s = 0; s <= secs; s++) {
                    cluster_buf[i + s * 32] = 0xE5; // Deleted marker
                }
                exfat_write_cluster(cwd_cluster, cluster_buf);
                return 1;
            }
            i += (secs * 32);
        }
    }
    return 0;
}

int exfat_write_file(const char* name, const uint8_t* data, uint32_t size) {
    uint8_t cluster_buf[65536];
    read_dir_entries(cwd_cluster, cluster_buf);
    
    for (int i = 0; i < 65536; i += 32) {
        if (cluster_buf[i] == 0x00) break;
        if (cluster_buf[i] == 0x85) {
            int secs = cluster_buf[i + 1];
            char entry_name[EXFAT_MAX_NAME];
            read_entry_name(cluster_buf, i, secs, entry_name);
            
            if (strcmp(entry_name, name) == 0) {
                uint32_t cluster = read_entry_cluster(cluster_buf, i, secs);
                uint64_t old_size = read_entry_size(cluster_buf, i, secs);
                
                // Calculate needed clusters
                uint32_t needed = (size + bytes_per_cluster - 1) / bytes_per_cluster;
                uint32_t existing = (old_size + bytes_per_cluster - 1) / bytes_per_cluster;
                
                if (needed > existing) {
                    // Allocate more
                    uint32_t last = cluster;
                    while (1) {
                        uint32_t next = exfat_get_fat_entry(last);
                        if (next >= 0xFFFFFFF0 || next == 0) break;
                        last = next;
                    }
                    for (uint32_t c = existing; c < needed; c++) {
                        uint32_t new_c = exfat_alloc_cluster();
                        if (!new_c) return 0;
                        if (c == 0) {
                            cluster = new_c;
                        } else {
                            exfat_set_fat_entry(last, new_c);
                        }
                        last = new_c;
                    }
                    // Update stream entry cluster
                    int stream_off = i + 32;
                    *(uint32_t*)&cluster_buf[stream_off + 20] = cluster;
                }
                
                // Write data
                uint32_t remaining = size;
                uint32_t curr = cluster;
                int buf_off = 0;
                while (curr >= 2 && curr < 0xFFFFFFF0 && remaining > 0) {
                    uint8_t tmp[4096];
                    memset(tmp, 0, sizeof(tmp));
                    uint32_t copy_size = remaining < bytes_per_cluster ? remaining : bytes_per_cluster;
                    if (copy_size > 4096) copy_size = 4096;
                    for (uint32_t k = 0; k < copy_size; k++) tmp[k] = data[buf_off + k];
                    exfat_write_cluster(curr, tmp);
                    buf_off += copy_size;
                    remaining -= copy_size;
                    curr = exfat_get_fat_entry(curr);
                }
                
                // Update size in stream entry
                int stream_off = i + 32;
                *(uint64_t*)&cluster_buf[stream_off + 24] = size;
                exfat_write_cluster(cwd_cluster, cluster_buf);
                return 1;
            }
            i += (secs * 32);
        }
    }
    return 0;
}

int exfat_stat(const char* name, exfat_entry_t* out) {
    uint8_t cluster_buf[65536];
    read_dir_entries(cwd_cluster, cluster_buf);
    for (int i = 0; i < 65536; i += 32) {
        if (cluster_buf[i] == 0x00) break;
        if (cluster_buf[i] == 0x85) {
            int secs = cluster_buf[i + 1];
            char entry_name[EXFAT_MAX_NAME];
            read_entry_name(cluster_buf, i, secs, entry_name);
            if (strcmp(entry_name, name) == 0) {
                strcpy(out->name, entry_name);
                out->first_cluster = read_entry_cluster(cluster_buf, i, secs);
                out->size = read_entry_size(cluster_buf, i, secs);
                out->is_dir = 0;
                for (int s = 1; s <= secs; s++) {
                    int off = i + (s * 32);
                    if (cluster_buf[off] == 0xC0 && (cluster_buf[off+1] & 0x03) == 1) out->is_dir = 1;
                }
                out->is_valid = 1;
                return 1;
            }
            i += (secs * 32);
        }
    }
    out->is_valid = 0;
    return 0;
}

void exfat_cd(const char* path) {
    if (path[0] == '\0' || (path[0] == '/' && path[1] == '\0')) {
        cwd_cluster = vbr.root_dir_first_cluster;
        cwd_lba = root_dir_lba;
        strcpy((char*)curr_path, "/");
        return;
    }
    
    // Handle ".."
    if (strcmp(path, "..") == 0) {
        // Go to root for simplicity
        cwd_cluster = vbr.root_dir_first_cluster;
        cwd_lba = root_dir_lba;
        strcpy((char*)curr_path, "/");
        return;
    }
    
    uint8_t cluster_buf[65536];
    read_dir_entries(cwd_cluster, cluster_buf);
    for (int i = 0; i < 65536; i += 32) {
        if (cluster_buf[i] == 0x00) break;
        if (cluster_buf[i] == 0x85) {
            int secs = cluster_buf[i + 1];
            char entry_name[EXFAT_MAX_NAME];
            read_entry_name(cluster_buf, i, secs, entry_name);
            
            // Check if it's a directory
            int is_dir = 0;
            for (int s = 1; s <= secs; s++) {
                int off = i + (s * 32);
                if (cluster_buf[off] == 0xC0 && (cluster_buf[off+1] & 0x03) == 1) is_dir = 1;
            }
            
            if (is_dir && strcmp(entry_name, path) == 0) {
                uint32_t cluster = read_entry_cluster(cluster_buf, i, secs);
                if (cluster > 0) {
                    cwd_cluster = cluster;
                    cwd_lba = exfat_cluster_to_lba(cluster);
                    // Update path
                    if (str_len((char*)curr_path) > 1) strcat((char*)curr_path, "/");
                    strcat((char*)curr_path, path);
                    return;
                }
            }
            i += (secs * 32);
        }
    }
}

void exfat_getcwd(char* out) { strcpy(out, (char*)curr_path); }
