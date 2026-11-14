/* roomed - room model */
#include "rooms.h"
#include "img.h"
#include "../third_party/font8x8_basic.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

const uint32_t zx_pal[16] = {
    0x000000, 0x0000D7, 0xD70000, 0xD700D7, 0x00D700, 0x00D7D7, 0xD7D700, 0xD7D7D7,
    0x000000, 0x0000FF, 0xFF0000, 0xFF00FF, 0x00FF00, 0x00FFFF, 0xFFFF00, 0xFFFFFF,
};

static const char *type_names[] = { "char", "rect", "pattern" };

const CustObj *proj_cust(const Project *p, int type) {
    int i;
    for (i = 0; i < p->ncust; i++) if (p->cust[i].type == type) return &p->cust[i];
    return NULL;
}

const char *obj_name(const Project *p, const Obj *o) {
    const CustObj *c;
    if (o->type <= OBJ_PATTERN) return type_names[o->type];
    if (o->type == OBJ_ZIGZAG) return "zigzag";
    c = proj_cust(p, o->type);
    return c ? c->name : "?";
}

void room_init(Room *r, int index) {
    memset(r, 0, sizeof(*r));
    snprintf(r->name, sizeof(r->name), "room %d", index);
}

/* neutral defaults: a whole Spectrum screen, 4 header bytes, BRIGHT = solid */
void proj_init(Project *p) {
    int i;
    memset(p, 0, sizeof(*p));
    p->sw = 32; p->sh = 24;
    p->ax = 0; p->ay = 0; p->aw = 32; p->ah = 24;
    p->hdr_n = 4;
    p->solid_bit = 6;
    snprintf(p->export_name, sizeof(p->export_name), "rooms");
    snprintf(p->tileset, sizeof(p->tileset), "tileset.png");
    for (i = 0; i < MAX_BG; i++) p->bg_params[i] = -1;
    p->nrooms = 1;
    room_init(&p->rooms[0], 0);
}

static void clamp_settings(Project *p) {
    if (p->sw < 1) p->sw = 1;
    if (p->sw > MAX_SW) p->sw = MAX_SW;
    if (p->sh < 1) p->sh = 1;
    if (p->sh > MAX_SH) p->sh = MAX_SH;
    if (p->ax < 0) p->ax = 0;
    if (p->ay < 0) p->ay = 0;
    if (p->ax > p->sw - 1) p->ax = p->sw - 1;
    if (p->ay > p->sh - 1) p->ay = p->sh - 1;
    if (p->aw < 1) p->aw = 1;
    if (p->ah < 1) p->ah = 1;
    if (p->ax + p->aw > p->sw) p->aw = p->sw - p->ax;
    if (p->ay + p->ah > p->sh) p->ah = p->sh - p->ay;
    if (p->hdr_n < 0) p->hdr_n = 0;
    if (p->hdr_n > MAX_HDR) p->hdr_n = MAX_HDR;
    if (p->solid_bit < -1 || p->solid_bit > 7) p->solid_bit = -1;
}

void obj_normalize(Obj *o) {
    uint8_t t;
    if (o->type == OBJ_CHAR) { o->x2 = o->x; o->y2 = o->y; o->w = o->h = 1; return; }
    if (IS_CUST(o->type)) {
        if (!o->w) o->w = 1;
        if (!o->h) o->h = 1;
        o->x2 = (uint8_t)(o->x + o->w - 1); o->y2 = (uint8_t)(o->y + o->h - 1);
        return;
    }
    if (o->x2 < o->x) { t = o->x; o->x = o->x2; o->x2 = t; }
    if (o->y2 < o->y) { t = o->y; o->y = o->y2; o->y2 = t; }
    if (o->type == OBJ_RECT) { o->w = o->h = 1; }
    if (o->type == OBJ_ZIGZAG) { o->w = 2; o->h = 1; }
    if (!o->w) o->w = 1;
    if (!o->h) o->h = 1;
}

/* ------------------------------------------------------------------ */
/* rooms.txt                                                           */

