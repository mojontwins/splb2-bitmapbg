/* roomed - image loading (stb_image) and BMP writing */
#include "img.h"

#include <stdio.h>
#include <stdlib.h>

#if defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#endif
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#include "../third_party/stb_image.h"

uint32_t *img_load_rgb(const char *path, int *w, int *h) {
    int comp, i;
    unsigned char *d = stbi_load(path, w, h, &comp, 3);
    uint32_t *out;
    if (!d) return NULL;
    out = (uint32_t *)malloc((size_t)*w * *h * 4);
    if (out)
        for (i = 0; i < *w * *h; i++)
            out[i] = ((uint32_t)d[i * 3] << 16) | ((uint32_t)d[i * 3 + 1] << 8) | d[i * 3 + 2];
    stbi_image_free(d);
    return out;
}

static void put32(FILE *f, uint32_t v) { fputc(v & 255, f); fputc((v >> 8) & 255, f); fputc((v >> 16) & 255, f); fputc(v >> 24, f); }
static void put16(FILE *f, unsigned v) { fputc(v & 255, f); fputc((v >> 8) & 255, f); }

int img_save_bmp(const char *path, const uint32_t *px, int w, int h) {
    FILE *f = fopen(path, "wb");
    int stride = (w * 3 + 3) & ~3, x, y;
    if (!f) return -1;
    fputc('B', f); fputc('M', f);
    put32(f, 54 + stride * h); put32(f, 0); put32(f, 54);
    put32(f, 40); put32(f, w); put32(f, h); put16(f, 1); put16(f, 24);
    put32(f, 0); put32(f, stride * h); put32(f, 2835); put32(f, 2835); put32(f, 0); put32(f, 0);
    for (y = h - 1; y >= 0; y--) {
        for (x = 0; x < w; x++) {
            uint32_t c = px[y * w + x];
            fputc(c & 255, f); fputc((c >> 8) & 255, f); fputc((c >> 16) & 255, f);
        }
        for (x = w * 3; x < stride; x++) fputc(0, f);
    }
    fclose(f);
    return 0;
}
