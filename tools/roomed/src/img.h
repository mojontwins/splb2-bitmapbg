#ifndef IMG_H
#define IMG_H
#include <stdint.h>

/* loads an image (PNG, BMP, GIF, TGA, JPG...) as 0x00RRGGBB pixels.
   Returns a malloc'ed buffer (free it) or NULL. */
uint32_t *img_load_rgb(const char *path, int *w, int *h);
/* writes a 24-bit BMP. 0 = ok */
int img_save_bmp(const char *path, const uint32_t *px, int w, int h);

#endif
