#ifndef TAR_H
#define TAR_H
#include <stdint.h>
uint8_t* tar_find_file(uint8_t* archive, const char* filename);
#endif