static int nums(const char *s, int *v, int max) {
    int n = 0;
    char *e;
    while (n < max) {
        long x;
        while (*s && isspace((unsigned char)*s)) s++;
        if (!*s || *s == '#') break;
        x = strtol(s, &e, 0);
        if (e == s) return -1;
        v[n++] = (int)x;
        s = e;
    }
    return n;
}

int proj_load(Project *p, const char *path, char *err, int errlen) {
    FILE *f = fopen(path, "r");
    char line[1024];
    int ln = 0, inroom = 0;
    Room *r = NULL;
    if (!f) { snprintf(err, errlen, "can't open %s", path); return -1; }
    proj_init(p);
    p->nrooms = 0;
    while (fgets(line, sizeof(line), f)) {
        char kw[32] = "", *s = line;
        int v[16], n, i;
        ln++;
        line[strcspn(line, "\r\n")] = 0;
        while (*s && isspace((unsigned char)*s)) s++;
        if (!*s || *s == '#') continue;
        for (i = 0; *s && !isspace((unsigned char)*s) && i < 31; i++) kw[i] = *s++;
        kw[i] = 0;
        while (*s && isspace((unsigned char)*s)) s++;
        if (!strcmp(kw, "tileset")) { snprintf(p->tileset, sizeof(p->tileset), "%.511s", s); continue; }
        if (!strcmp(kw, "background")) {
            /* "background FILE" (every type) or "background N FILE" (type N) */
            char *e;
            long t = strtol(s, &e, 10);
            if (e != s && isspace((unsigned char)*e) && t >= 0 && t < MAX_BG) {
                while (*e && isspace((unsigned char)*e)) e++;
                snprintf(p->bg_type[t], sizeof(p->bg_type[0]), "%.511s", e);
            } else snprintf(p->background, sizeof(p->background), "%.511s", s);
            continue;
        }
        if (!strcmp(kw, "export")) { snprintf(p->export_name, sizeof(p->export_name), "%.31s", s); continue; }
        if (!strcmp(kw, "object")) {
            /* object TYPE NAME W H [solid] [number] */
            CustObj c;
            char nm[64] = "", fl[16] = "", fl2[16] = "";
            int t, w, hh, k = sscanf(s, "%d %63s %d %d %15s %15s", &t, nm, &w, &hh, fl, fl2);
            if (k < 4 || t < OBJ_CUSTOM || t > 253 || w < 1 || hh < 1 || w > 32 || hh > 32) goto bad;
            if (p->ncust == MAX_CUST) { snprintf(err, errlen, "line %d: too many object types", ln); fclose(f); return -1; }
            if (!strcmp(nm, "char") || !strcmp(nm, "rect") || !strcmp(nm, "pattern") || !strcmp(nm, "room") ||
                !strcmp(nm, "zigzag") ||
                !strcmp(nm, "end") || !strcmp(nm, "header") || proj_cust(p, t)) goto bad;
            memset(&c, 0, sizeof(c));
            c.type = t; c.w = w; c.h = hh;
            c.solid = !strcmp(fl, "solid") || !strcmp(fl2, "solid");
            c.number = !strcmp(fl, "number") || !strcmp(fl2, "number");
            snprintf(c.name, sizeof(c.name), "%.23s", nm);
            p->cust[p->ncust++] = c;
            continue;
        }
        if (!strcmp(kw, "area")) {
            if (nums(s, v, 4) != 4) goto bad;
            p->ax = v[0]; p->ay = v[1]; p->aw = v[2]; p->ah = v[3];
            continue;
        }
        if (!strcmp(kw, "screen")) {
            if (nums(s, v, 2) != 2) goto bad;
            p->sw = v[0]; p->sh = v[1];
            continue;
        }
        if (!strcmp(kw, "header_bytes")) { if (nums(s, v, 1) != 1) goto bad; p->hdr_n = v[0]; continue; }
        if (!strcmp(kw, "encoding")) {
            if (!strcmp(s, "compact")) p->compact = 1;
            else if (!strcmp(s, "plain")) p->compact = 0;
            else goto bad;
            continue;
        }
        if (!strcmp(kw, "bg_params")) {
            if (nums(s, v, 2) != 2 || v[0] < 0 || v[0] >= MAX_BG || v[1] < 0 || v[1] >= MAX_HDR) goto bad;
            p->bg_params[v[0]] = v[1];
            continue;
        }
        if (!strcmp(kw, "solid_bit")) { if (nums(s, v, 1) != 1) goto bad; p->solid_bit = v[0]; continue; }
        if (!strcmp(kw, "room")) {
            if (p->nrooms == MAX_ROOMS) { snprintf(err, errlen, "line %d: too many rooms", ln); fclose(f); return -1; }
            r = &p->rooms[p->nrooms];
            room_init(r, p->nrooms);
            if (*s) snprintf(r->name, sizeof(r->name), "%.63s", s);
            p->nrooms++;
            inroom = 1;
            continue;
        }
        if (!inroom) goto bad;
        if (!strcmp(kw, "end")) { inroom = 0; continue; }
        if (!strcmp(kw, "header")) {
            n = nums(s, v, MAX_HDR);
            if (n < 0) goto bad;
            for (i = 0; i < n; i++) r->hdr[i] = (uint8_t)v[i];
            continue;
        }
        {
            int type = !strcmp(kw, "char") ? OBJ_CHAR : !strcmp(kw, "rect") ? OBJ_RECT :
                       !strcmp(kw, "pattern") ? OBJ_PATTERN : !strcmp(kw, "zigzag") ? OBJ_ZIGZAG : -1;
            int need = type == OBJ_CHAR ? 4 : (type == OBJ_RECT || type == OBJ_ZIGZAG) ? 6 : 8;
            const CustObj *cu = NULL;
            Obj o;
            if (type < 0) {
                for (i = 0; i < p->ncust; i++) if (!strcmp(kw, p->cust[i].name)) cu = &p->cust[i];
                if (!cu) goto bad;
                type = cu->type; need = 4;
            }
            n = nums(s, v, 8);
            if (n != need) goto bad;
            for (i = 0; i < n; i++) if (v[i] < 0 || v[i] > 255) goto bad;
            if (r->n == MAX_OBJ) { snprintf(err, errlen, "line %d: too many objects in a room", ln); fclose(f); return -1; }
            memset(&o, 0, sizeof(o));
            o.type = (uint8_t)type; o.x = v[0]; o.y = v[1]; o.code = v[2]; o.attr = v[3];
            if (IS_AREA(type)) { o.x2 = v[4]; o.y2 = v[5]; }
            if (type == OBJ_PATTERN) { o.w = v[6]; o.h = v[7]; }
            if (cu) { o.w = (uint8_t)cu->w; o.h = (uint8_t)cu->h; }
            obj_normalize(&o);
            r->o[r->n++] = o;
        }
        continue;
    bad:
        snprintf(err, errlen, "line %d: can't read \"%s\"", ln, line);
        fclose(f);
        return -1;
    }
    fclose(f);
    clamp_settings(p);
    if (!p->nrooms) { p->nrooms = 1; room_init(&p->rooms[0], 0); }
    return 0;
}

