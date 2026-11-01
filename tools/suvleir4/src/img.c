/*
 * Suvleir 4 - background image loading.
 * PNG/JPG/BMP/GIF/TGA/PSD/PNM through stb_image, plus PCX (draw.exe's
 * fondo.pcx) and ZX Spectrum .scr screens (6912 bytes) natively.
 * Everything ends up as a 256x192 0x00RRGGBB buffer.
 */
#include "img.h"
#include "vec.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#if defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#endif
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_FAILURE_USERMSG
#include "../third_party/stb_image.h"

static const uint32_t zx_pal[16] = {
    0x000000, 0x0000D7, 0xD70000, 0xD700D7, 0x00D700, 0x00D7D7, 0xD7D700, 0xD7D7D7,
    0x000000, 0x0000FF, 0xFF0000, 0xFF00FF, 0x00FF00, 0x00FFFF, 0xFFFF00, 0xFFFFFF,
};

static int ends_with(const char *s, const char *ext) {
    size_t a = strlen(s), b = strlen(ext), i;
    if (a < b) return 0;
    for (i = 0; i < b; i++)
        if (tolower((unsigned char)s[a - b + i]) != tolower((unsigned char)ext[i])) return 0;
    return 1;
}

/* box/nearest resample of an RGB(A) image into 256x192 */
static void resample(const uint8_t *src, int w, int h, int comp, uint32_t *out) {
    int x, y;
    for (y = 0; y < SCR_H; y++) {
        int y0 = y * h / SCR_H, y1 = (y + 1) * h / SCR_H;
        if (y1 <= y0) y1 = y0 + 1;
        for (x = 0; x < SCR_W; x++) {
            int x0 = x * w / SCR_W, x1 = (x + 1) * w / SCR_W, xx, yy;
            unsigned r = 0, g = 0, b = 0, n = 0;
            if (x1 <= x0) x1 = x0 + 1;
            for (yy = y0; yy < y1; yy++)
                for (xx = x0; xx < x1; xx++) {
                    const uint8_t *p = src + ((size_t)yy * w + xx) * comp;
                    if (comp >= 3) { r += p[0]; g += p[1]; b += p[2]; }
                    else { r += p[0]; g += p[0]; b += p[0]; }
                    n++;
                }
            out[y * SCR_W + x] = ((r / n) << 16) | ((g / n) << 8) | (b / n);
        }
    }
}

static int load_scr(const uint8_t *b, size_t len, uint32_t *out) {
    int x, y;
    if (len < 6144) return -1;
    for (y = 0; y < SCR_H; y++) {
        int base = ((y & 0xC0) << 5) | ((y & 7) << 8) | ((y & 0x38) << 2);
        for (x = 0; x < 32; x++) {
            uint8_t byte = b[base + x];
            uint8_t a = len >= 6912 ? b[6144 + (y >> 3) * 32 + x] : 0x38;
            int br = (a & 64) ? 8 : 0, bit;
            for (bit = 0; bit < 8; bit++)
                out[y * SCR_W + x * 8 + bit] = (byte & (128 >> bit)) ? zx_pal[(a & 7) + br] : zx_pal[((a >> 3) & 7) + br];
        }
    }
    return 0;
}

static int load_pcx(const uint8_t *b, size_t len, uint32_t *out) {
    int w, h, planes, bpl, bpp, x, y, i;
    size_t p = 128;
    uint8_t *raw, *rgb;
    if (len < 128 || b[0] != 0x0A || b[2] != 1) return -1;
    bpp = b[3];
    w = (b[8] | b[9] << 8) - (b[4] | b[5] << 8) + 1;
    h = (b[10] | b[11] << 8) - (b[6] | b[7] << 8) + 1;
    planes = b[65];
    bpl = b[66] | b[67] << 8;
    if (bpp != 8 || (planes != 1 && planes != 3) || w <= 0 || h <= 0) return -1;
    raw = (uint8_t *)calloc((size_t)bpl * planes * h, 1);
    for (i = 0; i < bpl * planes * h && p < len;) {
        uint8_t c = b[p++];
        if ((c & 0xC0) == 0xC0) {
            int n = c & 0x3F;
            uint8_t v = p < len ? b[p++] : 0;
            while (n-- && i < bpl * planes * h) raw[i++] = v;
        } else raw[i++] = c;
    }
    rgb = (uint8_t *)malloc((size_t)w * h * 3);
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++) {
            uint8_t *d = rgb + ((size_t)y * w + x) * 3;
            if (planes == 3) {
                d[0] = raw[(y * 3 + 0) * bpl + x];
                d[1] = raw[(y * 3 + 1) * bpl + x];
                d[2] = raw[(y * 3 + 2) * bpl + x];
            } else {
                uint8_t idx = raw[y * bpl + x];
                const uint8_t *pal = (len >= 769 && b[len - 769] == 12) ? b + len - 768 : NULL;
                if (pal) { d[0] = pal[idx * 3]; d[1] = pal[idx * 3 + 1]; d[2] = pal[idx * 3 + 2]; }
                else d[0] = d[1] = d[2] = idx;
            }
        }
    resample(rgb, w, h, 3, out);
    free(raw); free(rgb);
    return 0;
}

int img_load(const char *path, uint32_t *out, char *msg, size_t msglen) {
    size_t len;
    uint8_t *b = read_file(path, &len);
    int r = -1;
    if (!b) { snprintf(msg, msglen, "can't read %s", path); return -1; }
    if (ends_with(path, ".scr") || len == 6912) {
        r = load_scr(b, len, out);
    } else if (ends_with(path, ".pcx")) {
        r = load_pcx(b, len, out);
    } else {
        int w, h, comp;
        uint8_t *px = stbi_load_from_memory(b, (int)len, &w, &h, &comp, 3);
        if (px) { resample(px, w, h, 3, out); stbi_image_free(px); r = 0; }
        else snprintf(msg, msglen, "can't decode %s: %s", path, stbi_failure_reason());
    }
    free(b);
    if (!r) snprintf(msg, msglen, "background: %s", path);
    else if (!msg[0]) snprintf(msg, msglen, "can't decode %s", path);
    return r;
}

int img_save_bmp(const char *path, const uint32_t *px, int w, int h) {
    FILE *f = fopen(path, "wb");
    int rowpad = (4 - (w * 3) % 4) % 4, x, y;
    uint32_t size = 54 + (uint32_t)(w * 3 + rowpad) * h;
    uint8_t hd[54] = {'B', 'M'};
    if (!f) return -1;
    hd[2] = size; hd[3] = size >> 8; hd[4] = size >> 16; hd[5] = size >> 24;
    hd[10] = 54; hd[14] = 40;
    hd[18] = w; hd[19] = w >> 8; hd[22] = h; hd[23] = h >> 8;
    hd[26] = 1; hd[28] = 24;
    fwrite(hd, 1, 54, f);
    for (y = h - 1; y >= 0; y--) {
        for (x = 0; x < w; x++) {
            uint32_t c = px[y * w + x];
            uint8_t bgr[3] = {(uint8_t)c, (uint8_t)(c >> 8), (uint8_t)(c >> 16)};
            fwrite(bgr, 1, 3, f);
        }
        fwrite("\0\0\0", 1, rowpad, f);
    }
    return fclose(f);
}
