/* Beeptune - 24-bit BMP writer (screenshots of the headless test build) */
#ifndef BMP_H
#define BMP_H
#include <stdint.h>
int bmp_save(const char *path, const uint32_t *px, int w, int h);
#endif