int proj_save(const Project *p, const char *path) {
    FILE *f = fopen(path, "w");
    int i, j;
    if (!f) return -1;
    fprintf(f, "# rooms - written by roomed\n");
    fprintf(f, "#   char X Y CODE ATTR | rect X Y CODE ATTR X2 Y2 | pattern X Y CODE ATTR X2 Y2 W H\n");
    fprintf(f, "#   zigzag X Y CODE ATTR X2 Y2 (tiles CODE, CODE+1 taking turns, rows staggered)\n");
    fprintf(f, "screen %d %d\narea %d %d %d %d\nheader_bytes %d\nsolid_bit %d\nencoding %s\n",
            p->sw, p->sh, p->ax, p->ay, p->aw, p->ah, p->hdr_n, p->solid_bit, p->compact ? "compact" : "plain");
    for (i = 0; i < MAX_BG; i++) if (p->bg_params[i] >= 0) fprintf(f, "bg_params %d %d\n", i, p->bg_params[i]);
    fprintf(f, "export %s\n", p->export_name);
    for (i = 0; i < p->ncust; i++)
        fprintf(f, "object %d %s %d %d%s%s\n", p->cust[i].type, p->cust[i].name, p->cust[i].w, p->cust[i].h,
                p->cust[i].solid ? " solid" : "", p->cust[i].number ? " number" : "");
    fprintf(f, "tileset %s\n", p->tileset);
    if (p->background[0]) fprintf(f, "background %s\n", p->background);
    for (i = 0; i < MAX_BG; i++) if (p->bg_type[i][0]) fprintf(f, "background %d %s\n", i, p->bg_type[i]);
    for (i = 0; i < p->nrooms; i++) {
        const Room *r = &p->rooms[i];
        fprintf(f, "\nroom %s\n", r->name);
        if (p->hdr_n) {
            fprintf(f, "header");
            for (j = 0; j < p->hdr_n; j++) fprintf(f, " %d", r->hdr[j]);
            fprintf(f, "\n");
        }
        for (j = 0; j < r->n; j++) {
            const Obj *o = &r->o[j];
            if (o->type == OBJ_CHAR) fprintf(f, "char %d %d %d %d\n", o->x, o->y, o->code, o->attr);
            else if (o->type == OBJ_RECT) fprintf(f, "rect %d %d %d %d %d %d\n", o->x, o->y, o->code, o->attr, o->x2, o->y2);
            else if (o->type == OBJ_PATTERN) fprintf(f, "pattern %d %d %d %d %d %d %d %d\n", o->x, o->y, o->code, o->attr, o->x2, o->y2, o->w, o->h);
            else if (o->type == OBJ_ZIGZAG) fprintf(f, "zigzag %d %d %d %d %d %d\n", o->x, o->y, o->code, o->attr, o->x2, o->y2);
            else fprintf(f, "%s %d %d %d %d\n", obj_name(p, o), o->x, o->y, o->code, o->attr);
        }
        fprintf(f, "end\n");
    }
    return fclose(f) ? -1 : 0;
}

