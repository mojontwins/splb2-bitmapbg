/*
 * Suvleir 4 - vector screen model, SVL file format and engine-exact renderer.
 * Copyleft The Mojon Twins.
 *
 * The renderer reproduces, pixel for pixel, what the Z80 side does:
 *   - lines: the Bresenham variant of SPVecDraw (same decisions as ZX Basic's
 *     DRAW: error starts at major/2, minor step when error goes negative,
 *     the start pixel is not plotted);
 *   - fills: splib2's SPPFill: 4-connected flood over unset pixels from the
 *     seed, then every reached pixel takes the pattern bit
 *     pattern[y & 7] >> (7 - (x & 7)).
 */
#include "vec.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* document                                                            */

void vec_init(VecDoc *d) { memset(d, 0, sizeof(*d)); }

void vec_free(VecDoc *d) { free(d->ops); memset(d, 0, sizeof(*d)); }

void vec_clear(VecDoc *d) { d->nops = 0; memset(d->hard, 0, sizeof(d->hard)); }

static void reserve(VecDoc *d, int n) {
    if (n <= d->cap) return;
    int c = d->cap ? d->cap : 256;
    while (c < n) c *= 2;
    d->ops = (VecOp *)realloc(d->ops, (size_t)c * sizeof(VecOp));
    d->cap = c;
}

void vec_copy(VecDoc *dst, const VecDoc *src) {
    reserve(dst, src->nops);
    if (src->nops) memcpy(dst->ops, src->ops, (size_t)src->nops * sizeof(VecOp));
    dst->nops = src->nops;
    memcpy(dst->hard, src->hard, sizeof(dst->hard));
}

void vec_insert(VecDoc *d, int at, VecOp op) {
    reserve(d, d->nops + 1);
    if (at < 0 || at > d->nops) at = d->nops;
    memmove(d->ops + at + 1, d->ops + at, (size_t)(d->nops - at) * sizeof(VecOp));
    d->ops[at] = op;
    d->nops++;
}

void vec_delete(VecDoc *d, int at) {
    if (at < 0 || at >= d->nops) return;
    memmove(d->ops + at, d->ops + at + 1, (size_t)(d->nops - at - 1) * sizeof(VecOp));
    d->nops--;
}

/* ------------------------------------------------------------------ */
/* patterns                                                            */

