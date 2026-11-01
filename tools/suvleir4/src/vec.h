/*
 * Suvleir 4 - vector screen model, SVL file format and engine-exact renderer.
 * Copyleft The Mojon Twins. See ../README.md and ../docs/FORMAT.md
 */
#ifndef VEC_H
#define VEC_H

#include <stdint.h>
#include <stddef.h>

#define SCR_W 256
#define SCR_H 192
#define HARD_W 32
#define HARD_H 24
#define HARD_CELLS (HARD_W * HARD_H)
#define MAX_PATTERNS 16

/* SVL command bytes */
#define SVL_PLOT  0xC0   /* C0 x y   : set pixel, pen = (x,y)            */
#define SVL_FILL  0xC1   /* C1 x y p : patterned flood fill with pattern p */
#define SVL_MOVE  0xC2   /* C2 x y   : pen = (x,y), nothing drawn          */
#define SVL_END   0xFF   /* end of the command stream                     */
                         /* y x (y < 192) : line from pen to (x,y)        */

enum { OP_PLOT = 0, OP_LINE = 1, OP_FILL = 2, OP_MOVE = 3 };

typedef struct {
    uint8_t op, x, y, p;
} VecOp;

typedef struct {
    VecOp  *ops;
    int     nops, cap;
    uint8_t hard[HARD_CELLS];      /* 0..3 per 8x8 cell, row major */
} VecDoc;

typedef struct {
    uint8_t pat[MAX_PATTERNS][8];  /* fill patterns, as the engine has them */
    int     count;
} VecPatterns;

/* document */
void  vec_init(VecDoc *d);
void  vec_free(VecDoc *d);
void  vec_clear(VecDoc *d);
void  vec_copy(VecDoc *dst, const VecDoc *src);
void  vec_insert(VecDoc *d, int at, VecOp op);      /* at = -1: append */
void  vec_delete(VecDoc *d, int at);

/* patterns */
void  vec_default_patterns(VecPatterns *p);          /* the Lhikoj/Suvleir 3 set */
int   vec_load_patterns(VecPatterns *p, const char *path);  /* 8*n bytes .bin   */
int   vec_save_patterns(const VecPatterns *p, const char *path);

/* rendering: pix is SCR_W*SCR_H bytes, 0/1. Renders ops [0, upto). */
void  vec_render(const VecDoc *d, const VecPatterns *pats, int upto, uint8_t *pix);
/* the same primitives the engine uses */
void  vec_line(uint8_t *pix, int *cx, int *cy, int nx, int ny);
int   vec_fill(uint8_t *pix, int x, int y, const uint8_t *pattern, uint8_t *mark_out);

/* SVL (Suvleir 4) format */
size_t vec_svl_size(const VecDoc *d);
size_t vec_svl_encode(const VecDoc *d, uint8_t *out, size_t cap);
int    vec_svl_decode(VecDoc *d, const uint8_t *in, size_t len, char *err, size_t errlen);

/* legacy Suvleir 3 (draw.exe / gfxParser.bas) format */
int    vec_legacy_decode(VecDoc *d, const uint8_t *in, size_t len, int keep_trailing_plot,
                         char *warn, size_t warnlen);
size_t vec_legacy_encode(const VecDoc *d, uint8_t *out, size_t cap);

/* guess: 1 = SVL, 0 = legacy, -1 = unknown */
int    vec_detect(const uint8_t *in, size_t len);

/* file helpers */
int    vec_load_file(VecDoc *d, const char *path, int force_legacy, char *msg, size_t msglen);
int    vec_save_file(const VecDoc *d, const char *path, char *msg, size_t msglen);
int    vec_export_asm(const VecDoc *d, const char *path, const char *label);
int    vec_export_c(const VecDoc *d, const char *path, const char *label);

/* byte budget */
size_t vec_cmd_bytes(const VecDoc *d);
size_t vec_hard_bytes(const VecDoc *d);

uint8_t *read_file(const char *path, size_t *len);
int      write_file(const char *path, const uint8_t *data, size_t len);

#endif