static int header_len(const Project *p, const Room *r) {
    int t = r->hdr[0];
    if (!p->compact || !p->hdr_n) return p->hdr_n;
    if (t < MAX_BG && p->bg_params[t] >= 0) return 1 + p->bg_params[t];
    return p->hdr_n;
}

int room_encode(const Project *p, const Room *r, uint8_t *out) {
    int n = 0, j, hl = header_len(p, r);
    for (j = 0; j < hl; j++) out[n++] = r->hdr[j];
    for (j = 0; j < r->n; j++) {
        const Obj *o = &r->o[j];
        out[n++] = o->x; out[n++] = o->y;
        if (p->compact && o->type == OBJ_PATTERN) {
            int t = ((o->w - 1) & 7) << 3 | ((o->h - 1) & 7);
            int single = o->x2 - o->x + 1 == o->w && o->y2 - o->y + 1 == o->h;
            out[n++] = (uint8_t)((single ? 0x80 : 0x40) | t);
            out[n++] = o->code; out[n++] = o->attr;
            if (!single) { out[n++] = o->x2; out[n++] = o->y2; }
            continue;
        }
        if (o->type == OBJ_ZIGZAG) {
            out[n++] = p->compact ? OBJ_PATTERN : OBJ_ZIGZAG_PLAIN;
            out[n++] = o->code; out[n++] = o->attr; out[n++] = o->x2; out[n++] = o->y2;
            continue;
        }
        out[n++] = o->type; out[n++] = o->code; out[n++] = o->attr;
        if (o->type == OBJ_RECT || o->type == OBJ_PATTERN) { out[n++] = o->x2; out[n++] = o->y2; }
        if (o->type == OBJ_PATTERN) { out[n++] = o->w; out[n++] = o->h; }
    }
    out[n++] = OBJ_END;
    return n;
}

