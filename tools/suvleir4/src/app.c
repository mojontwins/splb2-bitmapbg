/*
 * Suvleir 4 - the editor.
 * Copyleft The Mojon Twins.
 */
#include "app.h"
#include "vec.h"
#include "img.h"
#include "plat.h"
#include "ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <sys/stat.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <direct.h>
#define getcwd _getcwd
#define PATHSEP '\\'
#else
#include <dirent.h>
#include <strings.h>
#include <unistd.h>
#define PATHSEP '/'
#endif
#ifndef S_ISDIR
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#endif

#define WIN_W 1040
#define WIN_H 664
#define VX 8            /* canvas viewport */
#define VY 38
#define VW 768
#define VH 576
#define PX 784          /* side panel */
#define PW 248
#define SB_Y (WIN_H - 22)
#define MAX_UNDO 100

static uint32_t fb[WIN_W * WIN_H];

/* ------------------------------------------------------------------ */
/* state                                                               */

enum { T_LINE, T_FILL, T_HARD, T_EDIT };
enum { M_NONE, M_CONFIRM, M_FILE, M_HELP };
enum { A_NONE, A_NEW, A_OPEN, A_QUIT, A_OPEN_PATH };
enum { F_OPEN, F_SAVE, F_BG, F_PAT_LOAD, F_PAT_SAVE, F_ASM, F_C, F_LEGACY };

static VecDoc doc;
static VecPatterns pats;
static VecDoc undo_stack[MAX_UNDO], redo_stack[MAX_UNDO];
static int undo_n, redo_n;

static char filename[1024];
static int modified, running = 1;

static uint32_t bgimg[SCR_W * SCR_H];
static int has_bg;
static char bgname[1024];

static int zoom = 3, panx, pany;
static int show_bg = 1, show_draw = 1, show_grid = 0, show_hard = 0, show_points = 1;
static int bg_alpha = 60;      /* % of background visible under the paper */

static int tool = T_LINE;
static int line_active, pen_x, pen_y, start_plot = 0, snap_points = 1;
static int cur_pat = 0, cur_hard = 1, hard_rect = 0;
static int hard_drag, hard_x0, hard_y0, hard_lx, hard_ly;
static int sel = -1, drag_op = -1;
static int insert_after = 0, preview_sel = 0;
static int list_scroll = 0;
static int panning, pan_mx, pan_my, pan_x0, pan_y0;

static uint8_t pix[SCR_W * SCR_H];
static int render_dirty = 1;
static uint8_t fill_mark[SCR_W * SCR_H];
static int fill_px = -1, fill_py = -1, fill_ok;

static char status[256];
static uint32_t status_time;

static int modal = M_NONE, pending_action = A_NONE;
static char pending_path[1024];

/* file browser */
typedef struct { char name[256]; int dir; } Entry;
static Entry *entries;
static int nentries, fb_scroll, fb_sel = -1, fb_purpose, fb_save;
static char fb_dir[1400], fb_name[256], fb_exts[64], fb_title[64];
static int fb_fresh;   /* suggested name is 'selected': typing replaces it */

static uint32_t last_click_time;
static int last_click_x, last_click_y;

/* ------------------------------------------------------------------ */
/* helpers                                                             */

static void set_status(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(status, sizeof(status), fmt, ap);
    va_end(ap);
    status_time = plat_ticks();
}

static const char *base_name(const char *p) {
    const char *b = p, *s;
    for (s = p; *s; s++) if (*s == '/' || *s == '\\') b = s + 1;
    return b;
}

static int has_ext(const char *p, const char *exts) {
    const char *dot = strrchr(base_name(p), '.'), *s = exts;
    if (!dot) return 0;
    dot++;
    while (*s) {
        size_t l = strcspn(s, ";");
        if (strlen(dot) == l) {
            size_t i;
            for (i = 0; i < l && tolower((unsigned char)dot[i]) == tolower((unsigned char)s[i]); i++);
            if (i == l) return 1;
        }
        s += l; if (*s) s++;
    }
    return 0;
}

static void update_title(void) {
    char t[1200];
    snprintf(t, sizeof(t), "Suvleir 4 - %s%s", filename[0] ? base_name(filename) : "untitled", modified ? " *" : "");
    plat_set_title(t);
}

static void changed(void) {
    render_dirty = 1;
    fill_px = -1;
    if (!modified) { modified = 1; update_title(); }
}

static void undo_push(void) {
    int i;
    if (undo_n == MAX_UNDO) {
        vec_free(&undo_stack[0]);
        for (i = 1; i < MAX_UNDO; i++) undo_stack[i - 1] = undo_stack[i];
        vec_init(&undo_stack[MAX_UNDO - 1]);
        undo_n--;
    }
    vec_copy(&undo_stack[undo_n++], &doc);
    redo_n = 0;
}

static void do_undo(void) {
    if (!undo_n) { set_status("nothing to undo"); return; }
    vec_copy(&redo_stack[redo_n++], &doc);
    vec_copy(&doc, &undo_stack[--undo_n]);
    if (sel >= doc.nops) sel = doc.nops - 1;
    line_active = 0;
    changed();
    set_status("undo");
}

static void do_redo(void) {
    if (!redo_n) { set_status("nothing to redo"); return; }
    vec_copy(&undo_stack[undo_n++], &doc);
    vec_copy(&doc, &redo_stack[--redo_n]);
    if (sel >= doc.nops) sel = doc.nops - 1;
    line_active = 0;
    changed();
    set_status("redo");
}

static int insert_pos(void) { return (insert_after && sel >= 0) ? sel + 1 : doc.nops; }

static void add_op(int op, int x, int y, int p) {
    VecOp o;
    int at = insert_pos();
    o.op = (uint8_t)op; o.x = (uint8_t)x; o.y = (uint8_t)y; o.p = (uint8_t)p;
    vec_insert(&doc, at, o);
    if (insert_after && sel >= 0) sel = at;
    changed();
}

static int img_x(int mx) { return (mx - VX - panx) / zoom - ((mx - VX - panx) < 0); }
static int img_y(int my) { return (my - VY - pany) / zoom - ((my - VY - pany) < 0); }
static int in_canvas(int mx, int my) { return mx >= VX && my >= VY && mx < VX + VW && my < VY + VH; }

static void clamp_pan(void) {
    int iw = SCR_W * zoom, ih = SCR_H * zoom;
    if (iw <= VW) panx = (VW - iw) / 2;
    else { if (panx > 0) panx = 0; if (panx < VW - iw) panx = VW - iw; }
    if (ih <= VH) pany = (VH - ih) / 2;
    else { if (pany > 0) pany = 0; if (pany < VH - ih) pany = VH - ih; }
}

static void set_zoom(int z, int mx, int my) {
    int ix, iy;
    if (z < 1) z = 1;
    if (z > 12) z = 12;
    if (z == zoom) return;
    if (!in_canvas(mx, my)) { mx = VX + VW / 2; my = VY + VH / 2; }
    ix = img_x(mx); iy = img_y(my);
    zoom = z;
    panx = mx - VX - ix * zoom - zoom / 2;
    pany = my - VY - iy * zoom - zoom / 2;
    clamp_pan();
    set_status("zoom %dx", zoom);
}

/* op point at index i (every op has one) */
static int near_point(int ix, int iy, int radius_screen, int only_vertices, int *ox, int *oy) {
    int i, best = -1, bestd = 1 << 30;
    for (i = 0; i < doc.nops; i++) {
        const VecOp *o = &doc.ops[i];
        int dx, dy, d;
        if (only_vertices && o->op == OP_FILL) continue;
        dx = (o->x - ix) * zoom; dy = (o->y - iy) * zoom;
        d = dx * dx + dy * dy;
        if (d <= radius_screen * radius_screen && d < bestd) { bestd = d; best = i; }
    }
    if (best >= 0 && ox) { *ox = doc.ops[best].x; *oy = doc.ops[best].y; }
    return best;
}

