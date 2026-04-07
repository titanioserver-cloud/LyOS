#include "../include/tar.h"
#include "../include/lib.h"
int octal_to_int(const char* str) {
    int val = 0;
    while(*str >= '0' && *str <= '7') { val = val * 8 + (*str - '0'); str++; }
    return val;
}
uint8_t* tar_find_file(uint8_t* archive, const char* filename) {
    uint8_t* ptr = archive;
    while (memcmp(ptr + 257, "ustar", 5) == 0) {
        int filesize = octal_to_int((char*)(ptr + 124));
        if (strcmp((char*)ptr, filename) == 0) return ptr + 512;
        ptr += (((filesize + 511) / 512) + 1) * 512;
    }
    return 0;
}