const char *proj_check(const Project *p, int *room, int *obj) {
    int i, j;
    *room = *obj = -1;
    if (!p->compact) return NULL;
    for (i = 0; i < p->ncust; i++)
        if (p->cust[i].type > 63) { *room = -1; return "custom object types must be 3-63 in the compact encoding"; }
    for (i = 0; i < p->nrooms; i++)
        for (j = 0; j < p->rooms[i].n; j++) {
            const Obj *o = &p->rooms[i].o[j];
            if (o->type == OBJ_PATTERN && (o->w > 8 || o->h > 8)) {
                *room = i; *obj = j;
                return "patterns can be at most 8x8 in the compact encoding";
            }
        }
    return NULL;
}

int room_bytes(const Project *p, const Room *r) {
    static uint8_t buf[MAX_HDR + MAX_OBJ * 9 + 1];
    return room_encode(p, r, buf);
}

int room_export_bin(const Project *p, const Room *r, const char *path) {
    static uint8_t buf[MAX_HDR + MAX_OBJ * 9 + 1];
    int n = room_encode(p, r, buf);
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    fwrite(buf, 1, n, f);
    return fclose(f) ? -1 : 0;
}

/* C: NAME_data [] (all rooms one after another) and NAME_off [] (where
   each one starts), NUM_NAME. Same layout as CJMC's tools/conv.py. */
int proj_export_c(const Project *p, const char *path) {
    static uint8_t buf[MAX_HDR + MAX_OBJ * 9 + 1];
    FILE *f = fopen(path, "w");
    const char *nm = p->export_name[0] ? p->export_name : "rooms";
    char up[32];
    int i, j, off = 0, n;
    if (!f) return -1;
    for (i = 0; nm[i] && i < 31; i++) up[i] = (char)toupper((unsigned char)nm[i]);
    up[i] = 0;
    fprintf(f, "/* %s - exported by roomed: room n starts at %s_data + %s_off [n] */\n", nm, nm, nm);
    fprintf(f, "#define NUM_%s %d\n", up, p->nrooms);
    fprintf(f, "unsigned char %s_data [] = {\n", nm);
    for (i = 0; i < p->nrooms; i++) {
        n = room_encode(p, &p->rooms[i], buf);
        fprintf(f, "\t/* %d: %s */\n\t", i, p->rooms[i].name);
        for (j = 0; j < n; j++) fprintf(f, "%d,%s", buf[j], (j % 16 == 15 && j + 1 < n) ? "\n\t" : "");
        fprintf(f, "\n");
    }
    fprintf(f, "};\nunsigned int %s_off [] = { ", nm);
    for (i = 0; i < p->nrooms; i++) {
        fprintf(f, "%s%d", i ? ", " : "", off);
        off += room_bytes(p, &p->rooms[i]);
    }
    fprintf(f, " };\n");
    return fclose(f) ? -1 : 0;
}

/* assembler: NAME_data (defb), NAME_off (defw offsets), NAME_N one label per
   room. "label:" syntax (pasmo, sjasmplus, z80asm 2.x...). */
int proj_export_asm(const Project *p, const char *path) {
    static uint8_t buf[MAX_HDR + MAX_OBJ * 9 + 1];
    FILE *f = fopen(path, "w");
    const char *nm = p->export_name[0] ? p->export_name : "rooms";
    int i, j, off = 0, n;
    if (!f) return -1;
    fprintf(f, "; %s - exported by roomed. Room n starts at %s_data + %s_off[n]\n", nm, nm, nm);
    fprintf(f, "%s_count: equ %d\n\n%s_off:\n", nm, p->nrooms, nm);
    for (i = 0; i < p->nrooms; i++) { fprintf(f, "    defw %d\n", off); off += room_bytes(p, &p->rooms[i]); }
    fprintf(f, "\n%s_data:\n", nm);
    for (i = 0; i < p->nrooms; i++) {
        n = room_encode(p, &p->rooms[i], buf);
        fprintf(f, "%s_%d:    ; %s\n", nm, i, p->rooms[i].name);
        for (j = 0; j < n; j++) {
            fprintf(f, "%s%d", j % 16 ? ", " : "    defb ", buf[j]);
            if (j % 16 == 15 || j + 1 == n) fputc('\n', f);
        }
    }
    return fclose(f) ? -1 : 0;
}