/* ------------------------------------------------------------------ */
/* files                                                               */

static void new_doc(void) {
    vec_clear(&doc);
    filename[0] = 0;
    modified = 0;
    sel = -1; line_active = 0;
    undo_n = redo_n = 0;
    render_dirty = 1; fill_px = -1;
    update_title();
    set_status("new screen");
}

static void open_path(const char *path) {
    VecDoc tmp;
    char msg[300];
    vec_init(&tmp);
    if (vec_load_file(&tmp, path, 0, msg, sizeof(msg)) == 0) {
        vec_copy(&doc, &tmp);
        snprintf(filename, sizeof(filename), "%s", path);
        /* a legacy file is saved as .svl next to it, never over the original */
        if (has_ext(path, "bin") && strstr(msg, "legacy")) {
            char *dot = strrchr(filename, '.');
            if (dot) strcpy(dot, ".svl");
        }
        modified = 0;
        sel = -1; line_active = 0;
        undo_n = redo_n = 0;
        render_dirty = 1; fill_px = -1;
        update_title();
    }
    set_status("%s", msg);
    vec_free(&tmp);
}

static int save_path(const char *path) {
    char msg[300];
    int r = vec_save_file(&doc, path, msg, sizeof(msg));
    if (!r) {
        snprintf(filename, sizeof(filename), "%s", path);
        modified = 0;
        update_title();
    }
    set_status("%s", msg);
    return r;
}

static void load_bg(const char *path) {
    char msg[300] = "";
    if (img_load(path, bgimg, msg, sizeof(msg)) == 0) {
        has_bg = 1; show_bg = 1;
        snprintf(bgname, sizeof(bgname), "%s", path);
    }
    set_status("%s", msg);
}

static void label_from(const char *path, char *out, size_t n) {
    const char *b = base_name(path);
    size_t i = 0;
    while (*b && *b != '.' && i + 1 < n) {
        char c = *b++;
        out[i++] = isalnum((unsigned char)c) ? c : '_';
    }
    out[i] = 0;
    if (!i || isdigit((unsigned char)out[0])) {
        char t[64];
        snprintf(t, sizeof(t), "svl_%s", out);
        snprintf(out, n, "%s", t);
    }
}

static void file_chosen(int purpose, const char *path) {
    char label[64];
    switch (purpose) {
    case F_OPEN: open_path(path); break;
    case F_SAVE: save_path(path); break;
    case F_BG: load_bg(path); break;
    case F_PAT_LOAD:
        if (vec_load_patterns(&pats, path)) set_status("can't load patterns from %s", path);
        else { render_dirty = 1; fill_px = -1; set_status("%d patterns loaded from %s", pats.count, path); }
        break;
    case F_PAT_SAVE:
        set_status(vec_save_patterns(&pats, path) ? "can't write %s" : "patterns saved to %s", path);
        break;
    case F_ASM: case F_C:
        label_from(path, label, sizeof(label));
        if ((purpose == F_ASM ? vec_export_asm : vec_export_c)(&doc, path, label)) set_status("can't write %s", path);
        else set_status("exported %s (label %s)", path, label);
        break;
    case F_LEGACY: {
        uint8_t *b = (uint8_t *)malloc(65536);
        size_t n = vec_legacy_encode(&doc, b, 65536);
        set_status(write_file(path, b, n) ? "can't write %s" : "exported legacy Suvleir 3 file %s", path);
        free(b);
        break;
    }
    }
}

static int cmp_entries(const void *a, const void *b) {
    const Entry *x = (const Entry *)a, *y = (const Entry *)b;
    if (x->dir != y->dir) return y->dir - x->dir;
#ifdef _WIN32
    return _stricmp(x->name, y->name);
#else
    return strcasecmp(x->name, y->name);
#endif
}

static void fb_add(const char *name, int isdir) {
    static int cap;
    if (!isdir && fb_exts[0] && !has_ext(name, fb_exts)) return;
    if (name[0] == '.' && strcmp(name, "..")) return;
    if (nentries == cap) { cap = cap ? cap * 2 : 64; entries = (Entry *)realloc(entries, cap * sizeof(Entry)); }
    snprintf(entries[nentries].name, sizeof(entries[0].name), "%s", name);
    entries[nentries].dir = isdir;
    nentries++;
}

