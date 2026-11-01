#ifndef IMG_H
#define IMG_H
#include <stdint.h>
#include <stddef.h>

/* loads any supported image into a 256x192 0x00RRGGBB buffer */
int img_load(const char *path, uint32_t *out, char *msg, size_t msglen);
/* writes a 24-bit BMP */
int img_save_bmp(const char *path, const uint32_t *px, int w, int h);

#endif