void proj_path(const char *projfile, const char *rel, char *out, int outlen) {
    const char *b = projfile, *s;
    if (!rel[0] || rel[0] == '/' || rel[0] == '\\' || (rel[0] && rel[1] == ':') || !projfile || !projfile[0]) {
        snprintf(out, outlen, "%s", rel);
        return;
    }
    for (s = projfile; *s; s++) if (*s == '/' || *s == '\\') b = s + 1;
    snprintf(out, outlen, "%.*s%s", (int)(b - projfile), projfile, rel);
}

/* ------------------------------------------------------------------ */
/* tileset                                                             */

static int colour_attr(uint32_t c) {
    int r = (c >> 16) & 255, g = (c >> 8) & 255, b = c & 255, mx = r > g ? r : g;
    if (b > mx) mx = b;
    return (b > 100 ? 1 : 0) | (r > 100 ? 2 : 0) | (g > 100 ? 4 : 0) | (mx > 230 ? 64 : 0);
}

int tileset_load(Tileset *t, const char *path) {
    int w, h, i, x, y, per_row;
    uint32_t *px = img_load_rgb(path, &w, &h);
    if (!px) return -1;
    memset(t, 0, sizeof(*t));
    per_row = w / 8;
    t->count = per_row * (h / 8);
    if (t->count > 256) t->count = 256;
    for (i = 0; i < 256; i++) {
        int tx = per_row ? (i % per_row) * 8 : 0, ty = per_row ? (i / per_row) * 8 : 0;
        t->ink[i] = 7;
        if (i >= t->count) continue;
        for (y = 0; y < 8; y++)
            for (x = 0; x < 8; x++) {
                uint32_t c = px[(ty + y) * w + tx + x];
                int r = (c >> 16) & 255, g = (c >> 8) & 255, b = c & 255;
                if (r > 48 || g > 48 || b > 48) {
                    t->bits[i][y] |= (uint8_t)(128 >> x);
                    if (!t->used[i]) t->ink[i] = (uint8_t)colour_attr(c);
                    t->used[i] = 1;
                }
            }
    }
    free(px);
    t->loaded = 1;
    return 0;
}

/* ------------------------------------------------------------------ */
/* rendering, exactly like room_fill () in the game                    */

void obj_cell(const Obj *o, int cx, int cy, int *code, int *attr) {
    int i = (cx - o->x) % (o->w ? o->w : 1), j = (cy - o->y) % (o->h ? o->h : 1);
    if (o->type == OBJ_ZIGZAG) {
        *code = (o->code + ((cx - o->x + cy - o->y) & 1)) & 255;
        *attr = o->attr;
        return;
    }
    *code = (o->code + j * o->w + i) & 255;
    *attr = o->attr;
}

uint32_t zx_nearest(uint32_t c) {
    int i, best = 0;
    long bd = -1;
    for (i = 0; i < 16; i++) {
        long dr = (long)((c >> 16) & 255) - (long)((zx_pal[i] >> 16) & 255);
        long dg = (long)((c >> 8) & 255) - (long)((zx_pal[i] >> 8) & 255);
        long db = (long)(c & 255) - (long)(zx_pal[i] & 255);
        long d = dr * dr + dg * dg + db * db;
        if (bd < 0 || d < bd) { bd = d; best = i; }
    }
    return zx_pal[best];
}

const char *proj_backdrop_default(const Project *p) { return p->background; }

const char *proj_backdrop(const Project *p, const Room *r) {
    static char out[600];
    const char *s;
    int t = r ? r->hdr[0] : 0, n = 0;
    if (!(p->hdr_n > 0 && t < MAX_BG && p->bg_type[t][0])) return p->background;
    for (s = p->bg_type[t]; *s && n < (int)sizeof(out) - 4; s++) {
        if (s[0] == '%' && s[1] >= '1' && s[1] <= '7') {
            n += snprintf(out + n, sizeof(out) - n, "%d", r ? r->hdr[s[1] - '0'] : 0);
            s++;
        } else out[n++] = *s;
    }
    out[n] = 0;
    return out;
}