static const uint8_t default_patterns[8][8] = {
    {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
    {0x77, 0xFF, 0xFF, 0xFF, 0x77, 0xFF, 0xFF, 0xFF},
    {0x77, 0xFF, 0xDD, 0xFF, 0x77, 0xFF, 0xDD, 0xFF},
    {0x55, 0xFF, 0x55, 0xFF, 0x55, 0xFF, 0x55, 0xFF},
    {0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55},
    {0xFF, 0x10, 0x10, 0x10, 0xFF, 0x01, 0x01, 0x01},
    {0x88, 0x00, 0x22, 0x00, 0x88, 0x00, 0x22, 0x00},
    {0x88, 0x00, 0x00, 0x00, 0x88, 0x00, 0x00, 0x00},
};

void vec_default_patterns(VecPatterns *p) {
    memset(p, 0, sizeof(*p));
    memcpy(p->pat, default_patterns, sizeof(default_patterns));
    p->count = 8;
}

int vec_load_patterns(VecPatterns *p, const char *path) {
    size_t len;
    uint8_t *b = read_file(path, &len);
    if (!b) return -1;
    if (len < 8 || len % 8) { free(b); return -2; }
    memset(p, 0, sizeof(*p));
    p->count = (int)(len / 8);
    if (p->count > MAX_PATTERNS) p->count = MAX_PATTERNS;
    memcpy(p->pat, b, (size_t)p->count * 8);
    free(b);
    return 0;
}

int vec_save_patterns(const VecPatterns *p, const char *path) {
    return write_file(path, &p->pat[0][0], (size_t)p->count * 8);
}

/* ------------------------------------------------------------------ */
/* rendering                                                           */

static void plot(uint8_t *pix, int x, int y) {
    if (x >= 0 && x < SCR_W && y >= 0 && y < SCR_H) pix[y * SCR_W + x] = 1;
}

void vec_line(uint8_t *pix, int *pcx, int *pcy, int nx, int ny) {
    int x = *pcx, y = *pcy;
    int dx = nx - x, dy = ny - y, xi = 1, yi = 1, err;
    if (dx < 0) { dx = -dx; xi = -1; }
    if (dy < 0) { dy = -dy; yi = -1; }
    if (dy >= dx) {
        err = dy >> 1;
        while (y != ny) {
            err -= dx;
            if (err < 0) { err += dy; x += xi; }
            y += yi;
            plot(pix, x, y);
        }
    } else {
        err = dx >> 1;
        while (x != nx) {
            err -= dy;
            if (err < 0) { err += dx; y += yi; }
            x += xi;
            plot(pix, x, y);
        }
    }
    *pcx = x; *pcy = y;
}

/* Returns number of pixels filled. mark_out (optional, SCR_W*SCR_H) gets 1
   for every reached pixel, even those the pattern leaves clear. */
int vec_fill(uint8_t *pix, int x, int y, const uint8_t *pattern, uint8_t *mark_out) {
    static int stack[SCR_W * SCR_H];
    static uint8_t mark[SCR_W * SCR_H];
    int sp = 0, n = 0, i;
    if (x < 0 || x >= SCR_W || y < 0 || y >= SCR_H) return 0;
    if (pix[y * SCR_W + x]) return 0;
    memset(mark, 0, sizeof(mark));
    stack[sp++] = y * SCR_W + x;
    mark[y * SCR_W + x] = 1;
    while (sp) {
        int p = stack[--sp];
        int px = p % SCR_W, py = p / SCR_W;
        n++;
        if (px > 0 && !mark[p - 1] && !pix[p - 1]) { mark[p - 1] = 1; stack[sp++] = p - 1; }
        if (px < SCR_W - 1 && !mark[p + 1] && !pix[p + 1]) { mark[p + 1] = 1; stack[sp++] = p + 1; }
        if (py > 0 && !mark[p - SCR_W] && !pix[p - SCR_W]) { mark[p - SCR_W] = 1; stack[sp++] = p - SCR_W; }
        if (py < SCR_H - 1 && !mark[p + SCR_W] && !pix[p + SCR_W]) { mark[p + SCR_W] = 1; stack[sp++] = p + SCR_W; }
    }
    for (i = 0; i < SCR_W * SCR_H; i++) {
        if (mark[i]) {
            int px = i % SCR_W, py = i / SCR_W;
            pix[i] = (uint8_t)((pattern[py & 7] >> (7 - (px & 7))) & 1);
        }
    }
    if (mark_out) memcpy(mark_out, mark, sizeof(mark));
    return n;
}

void vec_render(const VecDoc *d, const VecPatterns *pats, int upto, uint8_t *pix) {
    int cx = 0, cy = 0, i;
    static const uint8_t solid[8] = {255, 255, 255, 255, 255, 255, 255, 255};
    memset(pix, 0, SCR_W * SCR_H);
    if (upto < 0 || upto > d->nops) upto = d->nops;
    for (i = 0; i < upto; i++) {
        const VecOp *o = &d->ops[i];
        switch (o->op) {
        case OP_PLOT: cx = o->x; cy = o->y; plot(pix, cx, cy); break;
        case OP_MOVE: cx = o->x; cy = o->y; break;
        case OP_LINE: vec_line(pix, &cx, &cy, o->x, o->y); break;
        case OP_FILL:
            vec_fill(pix, o->x, o->y, (pats && o->p < pats->count) ? pats->pat[o->p] : solid, NULL);
            break;
        }
    }
}

/* ------------------------------------------------------------------ */
/* SVL format                                                          */

static size_t rle_encode(const uint8_t *hard, uint8_t *out) {
    size_t n = 0;
    int i = 0;
    while (i < HARD_CELLS) {
        int v = hard[i] & 3, run = 1;
        while (i + run < HARD_CELLS && (hard[i + run] & 3) == v && run < 63) run++;
        if (out) out[n] = (uint8_t)((v << 6) | run);
        n++;
        i += run;
    }
    return n;
}

size_t vec_cmd_bytes(const VecDoc *d) {
    size_t n = 0;
    int i;
    for (i = 0; i < d->nops; i++) {
        switch (d->ops[i].op) {
        case OP_LINE: n += 2; break;
        case OP_PLOT: case OP_MOVE: n += 3; break;
        case OP_FILL: n += 4; break;
        }
    }
    return n;
}

size_t vec_hard_bytes(const VecDoc *d) { return rle_encode(d->hard, NULL); }

size_t vec_svl_size(const VecDoc *d) { return 2 + vec_cmd_bytes(d) + 1 + vec_hard_bytes(d); }

size_t vec_svl_encode(const VecDoc *d, uint8_t *out, size_t cap) {
    size_t need = vec_svl_size(d), p = 2;
    int i;
    if (cap < need) return 0;
    for (i = 0; i < d->nops; i++) {
        const VecOp *o = &d->ops[i];
        uint8_t y = o->y < SCR_H ? o->y : SCR_H - 1;
        switch (o->op) {
        case OP_LINE: out[p++] = y; out[p++] = o->x; break;
        case OP_PLOT: out[p++] = SVL_PLOT; out[p++] = o->x; out[p++] = y; break;
        case OP_MOVE: out[p++] = SVL_MOVE; out[p++] = o->x; out[p++] = y; break;
        case OP_FILL: out[p++] = SVL_FILL; out[p++] = o->x; out[p++] = y; out[p++] = o->p; break;
        }
    }
    out[p++] = SVL_END;
    out[0] = (uint8_t)(p & 255);
    out[1] = (uint8_t)(p >> 8);
    p += rle_encode(d->hard, out + p);
    return p;
}

static int rle_decode(uint8_t *hard, const uint8_t *in, size_t len, size_t p) {
    int cell = 0;
    while (cell < HARD_CELLS) {
        int v, run;
        if (p >= len) return -1;
        v = in[p] >> 6; run = in[p] & 63; p++;
        if (run == 0) run = 256;               /* tolerate legacy count 0 */
        while (run-- && cell < HARD_CELLS) hard[cell++] = (uint8_t)v;
    }
    return 0;
}

int vec_svl_decode(VecDoc *d, const uint8_t *in, size_t len, char *err, size_t errlen) {
    size_t p = 2, hoff;
    vec_clear(d);
    if (len < 3) { snprintf(err, errlen, "file too short"); return -1; }
    hoff = in[0] | (in[1] << 8);
    for (;;) {
        VecOp o = {0, 0, 0, 0};
        uint8_t c;
        if (p >= len) { snprintf(err, errlen, "missing END (0xFF) byte"); return -1; }
        c = in[p++];
        if (c == SVL_END) break;
        if (c < SCR_H) {
            if (p + 1 > len) goto trunc;
            o.op = OP_LINE; o.y = c; o.x = in[p++];
        } else if (c == SVL_PLOT || c == SVL_MOVE) {
            if (p + 2 > len) goto trunc;
            o.op = c == SVL_PLOT ? OP_PLOT : OP_MOVE; o.x = in[p]; o.y = in[p + 1]; p += 2;
        } else if (c == SVL_FILL) {
            if (p + 3 > len) goto trunc;
            o.op = OP_FILL; o.x = in[p]; o.y = in[p + 1]; o.p = in[p + 2]; p += 3;
        } else {
            snprintf(err, errlen, "unknown command 0x%02X at offset %u", c, (unsigned)(p - 1));
            return -1;
        }
        vec_insert(d, -1, o);
    }
    if (hoff == 0) return 0;                   /* no hardness section */
    if (hoff != p) { snprintf(err, errlen, "hardness offset %u, commands end at %u", (unsigned)hoff, (unsigned)p); return -1; }
    if (rle_decode(d->hard, in, len, hoff)) { snprintf(err, errlen, "hardness map truncated"); return -1; }
    return 0;
trunc:
    snprintf(err, errlen, "truncated command at end of file");
    return -1;
}

/* ------------------------------------------------------------------ */
/* legacy Suvleir 3                                                    */
/*
 * bytes 0-1: offset L of the hardness layer.
 * commands as SVL (no MOVE, no END).
 * gfxParser.bas parses commands while ptr < L-2 (sic) and starts the RLE where
 * that loop stopped. draw.exe's RLE encoder never writes the 768th cell and
 * it always appends a spurious PLOT 0,0 (an uninitialised op) at the end.
 */
int vec_legacy_decode(VecDoc *d, const uint8_t *in, size_t len, int keep_trailing_plot,
                      char *warn, size_t warnlen) {
    size_t p = 2, L;
    int cell = 0;
    if (warn && warnlen) warn[0] = 0;
    vec_clear(d);
    if (len < 2) return -1;
    L = in[0] | (in[1] << 8);
    if (L < 2 || L > len) return -1;
    while (p + 2 < L + 0 && p < L - 2) {
        VecOp o = {0, 0, 0, 0};
        uint8_t c = in[p++];
        if (c < SCR_H) { o.op = OP_LINE; o.y = c; o.x = in[p++]; }
        else if (c == 192) { o.op = OP_PLOT; o.x = in[p]; o.y = in[p + 1]; p += 2; }
        else if (c == 193) { o.op = OP_FILL; o.x = in[p]; o.y = in[p + 1]; o.p = in[p + 2]; p += 3; }
        else return -1;
        vec_insert(d, -1, o);
    }
    if (p != L && warn) snprintf(warn, warnlen, "legacy quirk: commands ended at %u, header says %u", (unsigned)p, (unsigned)L);
    /* RLE, count 0 = 256 as in the BASIC decoder; cell 767 may be missing */
    while (cell < HARD_CELLS && p < len) {
        int v = in[p] >> 6, run = in[p] & 63;
        p++;
        if (run == 0) run = 256;
        while (run-- && cell < HARD_CELLS) d->hard[cell++] = (uint8_t)v;
    }
    while (cell < HARD_CELLS) { d->hard[cell] = cell ? d->hard[cell - 1] : 0; cell++; }
    if (!keep_trailing_plot && d->nops && d->ops[d->nops - 1].op == OP_PLOT &&
        d->ops[d->nops - 1].x == 0 && d->ops[d->nops - 1].y == 0)
        d->nops--;
    return 0;
}

size_t vec_legacy_encode(const VecDoc *d, uint8_t *out, size_t cap) {
    /* writes what gfxParser.bas can read. MOVE becomes a PLOT (no MOVE there),
       and a dummy PLOT 0,0 is appended so the last real command is never lost
       to the "ptr < L-2" quirk. */
    size_t p = 2;
    int i;
    for (i = 0; i < d->nops; i++) {
        const VecOp *o = &d->ops[i];
        if (p + 8 > cap) return 0;
        switch (o->op) {
        case OP_LINE: out[p++] = o->y; out[p++] = o->x; break;
        case OP_PLOT: case OP_MOVE: out[p++] = 192; out[p++] = o->x; out[p++] = o->y; break;
        case OP_FILL: out[p++] = 193; out[p++] = o->x; out[p++] = o->y; out[p++] = o->p; break;
        }
    }
    out[p++] = 192; out[p++] = 0; out[p++] = 0;
    out[0] = (uint8_t)(p & 255); out[1] = (uint8_t)(p >> 8);
    if (p + HARD_CELLS > cap) return 0;
    p += rle_encode(d->hard, out + p);
    return p;
}

int vec_detect(const uint8_t *in, size_t len) {
    size_t off;
    if (len < 3) return -1;
    off = in[0] | (in[1] << 8);
    if (off >= 3 && off <= len && in[off - 1] == SVL_END) {
        /* make sure the stream really ends at off-1 */
        size_t p = 2;
        while (p < len) {
            uint8_t c = in[p];
            if (c == SVL_END) return p == off - 1 ? 1 : 0;
            if (c < SCR_H) p += 2;
            else if (c == SVL_PLOT || c == SVL_MOVE) p += 3;
            else if (c == SVL_FILL) p += 4;
            else return 0;
        }
        return 0;
    }
    if (off >= 2 && off <= len) return 0;
    return -1;
}

/* ------------------------------------------------------------------ */
/* files                                                               */

uint8_t *read_file(const char *path, size_t *len) {
    FILE *f = fopen(path, "rb");
    uint8_t *b;
    long n;
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    b = (uint8_t *)malloc((size_t)n + 1);
    if (n && fread(b, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(b); return NULL; }
    fclose(f);
    *len = (size_t)n;
    return b;
}

int write_file(const char *path, const uint8_t *data, size_t len) {
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    if (len && fwrite(data, 1, len, f) != len) { fclose(f); return -1; }
    return fclose(f) ? -1 : 0;
}

int vec_load_file(VecDoc *d, const char *path, int force_legacy, char *msg, size_t msglen) {
    size_t len;
    char err[160] = "";
    int kind, r;
    uint8_t *b = read_file(path, &len);
    if (!b) { snprintf(msg, msglen, "can't read %s", path); return -1; }
    kind = force_legacy ? 0 : vec_detect(b, len);
    if (kind == 1) {
        r = vec_svl_decode(d, b, len, err, sizeof(err));
        if (r) snprintf(msg, msglen, "SVL error: %s", err);
        else snprintf(msg, msglen, "loaded %s (SVL, %d ops)", path, d->nops);
    } else if (kind == 0) {
        r = vec_legacy_decode(d, b, len, 0, err, sizeof(err));
        if (r) snprintf(msg, msglen, "not a Suvleir file: %s", path);
        else snprintf(msg, msglen, "imported %s (legacy Suvleir 3, %d ops)%s%s", path, d->nops,
                      err[0] ? " - " : "", err);
    } else {
        r = -1;
        snprintf(msg, msglen, "not a Suvleir file: %s", path);
    }
    free(b);
    return r;
}

int vec_save_file(const VecDoc *d, const char *path, char *msg, size_t msglen) {
    size_t cap = vec_svl_size(d), n;
    uint8_t *b = (uint8_t *)malloc(cap);
    int r;
    n = vec_svl_encode(d, b, cap);
    r = write_file(path, b, n);
    free(b);
    if (r) snprintf(msg, msglen, "can't write %s", path);
    else snprintf(msg, msglen, "saved %s (%u bytes)", path, (unsigned)n);
    return r;
}

static int export_text(const VecDoc *d, const char *path, const char *label, int asm_style) {
    size_t cap = vec_svl_size(d), n, i;
    uint8_t *b = (uint8_t *)malloc(cap);
    FILE *f;
    n = vec_svl_encode(d, b, cap);
    f = fopen(path, "w");
    if (!f) { free(b); return -1; }
    if (asm_style) {
        fprintf(f, "; Suvleir 4 vector screen (SVL), %u bytes\n._%s\n", (unsigned)n, label);
    } else {
        fprintf(f, "/* Suvleir 4 vector screen (SVL), %u bytes */\nunsigned char %s[%u] = {\n", (unsigned)n, label, (unsigned)n);
    }
    for (i = 0; i < n; i++) {
        if (i % 16 == 0) fputs(asm_style ? "\tdefb " : "\t", f);
        fprintf(f, asm_style ? "%u" : "%u,", b[i]);
        if (i % 16 == 15 || i == n - 1) fputc('\n', f);
        else if (asm_style) fputc(',', f);
    }
    if (!asm_style) fputs("};\n", f);
    fclose(f);
    free(b);
    return 0;
}

int vec_export_asm(const VecDoc *d, const char *path, const char *label) { return export_text(d, path, label, 1); }
int vec_export_c(const VecDoc *d, const char *path, const char *label) { return export_text(d, path, label, 0); }
