/* Beeptune - 24-bit BMP writer */
#include "bmp.h"
#include <stdio.h>
#include <string.h>

static void le32(uint8_t *p, uint32_t v) { p[0] = v; p[1] = v >> 8; p[2] = v >> 16; p[3] = v >> 24; }

int bmp_save(const char *path, const uint32_t *px, int w, int h) {
    FILE *f = fopen(path, "wb");
    int stride = (w * 3 + 3) & ~3, x, y;
    uint8_t hd[54], row[4096 * 3];
    if (!f || w > 4096) { if (f) fclose(f); return 1; }
    memset(hd, 0, sizeof(hd));
    hd[0] = 'B'; hd[1] = 'M'; le32(hd + 2, 54 + stride * h); le32(hd + 10, 54);
    le32(hd + 14, 40); le32(hd + 18, w); le32(hd + 22, h); hd[26] = 1; hd[28] = 24;
    le32(hd + 34, stride * h);
    fwrite(hd, 1, 54, f);
    for (y = h - 1; y >= 0; y--) {
        memset(row, 0, stride);
        for (x = 0; x < w; x++) {
            uint32_t c = px[y * w + x];
            row[x * 3] = c & 255; row[x * 3 + 1] = (c >> 8) & 255; row[x * 3 + 2] = (c >> 16) & 255;
        }
        fwrite(row, 1, stride, f);
    }
    return fclose(f) != 0;
}