int backdrop_load(const Project *p, const char *full, uint32_t *out) {
    int w, h, x, y, W = p->sw * 8, H = p->sh * 8;
    uint32_t *px = img_load_rgb(full, &w, &h);
    if (!px) return -1;
    for (y = 0; y < H; y++)
        for (x = 0; x < W; x++)
            out[y * W + x] = (x < w && y < h) ? zx_nearest(px[y * w + x]) : 0;
    free(px);
    return 0;
}

void room_render(const Project *p, const Room *r, const Tileset *t, const uint32_t *bg,
                 uint32_t *out, uint8_t *coll, int upto) {
    int k, cx, cy, y, b, W = p->sw * 8, H = p->sh * 8;
    int solid = p->solid_bit >= 0 ? 1 << p->solid_bit : 0;
    if (bg) memcpy(out, bg, (size_t)W * H * 4);
    else memset(out, 0, (size_t)W * H * 4);
    if (coll) memset(coll, 0, (size_t)p->sw * p->sh);
    if (upto < 0 || upto > r->n) upto = r->n;
    for (k = 0; k < upto; k++) {
        const Obj *o = &r->o[k];
        const CustObj *cu = IS_CUST(o->type) ? proj_cust(p, o->type) : NULL;
        if (cu && cu->number) {
            /* a box with the two values: CODE on top, ATTR below */
            int x0 = o->x * 8, y0 = o->y * 8, x1 = (o->x2 + 1) * 8 - 1, y1 = (o->y2 + 1) * 8 - 1, line, i;
            char txt[2][8];
            snprintf(txt[0], sizeof(txt[0]), "%d", o->code);
            snprintf(txt[1], sizeof(txt[1]), "%d", o->attr);
            for (y = y0; y <= y1 && y < H; y++)
                for (b = x0; b <= x1 && b < W; b++)
                    if (y == y0 || y == y1 || b == x0 || b == x1) out[y * W + b] = 0xFF00FF;
            for (line = 0; line < 2; line++)
                for (i = 0; txt[line][i] && x0 + 2 + i * 8 + 7 < W; i++) {
                    const unsigned char *g = font8x8_basic[(unsigned char)txt[line][i]];
                    int gy, gx, ty = y0 + 1 + line * 7;
                    for (gy = 0; gy < 7 && ty + gy < H && ty + gy < y1; gy++)
                        for (gx = 0; gx < 8; gx++)
                            if (g[gy] & (1 << gx)) out[(ty + gy) * W + x0 + 2 + i * 6 + gx] = line ? 0x00FFFF : 0xFFFFFF;
                }
            continue;
        }
        for (cy = o->y; cy <= o->y2 && cy < p->sh; cy++)
            for (cx = o->x; cx <= o->x2 && cx < p->sw; cx++) {
                int code, attr;
                uint32_t ink, paper;
                obj_cell(o, cx, cy, &code, &attr);
                ink = zx_pal[(attr & 7) + ((attr & 64) ? 8 : 0)];
                paper = zx_pal[((attr >> 3) & 7) + ((attr & 64) ? 8 : 0)];
                for (y = 0; y < 8; y++) {
                    uint8_t bits = t ? t->bits[code][y] : 0;
                    uint32_t *q = out + (cy * 8 + y) * W + cx * 8;
                    for (b = 0; b < 8; b++) q[b] = (bits & (128 >> b)) ? ink : paper;
                }
                if (coll && ((attr & solid) || (IS_CUST(o->type) && proj_cust(p, o->type) && proj_cust(p, o->type)->solid)))
                    coll[cy * p->sw + cx] = 1;
            }
    }
}

int room_hit(const Room *r, int cx, int cy) {
    int k;
    for (k = r->n - 1; k >= 0; k--) {
        const Obj *o = &r->o[k];
        if (cx >= o->x && cx <= o->x2 && cy >= o->y && cy <= o->y2) return k;
    }
    return -1;
}