static void fb_read_dir(void) {
    nentries = 0; fb_scroll = 0; fb_sel = -1;
#ifdef _WIN32
    {
        char pat[1100];
        WIN32_FIND_DATAA fd;
        HANDLE h;
        snprintf(pat, sizeof(pat), "%s\\*", fb_dir);
        h = FindFirstFileA(pat, &fd);
        if (h == INVALID_HANDLE_VALUE) return;
        do {
            if (strcmp(fd.cFileName, ".")) fb_add(fd.cFileName, (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0);
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
#else
    {
        DIR *d = opendir(fb_dir);
        struct dirent *de;
        if (!d) return;
        while ((de = readdir(d))) {
            char full[1700];
            struct stat st;
            if (!strcmp(de->d_name, ".")) continue;
            snprintf(full, sizeof(full), "%s%c%s", fb_dir, PATHSEP, de->d_name);
            fb_add(de->d_name, !stat(full, &st) && S_ISDIR(st.st_mode));
        }
        closedir(d);
    }
#endif
    qsort(entries, nentries, sizeof(Entry), cmp_entries);
}

static void fb_cd(const char *name) {
    if (!strcmp(name, "..")) {
        char *s = strrchr(fb_dir, PATHSEP);
        if (s && s != fb_dir) *s = 0;
        else if (s) s[1] = 0;
    } else {
        size_t l = strlen(fb_dir);
        if (l && fb_dir[l - 1] != PATHSEP) snprintf(fb_dir + l, sizeof(fb_dir) - l, "%c%s", PATHSEP, name);
        else snprintf(fb_dir + l, sizeof(fb_dir) - l, "%s", name);
    }
    fb_read_dir();
}

static void choose_file(int purpose, int save, const char *title, const char *desc, const char *exts, const char *suggest) {
    char path[1024] = "";
    int r;
    if (suggest) snprintf(path, sizeof(path), "%s", suggest);
    r = plat_file_dialog(save, title, desc, exts, path, sizeof(path));
    if (r == 1) { file_chosen(purpose, path); return; }
    if (r == 0) return;
    /* built-in browser */
    fb_purpose = purpose; fb_save = save;
    snprintf(fb_exts, sizeof(fb_exts), "%s", exts);
    snprintf(fb_title, sizeof(fb_title), "%s", title);
    fb_name[0] = 0;
    if (suggest && suggest[0]) {
        const char *b = base_name(suggest);
        snprintf(fb_name, sizeof(fb_name), "%s", b);
        if (b != suggest) {
            snprintf(fb_dir, sizeof(fb_dir), "%.*s", (int)(b - suggest - 1), suggest);
            if (!fb_dir[0]) snprintf(fb_dir, sizeof(fb_dir), "%c", PATHSEP);
        }
    }
    if (!fb_dir[0] && !getcwd(fb_dir, sizeof(fb_dir))) snprintf(fb_dir, sizeof(fb_dir), ".");
    fb_read_dir();
    fb_fresh = 1;
    modal = M_FILE;
}

static void fb_accept(void) {
    char path[1700];
    struct stat st;
    if (!fb_name[0]) return;
    if (fb_name[0] == PATHSEP || (fb_name[0] && fb_name[1] == ':'))
        snprintf(path, sizeof(path), "%s", fb_name);
    else
        snprintf(path, sizeof(path), "%s%c%s", fb_dir, PATHSEP, fb_name);
    if (!stat(path, &st) && S_ISDIR(st.st_mode)) {
        snprintf(fb_dir, sizeof(fb_dir), "%.1399s", path);
        fb_name[0] = 0;
        fb_read_dir();
        return;
    }
    if (fb_save && !strchr(fb_name, '.')) {
        size_t l = strlen(path);
        snprintf(path + l, sizeof(path) - l, ".%.*s", (int)strcspn(fb_exts, ";"), fb_exts);
    }
    if (!fb_save && stat(path, &st)) { set_status("no such file: %s", path); return; }
    modal = M_NONE;
    file_chosen(fb_purpose, path);
}

/* actions guarded by "unsaved changes?" */
static void run_action(int a) {
    switch (a) {
    case A_NEW: new_doc(); break;
    case A_OPEN: choose_file(F_OPEN, 0, "Open screen", "Suvleir screens (SVL, legacy .bin)", "svl;bin", filename); break;
    case A_OPEN_PATH: open_path(pending_path); break;
    case A_QUIT: running = 0; break;
    }
}

static void guarded(int a) {
    if (modified) { pending_action = a; modal = M_CONFIRM; }
    else run_action(a);
}

static void do_save(int as) {
    if (!as && filename[0]) save_path(filename);
    else choose_file(F_SAVE, 1, "Save screen as", "Suvleir 4 screens", "svl", filename[0] ? filename : "screen.svl");
}

/* ------------------------------------------------------------------ */
/* rendering the canvas                                                */

static uint32_t paper_at(int i) {
    if (show_bg && has_bg) return ui_mix(0xF4F4F4, bgimg[i], bg_alpha * 256 / 100);
    return 0xE8E8E8;
}

static void line_preview_pixels(uint8_t *buf, int x0, int y0, int x1, int y1) {
    int cx = x0, cy = y0;
    memset(buf, 0, SCR_W * SCR_H);
    vec_line(buf, &cx, &cy, x1, y1);
}

static void constrain(int x0, int y0, int *x, int *y) {
    int dx = *x - x0, dy = *y - y0, adx = abs(dx), ady = abs(dy);
    if (adx > 2 * ady) *y = y0;
    else if (ady > 2 * adx) *x = x0;
    else { int m = adx < ady ? adx : ady; *x = x0 + (dx < 0 ? -m : m); *y = y0 + (dy < 0 ? -m : m); }
}

/* where the next click would put a point (after snapping / constraining) */
static int target_point(int *tx, int *ty) {
    int ix = img_x(ui.mx), iy = img_y(ui.my);
    if (!in_canvas(ui.mx, ui.my)) return 0;
    if (ix < 0) ix = 0;
    if (iy < 0) iy = 0;
    if (ix > 255) ix = 255;
    if (iy > 191) iy = 191;
    if (snap_points && !(plat_mods() & KMOD_ALT) && near_point(ix, iy, 7, 1, tx, ty) >= 0) return 2;
    if (line_active && (plat_mods() & KMOD_SHIFT)) {
        constrain(pen_x, pen_y, &ix, &iy);
        if (ix < 0) ix = 0;
        if (ix > 255) ix = 255;
        if (iy < 0) iy = 0;
        if (iy > 191) iy = 191;
    }
    *tx = ix; *ty = iy;
    return 1;
}

static void draw_canvas(void) {
    static uint8_t preview[SCR_W * SCR_H];
    static uint32_t img[SCR_W * SCR_H];
    int i, x, y, tx = 0, ty = 0, tgt = 0, have_preview = 0;
    int hx = img_x(ui.mx), hy = img_y(ui.my), hover = in_canvas(ui.mx, ui.my) && hx >= 0 && hy >= 0 && hx < SCR_W && hy < SCR_H;

    if (render_dirty) {
        vec_render(&doc, &pats, (preview_sel && sel >= 0) ? sel + 1 : -1, pix);
        render_dirty = 0;
    }
    /* fill preview */
    if (tool == T_FILL && hover && !modal) {
        if (hx != fill_px || hy != fill_py) {
            static uint8_t tmp[SCR_W * SCR_H];
            memcpy(tmp, pix, sizeof(tmp));
            fill_ok = vec_fill(tmp, hx, hy, pats.pat[cur_pat], fill_mark) > 0;
            fill_px = hx; fill_py = hy;
        }
    }
    if (tool == T_LINE && line_active && !modal) {
        tgt = target_point(&tx, &ty);
        if (tgt) { line_preview_pixels(preview, pen_x, pen_y, tx, ty); have_preview = 1; }
    }

    for (i = 0; i < SCR_W * SCR_H; i++) {
        uint32_t c = paper_at(i);
        if (show_draw && pix[i]) c = 0x101010;
        if (tool == T_FILL && hover && fill_ok && fill_mark[i]) {
            int px = i % SCR_W, py = i / SCR_W;
            int bit = (pats.pat[cur_pat][py & 7] >> (7 - (px & 7))) & 1;
            c = bit ? 0x6A2C9C : ui_mix(c, 0xC9A6E8, 120);
        }
        if (have_preview && preview[i]) c = 0xE5534B;
        img[i] = c;
    }

    ui_rect(VX, VY, VW, VH, 0x15181E);
    for (y = 0; y < VH; y++) {
        int iy = (y - pany) / zoom;
        uint32_t *row = fb + (VY + y) * WIN_W + VX;
        if (y - pany < 0 || iy >= SCR_H) continue;
        for (x = 0; x < VW; x++) {
            int ix = (x - panx) / zoom;
            if (x - panx < 0 || ix >= SCR_W) continue;
            row[x] = img[iy * SCR_W + ix];
        }
    }

    /* overlays, clipped to the viewport */
#define SX(ix) (VX + panx + (ix) * zoom)
#define SY(iy) (VY + pany + (iy) * zoom)
    if (show_hard || tool == T_HARD) {
        static const uint32_t hc[4] = {0, 0xE5534B, 0x4FA3F7, 0x57C26A};
        int r, c;
        for (r = 0; r < HARD_H; r++)
            for (c = 0; c < HARD_W; c++) {
                int v = doc.hard[r * HARD_W + c];
                int sx = SX(c * 8), sy = SY(r * 8), w = 8 * zoom, h = 8 * zoom;
                if (!v) continue;
                if (sx < VX) { w -= VX - sx; sx = VX; }
                if (sy < VY) { h -= VY - sy; sy = VY; }
                if (sx + w > VX + VW) w = VX + VW - sx;
                if (sy + h > VY + VH) h = VY + VH - sy;
                if (w > 0 && h > 0) ui_blend_rect(sx, sy, w, h, hc[v], 140);
            }
        if (hard_drag && hard_rect) {
            int x0 = hard_x0, y0 = hard_y0, x1 = hx >> 3, y1 = hy >> 3, t;
            if (x1 < x0) { t = x0; x0 = x1; x1 = t; }
            if (y1 < y0) { t = y0; y0 = y1; y1 = t; }
            ui_frame(SX(x0 * 8), SY(y0 * 8), (x1 - x0 + 1) * 8 * zoom, (y1 - y0 + 1) * 8 * zoom, C_ACCENT);
        }
    }
    if (show_grid || tool == T_HARD) {
        int g;
        for (g = 0; g <= SCR_W; g += 8) {
            int sx = SX(g);
            if (sx >= VX && sx < VX + VW)
                for (y = (SY(0) > VY ? SY(0) : VY); y < VY + VH && y < SY(SCR_H); y += 2) ui_pixel(sx, y, (g % 64) ? 0x8890A0 : 0x5060A0);
        }
        for (g = 0; g <= SCR_H; g += 8) {
            int sy = SY(g);
            if (sy >= VY && sy < VY + VH)
                for (x = (SX(0) > VX ? SX(0) : VX); x < VX + VW && x < SX(SCR_W); x += 2) ui_pixel(x, sy, (g % 64) ? 0x8890A0 : 0x5060A0);
        }
    }
    if (show_points || tool == T_EDIT) {
        for (i = 0; i < doc.nops; i++) {
            const VecOp *o = &doc.ops[i];
            int sx = SX(o->x) + zoom / 2, sy = SY(o->y) + zoom / 2;
            uint32_t c = o->op == OP_FILL ? 0xB06AE8 : o->op == OP_LINE ? C_ACCENT2 : 0x57C26A;
            if (sx < VX + 2 || sy < VY + 2 || sx >= VX + VW - 2 || sy >= VY + VH - 2) continue;
            if (i == sel) { ui_frame(sx - 4, sy - 4, 9, 9, C_ACCENT); ui_frame(sx - 5, sy - 5, 11, 11, 0x000000); }
            if (o->op == OP_FILL) { ui_rect(sx - 2, sy - 2, 5, 5, c); ui_frame(sx - 3, sy - 3, 7, 7, 0x000000); }
            else ui_rect(sx - 1, sy - 1, 3, 3, c);
        }
    }
    /* hover markers */
    if (hover && !modal) {
        if (tool == T_LINE) {
            int px, py, k = target_point(&px, &py);
            if (k == 2) ui_frame(SX(px) + zoom / 2 - 5, SY(py) + zoom / 2 - 5, 11, 11, 0x00C000);
            if (k) ui_frame(SX(px), SY(py), zoom, zoom, 0xD02020);
        } else if (tool == T_EDIT) {
            int k = near_point(hx, hy, 8, 0, NULL, NULL);
            if (k >= 0) {
                const VecOp *o = &doc.ops[k];
                ui_frame(SX(o->x) + zoom / 2 - 6, SY(o->y) + zoom / 2 - 6, 13, 13, 0xFFFFFF);
            }
        } else if (tool == T_HARD) {
            ui_frame(SX((hx >> 3) * 8), SY((hy >> 3) * 8), 8 * zoom, 8 * zoom, 0xFFFFFF);
        }
    }
    if (tool == T_LINE && line_active) {
        int sx = SX(pen_x) + zoom / 2, sy = SY(pen_y) + zoom / 2;
        ui_frame(sx - 3, sy - 3, 7, 7, 0xE5534B);
    }
    ui_frame(VX - 1, VY - 1, VW + 2, VH + 2, C_BORDER);
#undef SX
#undef SY
}

/* ------------------------------------------------------------------ */
/* canvas input                                                        */

static void canvas_input(void) {
    int hx = img_x(ui.mx), hy = img_y(ui.my);
    int inside = in_canvas(ui.mx, ui.my) && !ui.blocked;
    int onimg = inside && hx >= 0 && hy >= 0 && hx < SCR_W && hy < SCR_H;

    /* panning: middle button or space+left */
    if (inside && (ui.pressed[BTN_MIDDLE])) {
        panning = 1; pan_mx = ui.mx; pan_my = ui.my; pan_x0 = panx; pan_y0 = pany;
    }
    if (panning) {
        panx = pan_x0 + ui.mx - pan_mx; pany = pan_y0 + ui.my - pan_my;
        clamp_pan();
        if (ui.released[BTN_MIDDLE] || ui.released[BTN_LEFT]) panning = 0;
        return;
    }
    if (inside && ui.wheel) {
        if (plat_mods() & KMOD_CTRL) set_zoom(zoom + ui.wheel, ui.mx, ui.my);
        else if (plat_mods() & KMOD_SHIFT) { panx += ui.wheel * 48; clamp_pan(); }
        else { pany += ui.wheel * 48; clamp_pan(); }
    }

    switch (tool) {
    case T_LINE:
        if (onimg && ui.pressed[BTN_LEFT]) {
            int tx, ty;
            if (!target_point(&tx, &ty)) break;
            undo_push();
            if (!line_active) {
                add_op(start_plot ? OP_PLOT : OP_MOVE, tx, ty, 0);
                line_active = 1;
            } else if (tx != pen_x || ty != pen_y) {
                add_op(OP_LINE, tx, ty, 0);
            } else {
                undo_n--;   /* nothing happened */
            }
            pen_x = tx; pen_y = ty;
        }
        if (ui.pressed[BTN_RIGHT] && inside) {
            if (line_active) { line_active = 0; set_status("polyline finished"); }
        }
        break;
    case T_FILL:
        if (onimg && ui.pressed[BTN_LEFT]) {
            if (pix[hy * SCR_W + hx]) set_status("can't fill there: that pixel is ink (the engine would do nothing)");
            else {
                undo_push();
                add_op(OP_FILL, hx, hy, cur_pat);
                set_status("fill %d,%d pattern %d", hx, hy, cur_pat);
            }
        }
        break;
    case T_HARD: {
        int cx = hx >> 3, cy = hy >> 3;
        int b = ui.pressed[BTN_LEFT] ? BTN_LEFT : ui.pressed[BTN_RIGHT] ? BTN_RIGHT : 0;
        if (onimg && b) {
            undo_push();
            hard_drag = b;
            hard_x0 = cx; hard_y0 = cy;
            hard_lx = cx; hard_ly = cy;
            hard_rect = (plat_mods() & KMOD_SHIFT) ? 1 : hard_rect;
        }
        if (hard_drag) {
            int v = hard_drag == BTN_LEFT ? cur_hard : 0;
            if (cx < 0) cx = 0;
            if (cy < 0) cy = 0;
            if (cx >= HARD_W) cx = HARD_W - 1;
            if (cy >= HARD_H) cy = HARD_H - 1;
            if (!hard_rect) {
                /* paint every cell between the last and the current position */
                int steps = abs(cx - hard_lx) > abs(cy - hard_ly) ? abs(cx - hard_lx) : abs(cy - hard_ly), k;
                for (k = 0; k <= steps; k++) {
                    int px = steps ? hard_lx + (cx - hard_lx) * k / steps : cx;
                    int py = steps ? hard_ly + (cy - hard_ly) * k / steps : cy;
                    if (doc.hard[py * HARD_W + px] != v) { doc.hard[py * HARD_W + px] = (uint8_t)v; changed(); }
                }
                hard_lx = cx; hard_ly = cy;
            }
            if (ui.released[hard_drag]) {
                if (hard_rect) {
                    int x0 = hard_x0 < cx ? hard_x0 : cx, x1 = hard_x0 < cx ? cx : hard_x0;
                    int y0 = hard_y0 < cy ? hard_y0 : cy, y1 = hard_y0 < cy ? cy : hard_y0, x, y;
                    for (y = y0; y <= y1; y++) for (x = x0; x <= x1; x++) doc.hard[y * HARD_W + x] = (uint8_t)v;
                    changed();
                    if (!(plat_mods() & KMOD_SHIFT) && hard_rect == 1) {}
                }
                hard_drag = 0;
            }
        }
        break;
    }
    case T_EDIT:
        if (onimg && ui.pressed[BTN_LEFT]) {
            int k = near_point(hx, hy, 8, 0, NULL, NULL);
            sel = k;
            if (k >= 0) { undo_push(); drag_op = k; }
            if (preview_sel) render_dirty = 1;
        }
        if (drag_op >= 0) {
            int x = hx < 0 ? 0 : hx > 255 ? 255 : hx, y = hy < 0 ? 0 : hy > 191 ? 191 : hy;
            if (doc.ops[drag_op].x != x || doc.ops[drag_op].y != y) {
                doc.ops[drag_op].x = (uint8_t)x; doc.ops[drag_op].y = (uint8_t)y; changed();
            }
            if (ui.released[BTN_LEFT]) drag_op = -1;
        }
        break;
    }
}

/* ------------------------------------------------------------------ */
/* panels                                                              */

static const char *op_name[4] = {"PLOT", "LINE", "FILL", "MOVE"};

static void draw_pattern_swatch(int x, int y, int size, const uint8_t *p, int scale) {
    int i, j;
    for (j = 0; j < size; j++)
        for (i = 0; i < size; i++) {
            int px = i / scale, py = j / scale;
            ui_pixel(x + i, y + j, ((p[py & 7] >> (7 - (px & 7))) & 1) ? 0x101010 : 0xE8E8E8);
        }
}

static void panel_tool_options(int x, int y) {
    int i;
    switch (tool) {
    case T_LINE:
        ui_text(x, y, "Polyline", C_ACCENT);
        ui_text(x, y + 14, "start point:", C_DIM);
        if (ui_button(x + 104, y + 10, 60, 16, "move", !start_plot)) start_plot = 0;
        if (ui_button(x + 168, y + 10, 60, 16, "plot", start_plot)) start_plot = 1;
        ui_check(x, y + 36, "snap to existing points", &snap_points);
        ui_text(x, y + 54, "L-click   add point", C_DIM);
        ui_text(x, y + 66, "R-click   end polyline", C_DIM);
        ui_text(x, y + 78, "Enter/Esc end polyline", C_DIM);
        ui_text(x, y + 90, "Shift     0/45/90 deg", C_DIM);
        ui_text(x, y + 102, "Alt       don't snap", C_DIM);
        ui_textf(x, y + 120, line_active ? C_GREEN : C_DIM, line_active ? "drawing from %d,%d" : "click to start", pen_x, pen_y);
        break;
    case T_FILL:
        ui_text(x, y, "Patterned fill", C_ACCENT);
        for (i = 0; i < pats.count && i < 16; i++) {
            int sx = x + (i % 8) * 29, sy = y + 14 + (i / 8) * 29;
            draw_pattern_swatch(sx + 2, sy + 2, 22, pats.pat[i], 2);
            ui_frame(sx, sy, 26, 26, i == cur_pat ? C_ACCENT : C_BORDER);
            if (i == cur_pat) ui_frame(sx + 1, sy + 1, 24, 24, C_ACCENT);
            if (ui_clicked(sx, sy, 26, 26, BTN_LEFT)) { cur_pat = i; fill_px = -1; }
        }
        {
            int ey = y + 14 + ((pats.count + 7) / 8) * 29 + 6, r, c;
            ui_textf(x, ey, C_DIM, "pattern %d (click to edit):", cur_pat);
            for (r = 0; r < 8; r++)
                for (c = 0; c < 8; c++) {
                    int bx = x + c * 11, by = ey + 12 + r * 11;
                    int on = (pats.pat[cur_pat][r] >> (7 - c)) & 1;
                    ui_rect(bx, by, 10, 10, on ? 0x101010 : 0xE8E8E8);
                    if (ui_clicked(bx, by, 10, 10, BTN_LEFT)) {
                        pats.pat[cur_pat][r] ^= (uint8_t)(0x80 >> c);
                        render_dirty = 1; fill_px = -1;
                        set_status("pattern %d edited - remember the engine has its own copy (Save pats)", cur_pat);
                    }
                }
            draw_pattern_swatch(x + 96, ey + 12, 87, pats.pat[cur_pat], 1);
            ui_frame(x + 95, ey + 11, 89, 89, C_BORDER);
            if (ui_button(x + 192, ey + 12, 40, 16, "load", 0)) choose_file(F_PAT_LOAD, 0, "Load fill patterns", "Pattern tables (8 bytes each)", "bin;pat", "patterns.bin");
            if (ui_button(x + 192, ey + 32, 40, 16, "save", 0)) choose_file(F_PAT_SAVE, 1, "Save fill patterns", "Pattern tables (8 bytes each)", "bin;pat", "patterns.bin");
            if (ui_button(x + 192, ey + 52, 40, 16, "+", 0) && pats.count < MAX_PATTERNS) {
                memset(pats.pat[pats.count], 0xFF, 8); cur_pat = pats.count++; }
            if (ui_button(x + 192, ey + 72, 40, 16, "-", 0) && pats.count > 1) {
                pats.count--; if (cur_pat >= pats.count) cur_pat = pats.count - 1; render_dirty = 1; }
        }
        break;
    case T_HARD: {
        static const char *names[4] = {"0 free", "1 solid", "2 user", "3 user"};
        static const uint32_t hc[4] = {0xE8E8E8, 0xE5534B, 0x4FA3F7, 0x57C26A};
        ui_text(x, y, "Hardness map (8x8 cells)", C_ACCENT);
        for (i = 0; i < 4; i++) {
            int bx = x + (i % 2) * 116, by = y + 14 + (i / 2) * 22;
            if (ui_button(bx + 20, by, 92, 18, names[i], cur_hard == i)) cur_hard = i;
            ui_rect(bx, by + 2, 14, 14, hc[i]);
            ui_frame(bx, by + 2, 14, 14, C_BORDER);
        }
        ui_text(x, y + 66, "mode:", C_DIM);
        if (ui_button(x + 48, y + 62, 60, 16, "brush", !hard_rect)) hard_rect = 0;
        if (ui_button(x + 112, y + 62, 60, 16, "rect", hard_rect)) hard_rect = 1;
        if (ui_button(x, y + 86, 72, 16, "clear", 0)) { undo_push(); memset(doc.hard, 0, sizeof(doc.hard)); changed(); }
        if (ui_button(x + 78, y + 86, 72, 16, "all = v", 0)) { undo_push(); memset(doc.hard, cur_hard, sizeof(doc.hard)); changed(); }
        if (ui_button(x + 156, y + 86, 76, 16, "ink -> v", 0)) {
            /* cells that are mostly ink get the current value */
            int r, c, a, b;
            undo_push();
            for (r = 0; r < HARD_H; r++)
                for (c = 0; c < HARD_W; c++) {
                    int n = 0;
                    for (b = 0; b < 8; b++) for (a = 0; a < 8; a++) n += pix[(r * 8 + b) * SCR_W + c * 8 + a];
                    if (n >= 32) doc.hard[r * HARD_W + c] = (uint8_t)cur_hard;
                }
            changed();
            set_status("cells at least half ink set to %d", cur_hard);
        }
        ui_text(x, y + 108, "L-drag paint, R-drag erase", C_DIM);
        ui_text(x, y + 120, "Shift+drag: rectangle", C_DIM);
        break;
    }
    case T_EDIT:
        ui_text(x, y, "Edit points", C_ACCENT);
        if (sel >= 0 && sel < doc.nops) {
            VecOp *o = &doc.ops[sel];
            ui_textf(x, y + 14, C_TEXT, "#%d %s %d,%d", sel, op_name[o->op], o->x, o->y);
            if (o->op == OP_FILL) ui_textf(x + 168, y + 14, C_TEXT, "p%d", o->p);
            if (o->op == OP_FILL) {
                if (ui_button(x, y + 28, 110, 16, "pattern <", 0) && o->p > 0) { undo_push(); o->p--; changed(); }
                if (ui_button(x + 116, y + 28, 110, 16, "pattern >", 0) && o->p + 1 < pats.count) { undo_push(); o->p++; changed(); }
            } else if (o->op != OP_LINE) {
                if (ui_button(x, y + 28, 110, 16, o->op == OP_PLOT ? "make MOVE" : "make PLOT", 0)) {
                    undo_push(); o->op = o->op == OP_PLOT ? OP_MOVE : OP_PLOT; changed(); }
            }
            if (ui_button(x, y + 50, 72, 16, "delete", 0)) { undo_push(); vec_delete(&doc, sel); if (sel >= doc.nops) sel = doc.nops - 1; changed(); }
            if (ui_button(x + 78, y + 50, 72, 16, "earlier", 0) && sel > 0) {
                VecOp t; undo_push(); t = doc.ops[sel]; doc.ops[sel] = doc.ops[sel - 1]; doc.ops[sel - 1] = t; sel--; changed(); }
            if (ui_button(x + 156, y + 50, 72, 16, "later", 0) && sel + 1 < doc.nops) {
                VecOp t; undo_push(); t = doc.ops[sel]; doc.ops[sel] = doc.ops[sel + 1]; doc.ops[sel + 1] = t; sel++; changed(); }
        } else {
            ui_text(x, y + 14, "nothing selected", C_DIM);
        }
        ui_text(x, y + 74, "click a point to select,", C_DIM);
        ui_text(x, y + 86, "drag to move it", C_DIM);
        ui_text(x, y + 98, "arrows  nudge 1 px", C_DIM);
        ui_text(x, y + 110, "Del     delete", C_DIM);
        ui_text(x, y + 122, "PgUp/Dn previous/next op", C_DIM);
        break;
    }
}

static void draw_panel(void) {
    int x = PX + 8, y = VY, i;
    static const char *tn[4] = {"Line", "Fill", "Hard", "Edit"};
    ui_rect(PX, VY - 2, PW, SB_Y - VY - 2, C_PANEL);
    ui_frame(PX, VY - 2, PW, SB_Y - VY - 2, C_BORDER);
    for (i = 0; i < 4; i++)
        if (ui_button(x + i * 58, y + 4, 54, 20, tn[i], tool == i)) {
            tool = i; line_active = 0; fill_px = -1;
        }
    ui_hline(PX + 1, y + 30, PW - 2, C_BORDER);
    panel_tool_options(x, y + 38);

    y = VY + 214;
    ui_hline(PX + 1, y - 6, PW - 2, C_BORDER);
    ui_check(x, y, "background", &show_bg);
    ui_check(x + 120, y, "drawing", &show_draw);
    ui_check(x, y + 16, "grid", &show_grid);
    ui_check(x + 120, y + 16, "hardness", &show_hard);
    ui_check(x, y + 32, "points", &show_points);
    ui_textf(x + 120, y + 32, C_DIM, "bg %3d%%", bg_alpha);
    if (ui_button(x + 190, y + 28, 18, 16, "-", 0) && bg_alpha > 0) bg_alpha -= 10;
    if (ui_button(x + 212, y + 28, 18, 16, "+", 0) && bg_alpha < 100) bg_alpha += 10;
    if (ui_check(x, y + 48, "insert after selected op", &insert_after)) {}
    if (ui_check(x, y + 64, "render only up to selection", &preview_sel)) render_dirty = 1;

    /* ops list */
    y = VY + 300;
    ui_hline(PX + 1, y - 6, PW - 2, C_BORDER);
    ui_textf(x, y, C_ACCENT, "Operations (%d)", doc.nops);
    {
        int ly = y + 14, rows = (SB_Y - 8 - ly) / 11, lh = rows * 11;
        int lx = x, lw = PW - 16;
        ui_rect(lx, ly, lw, lh, C_PANEL2);
        if (ui_inside(lx, ly, lw, lh) && ui.wheel) list_scroll -= ui.wheel * 3;
        if (sel >= 0 && ui.dbl == 0) {}
        if (list_scroll > doc.nops - rows) list_scroll = doc.nops - rows;
        if (list_scroll < 0) list_scroll = 0;
        for (i = 0; i < rows && list_scroll + i < doc.nops; i++) {
            int k = list_scroll + i;
            const VecOp *o = &doc.ops[k];
            int ry = ly + i * 11;
            uint32_t col = o->op == OP_FILL ? 0xD3A6FF : o->op == OP_LINE ? C_TEXT : 0x8FE39C;
            if (k == sel) ui_rect(lx, ry, lw, 11, 0x5A4A20);
            if (ui_clicked(lx, ry, lw, 11, BTN_LEFT)) {
                sel = (sel == k) ? -1 : k;
                if (preview_sel) render_dirty = 1;
            }
            if (o->op == OP_FILL) ui_textf(lx + 2, ry + 2, col, "%4d %s %3d,%3d p%d", k, op_name[o->op], o->x, o->y, o->p);
            else ui_textf(lx + 2, ry + 2, col, "%4d %s %3d,%3d", k, op_name[o->op], o->x, o->y);
        }
        if (doc.nops > rows) {
            int th = lh * rows / doc.nops, ty = ly + (lh - th) * list_scroll / (doc.nops - rows);
            ui_rect(lx + lw - 3, ty, 3, th < 6 ? 6 : th, C_BORDER);
        }
    }
}

static int tb_button(int *x, const char *label, int w) {
    int r = ui_button(*x, 5, w, 22, label, 0);
    *x += w + 4;
    return r;
}

static void draw_toolbar(void) {
    int x = 8;
    ui_rect(0, 0, WIN_W, 32, C_PANEL);
    ui_hline(0, 32, WIN_W, C_BORDER);
#define TB(label, w) tb_button(&x, label, w)
    if (TB("New", 40)) guarded(A_NEW);
    if (TB("Open", 48)) guarded(A_OPEN);
    if (TB("Save", 48)) do_save(0);
    if (TB("Save as", 64)) do_save(1);
    x += 8;
    if (TB("BG image", 72)) choose_file(F_BG, 0, "Load background image", "Images (PNG JPG BMP GIF TGA PCX SCR)", "png;jpg;jpeg;bmp;gif;tga;pcx;scr;psd;pnm;ppm", bgname);
    x += 8;
    if (TB("Undo", 48)) do_undo();
    if (TB("Redo", 48)) do_redo();
    x += 8;
    if (TB("-", 22)) set_zoom(zoom - 1, -1, -1);
    ui_textf(x, 12, C_TEXT, "%dx", zoom); x += 28;
    if (TB("+", 22)) set_zoom(zoom + 1, -1, -1);
    x += 8;
    if (TB("Export .asm", 96)) choose_file(F_ASM, 1, "Export as z80asm source", "Assembler", "asm", "screen.asm");
    if (TB("Export .h", 80)) choose_file(F_C, 1, "Export as C array", "C header", "h", "screen.h");
    if (TB("Legacy .bin", 96)) choose_file(F_LEGACY, 1, "Export legacy Suvleir 3 file", "Suvleir 3", "bin", "screen.bin");
    if (ui_button(WIN_W - 64, 5, 56, 22, "Help", modal == M_HELP)) modal = M_HELP;
#undef TB
}

static void draw_statusbar(void) {
    int hx = img_x(ui.mx), hy = img_y(ui.my);
    char right[200];
    ui_rect(0, SB_Y, WIN_W, 22, C_PANEL);
    ui_hline(0, SB_Y, WIN_W, C_BORDER);
    if (in_canvas(ui.mx, ui.my) && hx >= 0 && hy >= 0 && hx < SCR_W && hy < SCR_H)
        snprintf(right, sizeof(right), "%3d,%3d  cell %2d,%2d  hard %d  |  %u bytes", hx, hy, hx >> 3, hy >> 3,
                 doc.hard[(hy >> 3) * HARD_W + (hx >> 3)], (unsigned)vec_svl_size(&doc));
    else
        snprintf(right, sizeof(right), "cmds %u + hard %u  |  %u bytes", (unsigned)vec_cmd_bytes(&doc),
                 (unsigned)vec_hard_bytes(&doc), (unsigned)vec_svl_size(&doc));
    ui_text(WIN_W - 8 - ui_text_width(right), SB_Y + 7, right, C_TEXT);
    if (status[0] && plat_ticks() - status_time < 8000) {
        char s[120];
        snprintf(s, sizeof(s), "%.*s", (int)((WIN_W - 24 - ui_text_width(right)) / 8), status);
        ui_text(8, SB_Y + 7, s, C_ACCENT);
    }
}

/* ------------------------------------------------------------------ */
/* modals                                                              */

static void begin_modal(int w, int h, const char *title, int *ox, int *oy) {
    int x = (WIN_W - w) / 2, y = (WIN_H - h) / 2;
    ui_blend_rect(0, 0, WIN_W, WIN_H, 0x000000, 120);
    ui_rect(x, y, w, h, C_PANEL);
    ui_frame(x, y, w, h, C_ACCENT);
    ui_rect(x, y, w, 20, C_PANEL2);
    ui_text(x + 8, y + 6, title, C_ACCENT);
    ui.modal_x = x; ui.modal_y = y; ui.modal_w = w; ui.modal_h = h;
    *ox = x; *oy = y;
}

static void draw_confirm(void) {
    int x, y;
    begin_modal(380, 110, "Unsaved changes", &x, &y);
    ui_textf(x + 16, y + 36, C_TEXT, "Save changes to %s?", filename[0] ? base_name(filename) : "untitled");
    if (ui_button(x + 16, y + 70, 100, 24, "Save", 0)) {
        modal = M_NONE;
        if (filename[0]) { if (!save_path(filename)) run_action(pending_action); }
        else do_save(1);
    } else if (ui_button(x + 140, y + 70, 100, 24, "Discard", 0)) {
        modal = M_NONE; modified = 0; run_action(pending_action);
    } else if (ui_button(x + 264, y + 70, 100, 24, "Cancel", 0)) {
        modal = M_NONE;
    }
}

static void draw_filebrowser(void) {
    int x, y, i, rows = 24, lx, ly, lw;
    begin_modal(620, 420, fb_title, &x, &y);
    ui_textf(x + 12, y + 30, C_DIM, "%.70s", fb_dir);
    if (ui_button(x + 560, y + 26, 48, 16, "up", 0)) fb_cd("..");
    lx = x + 12; ly = y + 48; lw = 596;
    ui_rect(lx, ly, lw, rows * 12 + 4, C_PANEL2);
    if (ui_inside(lx, ly, lw, rows * 12) && ui.wheel) fb_scroll -= ui.wheel * 3;
    if (fb_scroll > nentries - rows) fb_scroll = nentries - rows;
    if (fb_scroll < 0) fb_scroll = 0;
    for (i = 0; i < rows && fb_scroll + i < nentries; i++) {
        int k = fb_scroll + i, ry = ly + 2 + i * 12;
        Entry *e = &entries[k];
        if (k == fb_sel) ui_rect(lx, ry, lw, 12, 0x5A4A20);
        ui_textf(lx + 4, ry + 2, e->dir ? C_ACCENT2 : C_TEXT, e->dir ? "[%s]" : "%s", e->name);
        if (ui_clicked(lx, ry, lw, 12, BTN_LEFT)) {
            if (ui.dbl && fb_sel == k) {
                if (e->dir) { fb_cd(e->name); return; }
                snprintf(fb_name, sizeof(fb_name), "%s", e->name);
                fb_accept();
                return;
            }
            fb_sel = k;
            if (!e->dir) snprintf(fb_name, sizeof(fb_name), "%s", e->name);
        }
    }
    ui_text(x + 12, y + 354, "file:", C_DIM);
    ui_rect(x + 56, y + 350, 552, 16, 0x15181E);
    ui_frame(x + 56, y + 350, 552, 16, C_ACCENT);
    if (fb_fresh && fb_name[0]) ui_rect(x + 58, y + 352, 8 * (int)strlen(fb_name) + 4, 12, 0x3A5A8A);
    ui_textf(x + 60, y + 354, C_TEXT, "%.66s_", fb_name);
    ui_textf(x + 12, y + 376, C_DIM, "showing *.%s", fb_exts);
    if (ui_button(x + 400, y + 388, 96, 22, fb_save ? "Save" : "Open", 0)) fb_accept();
    else if (ui_button(x + 508, y + 388, 96, 22, "Cancel", 0)) modal = M_NONE;
}

static void draw_help(void) {
    int x, y, i;
    static const char *lines[] = {
        "TOOLS       L line   F fill   H hardness   E edit",
        "FILES       Ctrl+N new   Ctrl+O open   Ctrl+S save   Ctrl+Shift+S save as",
        "            Ctrl+B background image   (or drop files on the window - Windows)",
        "EDIT        Ctrl+Z undo   Ctrl+Y / Ctrl+Shift+Z redo",
        "LINE        click: add point   right click / Enter / Esc: end polyline",
        "            Shift: snap angle   Alt: don't snap to existing points",
        "FILL        1-8 choose pattern   (hover shows the exact result)",
        "HARDNESS    0-3 choose value   L-drag paint   R-drag erase   Shift: rectangle",
        "EDIT        drag points   arrows: nudge   Del: delete   PgUp/PgDn: select",
        "VIEW        +/- or Ctrl+wheel zoom   wheel/Shift+wheel or middle-drag pan",
        "            G grid   B background   K hardness   D drawing   P points",
        "",
        "What you see is exactly what the engine draws: lines use the engine's",
        "Bresenham, fills use SPPFill's rules and the engine's pattern table.",
        "Files are saved as SVL (Suvleir 4); legacy .bin files are imported.",
        "Byte budget for the screen is shown in the status bar.",
    };
    begin_modal(660, 290, "Suvleir 4 - help", &x, &y);
    for (i = 0; i < (int)(sizeof(lines) / sizeof(lines[0])); i++) ui_text(x + 14, y + 32 + i * 13, lines[i], i < 11 ? C_TEXT : C_DIM);
    if (ui_button(x + 280, y + 258, 100, 22, "Close", 0)) modal = M_NONE;
}

/* ------------------------------------------------------------------ */
/* keyboard                                                            */

static void key_event(const PEvent *e) {
    int k = e->key, ctrl = e->mod & KMOD_CTRL, shift = e->mod & KMOD_SHIFT;
    if (modal == M_FILE) {
        if (k == K_ESC) modal = M_NONE;
        else if (k == K_ENTER) fb_accept();
        else if (k == K_BACKSPACE) { size_t l = strlen(fb_name); fb_fresh = 0; if (l) fb_name[l - 1] = 0; }
        else if (k == K_UP && fb_sel > 0) { fb_sel--; if (!entries[fb_sel].dir) snprintf(fb_name, sizeof(fb_name), "%s", entries[fb_sel].name); }
        else if (k == K_DOWN && fb_sel + 1 < nentries) { fb_sel++; if (!entries[fb_sel].dir) snprintf(fb_name, sizeof(fb_name), "%s", entries[fb_sel].name); }
        return;
    }
    if (modal == M_HELP) { if (k == K_ESC || k == K_ENTER || k == K_F1) modal = M_NONE; return; }
    if (modal == M_CONFIRM) { if (k == K_ESC) modal = M_NONE; return; }

    if (ctrl) {
        switch (k) {
        case 'z': if (shift) do_redo(); else do_undo(); return;
        case 'y': do_redo(); return;
        case 's': do_save(shift); return;
        case 'o': guarded(A_OPEN); return;
        case 'n': guarded(A_NEW); return;
        case 'b': choose_file(F_BG, 0, "Load background image", "Images", "png;jpg;jpeg;bmp;gif;tga;pcx;scr", bgname); return;
        }
        return;
    }
    switch (k) {
    case K_F1: modal = M_HELP; return;
    case K_ESC: case K_ENTER:
        if (tool == T_LINE && line_active) { line_active = 0; set_status("polyline finished"); }
        else if (k == K_ESC) { sel = -1; if (preview_sel) render_dirty = 1; }
        return;
    case 'l': tool = T_LINE; return;
    case 'f': tool = T_FILL; line_active = 0; fill_px = -1; return;
    case 'h': tool = T_HARD; line_active = 0; return;
    case 'e': tool = T_EDIT; line_active = 0; return;
    case 'g': show_grid = !show_grid; return;
    case 'b': show_bg = !show_bg; return;
    case 'k': show_hard = !show_hard; return;
    case 'd': show_draw = !show_draw; return;
    case 'p': show_points = !show_points; return;
    case '+': case '=': set_zoom(zoom + 1, ui.mx, ui.my); return;
    case '-': set_zoom(zoom - 1, ui.mx, ui.my); return;
    case K_PGUP: if (sel > 0) sel--; else sel = doc.nops - 1; if (preview_sel) render_dirty = 1; return;
    case K_PGDN: if (sel + 1 < doc.nops) sel++; if (preview_sel) render_dirty = 1; return;
    case K_DELETE: case K_BACKSPACE:
        if (sel >= 0 && sel < doc.nops) {
            undo_push(); vec_delete(&doc, sel);
            if (sel >= doc.nops) sel = doc.nops - 1;
            changed();
        }
        return;
    }
    if (k >= '0' && k <= '9') {
        if (tool == T_FILL && k >= '1' && k - '1' < pats.count) { cur_pat = k - '1'; fill_px = -1; }
        if (tool == T_HARD && k <= '3') cur_hard = k - '0';
        return;
    }
    if (k == K_LEFT || k == K_RIGHT || k == K_UP || k == K_DOWN) {
        int dx = k == K_LEFT ? -1 : k == K_RIGHT ? 1 : 0, dy = k == K_UP ? -1 : k == K_DOWN ? 1 : 0;
        if (tool == T_EDIT && sel >= 0 && sel < doc.nops) {
            VecOp *o = &doc.ops[sel];
            int nx = o->x + dx, ny = o->y + dy;
            if (nx >= 0 && nx <= 255 && ny >= 0 && ny <= 191) { undo_push(); o->x = (uint8_t)nx; o->y = (uint8_t)ny; changed(); }
        } else {
            panx -= dx * 32; pany -= dy * 32; clamp_pan();
        }
    }
}

/* ------------------------------------------------------------------ */

static void frame(void) {
    ui_begin(fb, WIN_W, WIN_H);
    ui.blocked = modal != M_NONE;
    ui_rect(0, 0, WIN_W, WIN_H, C_BG);
    canvas_input();
    draw_canvas();
    draw_toolbar();
    draw_panel();
    draw_statusbar();
    if (modal) {
        ui.blocked = 0;   /* the modal itself gets the input */
        if (modal == M_CONFIRM) draw_confirm();
        else if (modal == M_FILE) draw_filebrowser();
        else if (modal == M_HELP) draw_help();
    }
}

static void handle_drop(const char *path) {
    if (!path || !path[0]) return;
    if (has_ext(path, "png;jpg;jpeg;bmp;gif;tga;pcx;scr;psd;pnm;ppm")) load_bg(path);
    else if (has_ext(path, "pat")) file_chosen(F_PAT_LOAD, path);
    else { snprintf(pending_path, sizeof(pending_path), "%s", path); guarded(A_OPEN_PATH); }
}

int app_run(const char *file, const char *bg, const char *patfile) {
    PEvent e;
    int redraw = 1, quit_requests = 0;
    vec_init(&doc);
    vec_default_patterns(&pats);
    for (e.type = 0; e.type < MAX_UNDO; e.type++) { vec_init(&undo_stack[e.type]); vec_init(&redo_stack[e.type]); }
    if (plat_init(WIN_W, WIN_H, "Suvleir 4")) {
        fprintf(stderr, "can't open a window\n");
        return 1;
    }
    clamp_pan();
    if (patfile && vec_load_patterns(&pats, patfile)) set_status("can't load patterns %s", patfile);
    if (file) open_path(file); else update_title();
    if (bg) load_bg(bg);
    if (!file && !bg) set_status("Welcome to Suvleir 4. F1 for help. Load a background with 'BG image', then trace over it.");

    while (running) {
        int got = 0;
        memset(ui.pressed, 0, sizeof(ui.pressed));
        memset(ui.released, 0, sizeof(ui.released));
        ui.wheel = 0; ui.dbl = 0;
        while (plat_next_event(&e)) {
            got = 1;
            ui.mx = e.x; ui.my = e.y;
            if (e.type == EV_QUIT) {
                if (++quit_requests > 1 || !modified) running = 0;
                else guarded(A_QUIT);
                break;
            }
            if (e.type == EV_MDOWN) {
                ui.down[e.button] = 1; ui.pressed[e.button] = 1;
                if (e.button == BTN_LEFT) {
                    uint32_t t = plat_ticks();
                    ui.dbl = (t - last_click_time < 400 && abs(e.x - last_click_x) < 4 && abs(e.y - last_click_y) < 4);
                    last_click_time = t; last_click_x = e.x; last_click_y = e.y;
                }
                break;   /* one press per frame: keeps immediate-mode widgets simple */
            }
            if (e.type == EV_MUP) { ui.down[e.button] = 0; ui.released[e.button] = 1; break; }
            if (e.type == EV_WHEEL) { ui.wheel += e.wheel; break; }
            if (e.type == EV_KEY) { key_event(&e); break; }
            if (e.type == EV_CHAR && modal == M_FILE) {
                size_t l;
                if (fb_fresh) { fb_name[0] = 0; fb_fresh = 0; }
                l = strlen(fb_name);
                if (l + 1 < sizeof(fb_name)) { fb_name[l] = (char)e.ch; fb_name[l + 1] = 0; }
            }
            if (e.type == EV_DROP) { handle_drop(plat_drop_path()); break; }
        }
        if (modal != M_CONFIRM) quit_requests = 0;
        if (got || redraw) {
            frame();
            plat_present(fb);
            redraw = 0;
            /* a click may have changed what the next frame shows */
            if (got) redraw = 1;
        } else {
            plat_wait(50);
            /* status messages fade out */
            if (status[0] && plat_ticks() - status_time >= 8000 && plat_ticks() - status_time < 8100) redraw = 1;
        }
    }
    plat_shutdown();
    return 0;
}
