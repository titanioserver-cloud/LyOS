#include "../include/lib.h"
void* memcpy(void* dest, const void* src, size_t n) {
    uint8_t* d = (uint8_t*)dest; const uint8_t* s = (const uint8_t*)src;
    while (n--) *d++ = *s++; return dest;
}
void* memset(void* s, int c, size_t n) {
    uint8_t* p = (uint8_t*)s;
    while (n--) *p++ = (uint8_t)c; return s;
}
int memcmp(const void* s1, const void* s2, size_t n) {
    const uint8_t* p1 = (const uint8_t*)s1; const uint8_t* p2 = (const uint8_t*)s2;
    while (n--) { if (*p1 != *p2) return *p1 - *p2; p1++; p2++; } return 0;
}
int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) { s1++; s2++; }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}