/*
 * roomed - CJMC room editor. GUI.
 * Platform layer, UI helpers and file browser come from Suvleir 4
 * (splb2-bitmapbg/tools/suvleir4).
 */
#include "app.h"
#include "rooms.h"
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

#define WIN_W 1112
#define WIN_H 700
#define VX 8                     /* canvas: up to 768 x 576, zoom to fit */
#define VY 38
#define VMAXW 768
#define VMAXH 576
#define CELL (8 * zoom)
#define VW (proj.sw * CELL)
#define VH (proj.sh * CELL)
#define SW8 (proj.sw * 8)
#define PX 788                   /* side panel */
#define PW 316
#define SB_Y (WIN_H - 22)
#define MAX_UNDO 100
#define PAL_X (PX + 8)           /* tileset palette: 16 x 16 tiles at 2x */
#define PAL_Y 108
#define LIST_ROWS 11

static uint32_t fb[WIN_W * WIN_H];

/* ------------------------------------------------------------------ */
/* state                                                               */

enum { T_CHAR, T_RECT, T_PATTERN, T_OBJECT, T_SELECT, T_ZIGZAG };
#define AREA_TOOL(t) ((t) == T_RECT || (t) == T_PATTERN || (t) == T_ZIGZAG)
enum { M_NONE, M_CONFIRM, M_FILE, M_HELP, M_NAME, M_SETTINGS };
enum { A_NONE, A_NEW, A_OPEN, A_QUIT, A_OPEN_PATH, A_DEL_ROOM };
enum { F_OPEN, F_SAVE, F_EXPORT_C, F_EXPORT_ASM, F_EXPORT_BIN, F_TILESET, F_BG };

static Project proj;
static int cur = 0;                      /* current room */
static Tileset tiles;
static uint32_t bgimg[MAX_SW * 8 * MAX_SH * 8];
static int has_bg, zoom = 3;
static char bg_loaded[1300];        /* full path of the backdrop in bgimg, "" = none */
static int bg_reload = 1, set_bg_type = 0, bg_choose_type = -1;
static long bg_mtime;
static char tileset_full[1200];
static long tileset_mtime;
static uint32_t last_poll;

static Room undo_stack[MAX_UNDO], redo_stack[MAX_UNDO];
static int undo_room[MAX_UNDO], redo_room[MAX_UNDO];
static int undo_n, redo_n;

static char filename[1024];
static int modified, running = 1;

static int tool = T_CHAR;
static int cur_code = 64, cur_attr = 0x07, pat_w = 2, pat_h = 2, auto_ink = 1, cur_cust = 0;
static int num_a = 1, num_b = 0;     /* the two values of a 'number' object */
static int show_grid = 1, show_coll = 0, show_bg = 1, show_area = 1;
static int sel = -1, list_scroll;

/* canvas drags */
static int dragging, drag_x0, drag_y0, drag_x1, drag_y1, drag_btn;
static int moving, resizing, move_ox, move_oy, paint_lx, paint_ly;
static Obj move_orig;

static uint32_t scr[MAX_SW * 8 * MAX_SH * 8];
static uint8_t coll[MAX_SW * MAX_SH];

static char status[256];
static uint32_t status_time;

static int modal = M_NONE, pending_action = A_NONE;
static char pending_path[1024];
static char name_buf[64];
static char *text_target;            /* M_NAME edits this (room name or export name) */
static int text_size;
static const char *text_title = "Room name";

/* file browser (from Suvleir 4) */
typedef struct { char name[256]; int dir; } Entry;
static Entry *entries;
static int nentries, fb_scroll, fb_sel = -1, fb_purpose, fb_save;
static char fb_dir[1400], fb_name[256], fb_exts[64], fb_title[64];
static int fb_fresh;

static uint32_t last_click_time;
static int last_click_x, last_click_y;

#define ROOM (&proj.rooms[cur])

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

static long file_mtime(const char *p) {
    struct stat st;
    return stat(p, &st) ? 0 : (long)st.st_mtime;
}

static void update_title(void) {
    char t[1200];
    snprintf(t, sizeof(t), "roomed - %s%s", filename[0] ? base_name(filename) : "untitled", modified ? " *" : "");
    plat_set_title(t);
}

static void changed(void) { modified = 1; update_title(); }

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

/* path to store in the project: relative to the project file if possible */
static void rel_path(const char *full, char *out, int outlen) {
    const char *b = base_name(filename);
    size_t dl = (size_t)(b - filename);
    if (filename[0] && dl && !strncmp(full, filename, dl)) snprintf(out, outlen, "%s", full + dl);
    else snprintf(out, outlen, "%s", full);
}

/* ------------------------------------------------------------------ */
/* tileset and background                                              */

static void load_tileset(int quiet) {
    proj_path(filename, proj.tileset, tileset_full, sizeof(tileset_full));
    if (tileset_load(&tiles, tileset_full)) {
        if (!quiet) set_status("can't load the tileset %s (build the game once: it writes gfx/tileset.png)", tileset_full);
        tiles.loaded = 0;
        tileset_mtime = 0;
        return;
    }
    tileset_mtime = file_mtime(tileset_full);
    if (!quiet) set_status("tileset %s", tileset_full);
}

/* the zoom that fits the screen in the canvas (3x for a Spectrum screen) */
static void fit_zoom(void) {
    zoom = VMAXW / (proj.sw * 8);
    if (VMAXH / (proj.sh * 8) < zoom) zoom = VMAXH / (proj.sh * 8);
    if (zoom < 1) zoom = 1;
    if (zoom > 4) zoom = 4;
}

/* the backdrop for the current room (its background type's, or the default) */
static void update_backdrop(void) {
    char full[1300];
    const char *rel = proj_backdrop(&proj, ROOM);
    full[0] = 0;
    if (rel[0]) proj_path(filename, rel, full, sizeof(full));
    /* a per-type backdrop that doesn't exist (yet): the default one */
    if (full[0] && !file_mtime(full) && proj_backdrop_default(&proj)[0])
        proj_path(filename, proj_backdrop_default(&proj), full, sizeof(full));
    if (!bg_reload && !strcmp(full, bg_loaded)) return;
    bg_reload = 0;
    snprintf(bg_loaded, sizeof(bg_loaded), "%s", full);
    has_bg = full[0] && !backdrop_load(&proj, full, bgimg);
    bg_mtime = full[0] ? file_mtime(full) : 0;
}

static void load_background(void) { fit_zoom(); bg_reload = 1; }

/* ------------------------------------------------------------------ */
/* undo                                                                */

static void undo_push(void) {
    if (undo_n == MAX_UNDO) {
        memmove(undo_stack, undo_stack + 1, sizeof(Room) * (MAX_UNDO - 1));
        memmove(undo_room, undo_room + 1, sizeof(int) * (MAX_UNDO - 1));
        undo_n--;
    }
    undo_stack[undo_n] = *ROOM; undo_room[undo_n] = cur; undo_n++;
    redo_n = 0;
}

static void undo(void) {
    if (!undo_n) { set_status("nothing to undo"); return; }
    undo_n--;
    cur = undo_room[undo_n] < proj.nrooms ? undo_room[undo_n] : cur;
    redo_stack[redo_n] = *ROOM; redo_room[redo_n] = cur; redo_n++;
    *ROOM = undo_stack[undo_n];
    sel = -1; changed();
}

static void redo(void) {
    if (!redo_n) { set_status("nothing to redo"); return; }
    redo_n--;
    cur = redo_room[redo_n] < proj.nrooms ? redo_room[redo_n] : cur;
    undo_stack[undo_n] = *ROOM; undo_room[undo_n] = cur; undo_n++;
    *ROOM = redo_stack[redo_n];
    sel = -1; changed();
}

/* ------------------------------------------------------------------ */
/* files                                                               */

static void new_project(void) {
    proj_init(&proj);
    cur = 0; sel = -1; filename[0] = 0; modified = 0;
    undo_n = redo_n = 0;
    load_tileset(1); load_background();
    update_title();
    set_status("new room file");
}

static void open_path(const char *path) {
    char err[256];
    if (proj_load(&proj, path, err, sizeof(err))) { set_status("%s", err); return; }
    snprintf(filename, sizeof(filename), "%s", path);
    cur = 0; sel = -1; modified = 0; undo_n = redo_n = 0;
    load_tileset(1); load_background();
    update_title();
    set_status("opened %s: %d rooms%s", base_name(path), proj.nrooms, tiles.loaded ? "" : " (no tileset!)");
}

static int save_path(const char *path) {
    if (proj_save(&proj, path)) { set_status("can't write %s", path); return -1; }
    snprintf(filename, sizeof(filename), "%s", path);
    modified = 0; update_title();
    set_status("saved %s", base_name(path));
    return 0;
}

static void choose_file(int purpose, int save, const char *title, const char *desc, const char *exts, const char *suggest);
static void goto_room(int n);

static void do_save(int as) {
    if (!as && filename[0]) save_path(filename);
    else choose_file(F_SAVE, 1, "Save rooms", "Room files", "txt", filename[0] ? filename : "rooms.txt");
}

static void run_action(int a);

static int check_ok(void) {
    int rr, oo;
    const char *e = proj_check(&proj, &rr, &oo);
    if (!e) return 1;
    if (rr >= 0) { goto_room(rr); sel = oo; set_status("can't export: room %d object %d: %s", rr, oo, e); }
    else set_status("can't export: %s", e);
    return 0;
}

static void file_chosen(int purpose, const char *path) {
    char rel[1024];
    if ((purpose == F_EXPORT_C || purpose == F_EXPORT_ASM || purpose == F_EXPORT_BIN) && !check_ok()) return;
    switch (purpose) {
    case F_OPEN: open_path(path); break;
    case F_SAVE: if (!save_path(path)) { load_tileset(1); load_background(); if (pending_action) { int a = pending_action; pending_action = 0; run_action(a); } } break;
    case F_EXPORT_C:
        if (proj_export_c(&proj, path)) set_status("can't write %s", path);
        else set_status("exported %d rooms to %s", proj.nrooms, base_name(path));
        break;
    case F_EXPORT_ASM:
        if (proj_export_asm(&proj, path)) set_status("can't write %s", path);
        else set_status("exported %d rooms to %s", proj.nrooms, base_name(path));
        break;
    case F_EXPORT_BIN:
        if (room_export_bin(&proj, ROOM, path)) set_status("can't write %s", path);
        else set_status("room %d -> %s (%d bytes)", cur, base_name(path), room_bytes(&proj, ROOM));
        break;
    case F_TILESET:
        rel_path(path, rel, sizeof(rel));
        snprintf(proj.tileset, sizeof(proj.tileset), "%.511s", rel);
        load_tileset(0); changed();
        break;
    case F_BG:
        rel_path(path, rel, sizeof(rel));
        if (bg_choose_type < 0) snprintf(proj.background, sizeof(proj.background), "%.511s", rel);
        else snprintf(proj.bg_type[bg_choose_type], sizeof(proj.bg_type[0]), "%.511s", rel);
        load_background(); changed();
        break;
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
        char pat[1500];
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
    if (fb_name[0] == PATHSEP || fb_name[0] == '/' || (fb_name[0] && fb_name[1] == ':'))
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

/* ------------------------------------------------------------------ */
/* rooms                                                               */

static void goto_room(int n) {
    if (n < 0 || n >= proj.nrooms) return;
    cur = n; sel = -1; list_scroll = 0;
}

static void add_room(int duplicate) {
    int i;
    if (proj.nrooms == MAX_ROOMS) { set_status("can't have more than %d rooms", MAX_ROOMS); return; }
    for (i = proj.nrooms; i > cur + 1; i--) proj.rooms[i] = proj.rooms[i - 1];
    if (duplicate) {
        char nm[64];
        snprintf(nm, sizeof(nm), "%.50s copy", proj.rooms[cur].name);
        proj.rooms[cur + 1] = proj.rooms[cur];
        memcpy(proj.rooms[cur + 1].name, nm, sizeof(nm));
    } else room_init(&proj.rooms[cur + 1], proj.nrooms);
    proj.nrooms++;
    undo_n = redo_n = 0;      /* room numbers changed */
    goto_room(cur + 1);
    changed();
}

static void delete_room(void) {
    int i;
    if (proj.nrooms == 1) { room_init(&proj.rooms[0], 0); sel = -1; changed(); return; }
    for (i = cur; i < proj.nrooms - 1; i++) proj.rooms[i] = proj.rooms[i + 1];
    proj.nrooms--;
    undo_n = redo_n = 0;
    goto_room(cur >= proj.nrooms ? proj.nrooms - 1 : cur);
    changed();
}

static void move_room(int d) {
    Room t;
    int n = cur + d;
    if (n < 0 || n >= proj.nrooms) return;
    t = proj.rooms[cur]; proj.rooms[cur] = proj.rooms[n]; proj.rooms[n] = t;
    undo_n = redo_n = 0;
    cur = n; changed();
}

/* actions guarded by "unsaved changes?" */
static void run_action(int a) {
    switch (a) {
    case A_NEW: new_project(); break;
    case A_OPEN: choose_file(F_OPEN, 0, "Open rooms", "Room files", "txt", filename); break;
    case A_OPEN_PATH: open_path(pending_path); break;
    case A_QUIT: running = 0; break;
    case A_DEL_ROOM: delete_room(); break;
    }
}

static void guarded(int a) {
    if (modified) { pending_action = a; modal = M_CONFIRM; }
    else run_action(a);
}

/* ------------------------------------------------------------------ */
/* objects                                                             */

static void add_obj(Obj o) {
    if (ROOM->n == MAX_OBJ) { set_status("a room can't have more than %d objects", MAX_OBJ); return; }
    obj_normalize(&o);
    undo_push();
    ROOM->o[ROOM->n++] = o;
    sel = ROOM->n - 1;
    changed();
}

static void delete_sel(void) {
    int i;
    if (sel < 0 || sel >= ROOM->n) return;
    undo_push();
    for (i = sel; i < ROOM->n - 1; i++) ROOM->o[i] = ROOM->o[i + 1];
    ROOM->n--;
    if (sel >= ROOM->n) sel = ROOM->n - 1;
    changed();
}

static void move_sel(int d) {
    Obj t;
    int n = sel + d;
    if (sel < 0 || n < 0 || n >= ROOM->n) return;
    undo_push();
    t = ROOM->o[sel]; ROOM->o[sel] = ROOM->o[n]; ROOM->o[n] = t;
    sel = n; changed();
}

static void nudge_sel(int dx, int dy) {
    Obj *o;
    if (sel < 0 || sel >= ROOM->n) return;
    o = &ROOM->o[sel];
    if (o->x + dx < 0 || o->x2 + dx > proj.sw - 1 || o->y + dy < 0 || o->y2 + dy > proj.sh - 1) return;
    undo_push();
    o->x += dx; o->x2 += dx; o->y += dy; o->y2 += dy;
    changed();
}

/* give the selected object the current tile / attribute / pattern size */
static void apply_to_sel(void) {
    Obj *o;
    if (sel < 0 || sel >= ROOM->n) return;
    undo_push();
    o = &ROOM->o[sel];
    if (IS_CUST(o->type) && proj_cust(&proj, o->type) && proj_cust(&proj, o->type)->number) {
        o->code = (uint8_t)num_a; o->attr = (uint8_t)num_b;
    } else {
        o->code = (uint8_t)cur_code; o->attr = (uint8_t)cur_attr;
    }
    if (o->type == OBJ_PATTERN) { o->w = (uint8_t)pat_w; o->h = (uint8_t)pat_h; }
    changed();
}

/* eyedropper: current tile, attribute (and pattern size) from the object at a cell */
static void pick(int cx, int cy) {
    int k = room_hit(ROOM, cx, cy);
    if (k < 0) { set_status("nothing there to pick"); return; }
    {
        Obj *o = &ROOM->o[k];
        int code, attr;
        obj_cell(o, cx, cy, &code, &attr);
        cur_attr = o->attr;
        if (o->type == OBJ_PATTERN) { cur_code = o->code; pat_w = o->w; pat_h = o->h; }
        else if (o->type == OBJ_ZIGZAG) cur_code = o->code;
        else if (IS_CUST(o->type)) {
            int i;
            const CustObj *cu = proj_cust(&proj, o->type);
            if (cu && cu->number) { num_a = o->code; num_b = o->attr; }
            else { cur_code = o->code; cur_attr = o->attr; }
            for (i = 0; i < proj.ncust; i++) if (proj.cust[i].type == o->type) cur_cust = i;
        } else cur_code = code;
        set_status("picked tile %d, attribute %d from object %d", cur_code, cur_attr, k);
    }
}

static void select_tile(int code) {
    cur_code = code;
    if (auto_ink && tiles.loaded && tiles.used[code]) cur_attr = (cur_attr & ~7) | (tiles.ink[code] & 7);
}

/* ------------------------------------------------------------------ */
/* drawing                                                             */

static uint32_t attr_ink(int a) { return zx_pal[(a & 7) + ((a & 64) ? 8 : 0)]; }
static uint32_t attr_paper(int a) { return zx_pal[((a >> 3) & 7) + ((a & 64) ? 8 : 0)]; }

/* tile `code` at (x, y), each pixel s x s, in attribute a (or its own
   colour on black if a < 0) */
static void draw_tile(int x, int y, int code, int a, int s) {
    int r, b;
    uint32_t ink = a < 0 ? zx_pal[(tiles.ink[code] & 7) + ((tiles.ink[code] & 64) ? 8 : 0)] : attr_ink(a);
    uint32_t paper = a < 0 ? 0 : attr_paper(a);
    for (r = 0; r < 8; r++)
        for (b = 0; b < 8; b++)
            ui_rect(x + b * s, y + r * s, s, s, (tiles.bits[code][r] & (128 >> b)) ? ink : paper);
}

static int canvas_cell(int *cx, int *cy) {
    if (ui.mx < VX || ui.my < VY || ui.mx >= VX + VW || ui.my >= VY + VH) return 0;
    *cx = (ui.mx - VX) / CELL; *cy = (ui.my - VY) / CELL;
    return 1;
}

static void cell_frame(int x0, int y0, int x1, int y1, uint32_t c) {
    ui_frame(VX + x0 * CELL, VY + y0 * CELL, (x1 - x0 + 1) * CELL, (y1 - y0 + 1) * CELL, c);
    ui_frame(VX + x0 * CELL + 1, VY + y0 * CELL + 1, (x1 - x0 + 1) * CELL - 2, (y1 - y0 + 1) * CELL - 2, c);
}

/* the object a drag would create right now */
static Obj drag_obj(void) {
    Obj o;
    memset(&o, 0, sizeof(o));
    o.type = tool == T_RECT ? OBJ_RECT : tool == T_ZIGZAG ? OBJ_ZIGZAG : OBJ_PATTERN;
    o.x = (uint8_t)drag_x0; o.y = (uint8_t)drag_y0; o.x2 = (uint8_t)drag_x1; o.y2 = (uint8_t)drag_y1;
    o.code = (uint8_t)cur_code; o.attr = (uint8_t)cur_attr;
    o.w = (uint8_t)pat_w; o.h = (uint8_t)pat_h;
    obj_normalize(&o);
    return o;
}

static void draw_canvas(void) {
    int x, y, cx, cy;
    static Room tmp;
    const Room *r = ROOM;
    if (dragging && AREA_TOOL(tool) && ROOM->n < MAX_OBJ) {
        tmp = *ROOM;
        tmp.o[tmp.n++] = drag_obj();
        r = &tmp;
    }
    update_backdrop();
    room_render(&proj, r, tiles.loaded ? &tiles : NULL, (show_bg && has_bg) ? bgimg : NULL, scr, coll, -1);
    for (y = 0; y < VH; y++) {
        uint32_t *dst = fb + (VY + y) * WIN_W + VX;
        const uint32_t *src = scr + (y / zoom) * SW8;
        for (x = 0; x < VW; x++) dst[x] = src[x / zoom];
    }
    if (show_coll)
        for (cy = 0; cy < proj.sh; cy++)
            for (cx = 0; cx < proj.sw; cx++)
                if (coll[cy * proj.sw + cx]) {
                    int px = VX + cx * CELL, py = VY + cy * CELL, i;
                    ui_blend_rect(px, py, CELL, CELL, 0xFF2020, 90);
                    for (i = 0; i < CELL; i += 4) ui_pixel(px + i, py + i, 0xFF4040);
                }
    if (show_grid) {
        for (cx = 1; cx < proj.sw; cx++) ui_blend_rect(VX + cx * CELL, VY, 1, VH, 0x808080, 70);
        for (cy = 1; cy < proj.sh; cy++) ui_blend_rect(VX, VY + cy * CELL, VW, 1, 0x808080, 70);
    }
    if (show_area) ui_frame(VX + proj.ax * CELL - 1, VY + proj.ay * CELL - 1, proj.aw * CELL + 2, proj.ah * CELL + 2, C_ACCENT);
    if (sel >= 0 && sel < ROOM->n) {
        Obj *o = &ROOM->o[sel];
        cell_frame(o->x, o->y, o->x2, o->y2, C_ACCENT2);
        if IS_AREA(o->type) ui_rect(VX + (o->x2 + 1) * CELL - 6, VY + (o->y2 + 1) * CELL - 6, 6, 6, C_ACCENT2);
    }
    if (dragging && AREA_TOOL(tool)) {
        Obj o = drag_obj();
        cell_frame(o.x, o.y, o.x2, o.y2, C_ACCENT);
    }
    if (!modal && canvas_cell(&cx, &cy)) ui_frame(VX + cx * CELL, VY + cy * CELL, CELL, CELL, 0xFFFFFF);
    ui_frame(VX - 1, VY - 1, VW + 2, VH + 2, C_BORDER);
}

static void draw_toolbar(void) {
    int x = 8, y = 8;
    ui_rect(0, 0, WIN_W, 32, C_PANEL);
    if (ui_button(x, y, 44, 20, "New", 0)) guarded(A_NEW);
    x += 48;
    if (ui_button(x, y, 52, 20, "Open", 0)) guarded(A_OPEN);
    x += 56;
    if (ui_button(x, y, 52, 20, "Save", 0)) do_save(0);
    x += 56;
    if (ui_button(x, y, 68, 20, "Save as", 0)) do_save(1);
    x += 80;
    if (ui_button(x, y, 76, 20, "Export C", 0)) choose_file(F_EXPORT_C, 1, "Export rooms as C", "C header", "h", "rooms.h");
    x += 80;
    if (ui_button(x, y, 92, 20, "Export asm", 0)) choose_file(F_EXPORT_ASM, 1, "Export rooms as assembler", "Assembler", "asm", "rooms.asm");
    x += 96;
    if (ui_button(x, y, 84, 20, "Room .bin", 0)) {
        char s[64]; snprintf(s, sizeof(s), "room%d.bin", cur);
        choose_file(F_EXPORT_BIN, 1, "Export this room (binary)", "Binary", "bin", s);
    }
    x += 96;
    if (ui_button(x, y, 76, 20, "Settings", 0)) modal = M_SETTINGS;
    x += 88;
    ui_check(x, y + 6, "grid", &show_grid); x += 60;
    ui_check(x, y + 6, "solid", &show_coll); x += 68;
    ui_check(x, y + 6, "backdrop", &show_bg); x += 92;
    ui_check(x, y + 6, "area", &show_area);
    if (ui_button(WIN_W - 56, y, 48, 20, "Help", 0)) modal = M_HELP;
}

/* [-] label value [+]  */
static int spin(int x, int y, const char *label, int *v, int lo, int hi) {
    int ch = 0, lw = ui_text_width(label);
    ui_text(x, y + 4, label, C_DIM);
    x += lw + 4;
    if (ui_button(x, y, 16, 16, "-", 0) && *v > lo) { (*v)--; ch = 1; }
    ui_textf(x + 20, y + 4, C_TEXT, "%3d", *v);
    if (ui_button(x + 46, y, 16, 16, "+", 0) && *v < hi) { (*v)++; ch = 1; }
    return ch;
}

static void draw_panel(void) {
    int x = PX + 8, y = 38, i, k;
    Room *r = ROOM;
    ui_rect(PX, 32, PW, SB_Y - 32, C_PANEL);

    /* room navigation */
    ui_textf(x, y + 2, C_ACCENT, "Room %d/%d", cur, proj.nrooms);
    ui_textf(x + 96, y + 2, C_TEXT, "%.25s", r->name);
    y += 14;
    if (ui_button(x, y, 24, 18, "<", 0)) goto_room(cur - 1);
    if (ui_button(x + 26, y, 24, 18, ">", 0)) goto_room(cur + 1);
    if (ui_button(x + 56, y, 36, 18, "new", 0)) add_room(0);
    if (ui_button(x + 94, y, 36, 18, "dup", 0)) add_room(1);
    if (ui_button(x + 132, y, 36, 18, "del", 0)) {
        if (r->n) { pending_action = A_DEL_ROOM; modal = M_CONFIRM; }
        else delete_room();
    }
    if (ui_button(x + 170, y, 44, 18, "name", 0)) {
        snprintf(name_buf, sizeof(name_buf), "%s", r->name);
        text_target = NULL; text_title = "Room name"; modal = M_NAME;
    }
    if (ui_button(x + 218, y, 40, 18, "<<", 0)) move_room(-1);
    if (ui_button(x + 260, y, 40, 18, ">>", 0)) move_room(1);
    y += 24;

    /* tools */
    if (ui_button(x, y, 56, 20, "1 char", tool == T_CHAR)) tool = T_CHAR;
    if (ui_button(x + 60, y, 56, 20, "2 rect", tool == T_RECT)) tool = T_RECT;
    if (ui_button(x + 120, y, 56, 20, "3 patt", tool == T_PATTERN)) tool = T_PATTERN;
    if (ui_button(x + 180, y, 40, 20, "4 obj", tool == T_OBJECT)) tool = T_OBJECT;
    if (ui_button(x + 224, y, 40, 20, "5 zig", tool == T_ZIGZAG)) tool = T_ZIGZAG;
    if (ui_button(x + 268, y, 32, 20, "S", tool == T_SELECT)) tool = T_SELECT;

    /* tileset: 16 x 16 tiles at 2x */
    ui_rect(PAL_X - 1, PAL_Y - 1, 258, 258, C_BORDER);
    for (k = 0; k < 256; k++) {
        int tx = PAL_X + (k % 16) * 16, ty = PAL_Y + (k / 16) * 16;
        if (tiles.loaded && k < tiles.count) draw_tile(tx, ty, k, -1, 2);
        else ui_rect(tx, ty, 16, 16, 0x101010);
        if (ui_clicked(tx, ty, 16, 16, BTN_LEFT)) select_tile(k);
    }
    /* the run of tiles the current tool uses */
    {
        int n = tool == T_PATTERN ? pat_w * pat_h : tool == T_ZIGZAG ? 2 :
                (tool == T_OBJECT && cur_cust < proj.ncust) ?
                    (proj.cust[cur_cust].number ? 0 : proj.cust[cur_cust].w * proj.cust[cur_cust].h) : 1;
        for (k = cur_code; k < cur_code + n && k < 256; k++)
            ui_frame(PAL_X + (k % 16) * 16, PAL_Y + (k / 16) * 16, 16, 16, k == cur_code ? C_ACCENT : C_ACCENT2);
    }
    if (ui_inside(PAL_X, PAL_Y, 256, 256)) {
        k = (ui.mx - PAL_X) / 16 + (ui.my - PAL_Y) / 16 * 16;
        ui_textf(PAL_X + 264, PAL_Y, C_TEXT, "%3d", k);
    }
    ui_textf(PAL_X + 264, PAL_Y + 14, C_ACCENT, "%3d", cur_code);
    if (cur_code < 64) ui_textf(PAL_X + 264, PAL_Y + 28, C_DIM, "'%c'", cur_code + 32);
    y = PAL_Y + 262;

    /* pattern size (or custom object type) + preview of the figure */
    {
        int fw = pat_w, fh = pat_h;
        if (tool == T_OBJECT) {
            if (!proj.ncust) ui_text(x, y + 4, "no objects declared", C_DIM);
            else {
                CustObj *c;
                if (cur_cust >= proj.ncust) cur_cust = 0;
                c = &proj.cust[cur_cust];
                if (ui_button(x, y, 16, 16, "<", 0)) cur_cust = (cur_cust + proj.ncust - 1) % proj.ncust;
                if (ui_button(x + 18, y, 16, 16, ">", 0)) cur_cust = (cur_cust + 1) % proj.ncust;
                c = &proj.cust[cur_cust];
                ui_textf(x + 40, y + 4, C_ACCENT, "%.10s", c->name);
                ui_textf(x + 40, y + 16, C_DIM, "%dx%d t%d", c->w, c->h, c->type);
                fw = c->w; fh = c->h;
                if (c->number) {                 /* two values instead of tiles */
                    spin(x + 150, y, "a", &num_a, 0, 255);
                    spin(x + 150, y + 18, "b", &num_b, 0, 255);
                    fw = 0;
                }
            }
        } else {
            spin(x, y, "W", &pat_w, 1, proj.compact ? 8 : 16);
            spin(x + 86, y, "H", &pat_h, 1, proj.compact ? 8 : 16);
            if (proj.compact && pat_w > 8) pat_w = 8;
            if (proj.compact && pat_h > 8) pat_h = 8;
            fw = pat_w; fh = pat_h;
        }
        if (tiles.loaded && (tool != T_OBJECT || proj.ncust) && fw) {
            int px = x + 176, s = (fw > 4 || fh > 2) ? 1 : 2, j;
            for (j = 0; j < fh && j * 8 * s < 36; j++)
                for (i = 0; i < fw && i * 8 * s < 120; i++)
                    draw_tile(px + i * 8 * s, y + j * 8 * s, (cur_code + j * fw + i) & 255, cur_attr, s);
        }
    }
    y += 38;

    /* attribute */
    ui_text(x, y + 4, "INK", C_DIM);
    for (i = 0; i < 8; i++) {
        int sx = x + 48 + i * 18;
        ui_rect(sx, y, 16, 16, zx_pal[i + ((cur_attr & 64) ? 8 : 0)]);
        ui_frame(sx, y, 16, 16, (cur_attr & 7) == i ? C_ACCENT : C_BORDER);
        if (ui_clicked(sx, y, 16, 16, BTN_LEFT)) cur_attr = (cur_attr & ~7) | i;
    }
    y += 20;
    ui_text(x, y + 4, "PAPER", C_DIM);
    for (i = 0; i < 8; i++) {
        int sx = x + 48 + i * 18;
        ui_rect(sx, y, 16, 16, zx_pal[i + ((cur_attr & 64) ? 8 : 0)]);
        ui_frame(sx, y, 16, 16, ((cur_attr >> 3) & 7) == i ? C_ACCENT : C_BORDER);
        if (ui_clicked(sx, y, 16, 16, BTN_LEFT)) cur_attr = (cur_attr & ~0x38) | (i << 3);
    }
    {
        int sx = x + 200;
        ui_rect(sx, y - 20, 36, 36, attr_paper(cur_attr));
        if (tiles.loaded) draw_tile(sx + 2, y - 18, cur_code, cur_attr, 4);
        ui_textf(sx + 44, y - 16, C_TEXT, "attr");
        ui_textf(sx + 44, y - 4, C_ACCENT, "%3d", cur_attr);
        ui_textf(sx + 44, y + 8, C_DIM, "$%02X", cur_attr);
    }
    y += 22;
    {
        int b = (cur_attr & 64) != 0, f = (cur_attr & 128) != 0;
        if (ui_check(x, y, proj.solid_bit == 6 ? "BRIGHT = obstacle" : "BRIGHT", &b)) cur_attr ^= 64;
        if (ui_check(x + 160, y, proj.solid_bit == 7 ? "FLASH = obstacle" : "FLASH", &f)) cur_attr ^= 128;
    }
    y += 16;
    ui_check(x, y, "ink from tile colour", &auto_ink);
    y += 18;

    /* header bytes (as many as the settings say), then the settings summary */
    {
        int v[MAX_HDR], ch = 0, rows = (proj.hdr_n + 3) / 4, j;
        for (i = 0; i < MAX_HDR; i++) v[i] = r->hdr[i];
        if (!proj.hdr_n) ui_text(x, y + 4, "hdr  (no header bytes)", C_DIM);
        else ui_text(x, y + 4, "hdr", C_DIM);
        for (i = 0; i < proj.hdr_n; i++) {
            int sx = x + 30 + (i % 4) * 70, sy = y + (i / 4) * 18;
            if (ui_button(sx, sy, 16, 16, "-", 0) && v[i] > 0) { v[i]--; ch = 1; }
            ui_textf(sx + 18, sy + 4, i ? C_TEXT : C_ACCENT, "%3d", v[i]);
            if (ui_button(sx + 44, sy, 16, 16, "+", 0) && v[i] < 255) { v[i]++; ch = 1; }
        }
        if (ch) { undo_push(); for (j = 0; j < MAX_HDR; j++) r->hdr[j] = (uint8_t)v[j]; changed(); }
        y += (rows > 1 ? rows : 1) * 18 + 2;
    }
    ui_textf(x, y + 4, C_DIM, "screen %dx%d  area %d,%d %dx%d", proj.sw, proj.sh, proj.ax, proj.ay, proj.aw, proj.ah);
    if (ui_button(x + 240, y, 60, 16, "settings", 0)) modal = M_SETTINGS;
    y += 22;

    /* object list */
    ui_textf(x, y, C_DIM, "objects: %d   (%d bytes)", r->n, room_bytes(&proj, r));
    y += 12;
    ui_rect(x, y, PW - 16, LIST_ROWS * 12 + 4, C_PANEL2);
    if (ui_inside(x, y, PW - 16, LIST_ROWS * 12) && ui.wheel) list_scroll -= ui.wheel * 2;
    if (sel >= 0 && (sel < list_scroll || sel >= list_scroll + LIST_ROWS) && !ui.wheel) list_scroll = sel - LIST_ROWS / 2;
    if (list_scroll > r->n - LIST_ROWS) list_scroll = r->n - LIST_ROWS;
    if (list_scroll < 0) list_scroll = 0;
    for (i = 0; i < LIST_ROWS && list_scroll + i < r->n; i++) {
        int kk = list_scroll + i, ry = y + 2 + i * 12;
        Obj *o = &r->o[kk];
        if (kk == sel) ui_rect(x, ry, PW - 16, 12, 0x5A4A20);
        if (o->type == OBJ_CHAR)
            ui_textf(x + 4, ry + 2, C_TEXT, "%3d char    %2d,%2d        c%3d a%3d", kk, o->x, o->y, o->code, o->attr);
        else if (o->type == OBJ_RECT)
            ui_textf(x + 4, ry + 2, C_TEXT, "%3d rect    %2d,%2d-%2d,%2d  c%3d a%3d", kk, o->x, o->y, o->x2, o->y2, o->code, o->attr);
        else if (o->type == OBJ_PATTERN)
            ui_textf(x + 4, ry + 2, C_TEXT, "%3d pat%dx%d  %2d,%2d-%2d,%2d  c%3d a%3d", kk, o->w, o->h, o->x, o->y, o->x2, o->y2, o->code, o->attr);
        else if (o->type == OBJ_ZIGZAG)
            ui_textf(x + 4, ry + 2, C_TEXT, "%3d zigzag  %2d,%2d-%2d,%2d  c%3d a%3d", kk, o->x, o->y, o->x2, o->y2, o->code, o->attr);
        else
            ui_textf(x + 4, ry + 2, C_ACCENT2, "%3d %-7.7s %2d,%2d        %s%3d %s%3d", kk, obj_name(&proj, o), o->x, o->y,
                     (proj_cust(&proj, o->type) && proj_cust(&proj, o->type)->number) ? "a" : "c", o->code,
                     (proj_cust(&proj, o->type) && proj_cust(&proj, o->type)->number) ? "b" : "a", o->attr);
        if (ui_clicked(x, ry, PW - 16, 12, BTN_LEFT)) sel = kk;
    }
    y += LIST_ROWS * 12 + 8;
    if (ui_button(x, y, 52, 18, "up", 0)) move_sel(-1);
    if (ui_button(x + 56, y, 52, 18, "down", 0)) move_sel(1);
    if (ui_button(x + 112, y, 52, 18, "del", 0)) delete_sel();
    if (ui_button(x + 168, y, 132, 18, "apply tile+attr", 0)) apply_to_sel();
}

static void draw_statusbar(void) {
    int cx, cy, total = 0, i;
    ui_rect(0, SB_Y, WIN_W, 22, C_PANEL2);
    for (i = 0; i < proj.nrooms; i++) total += room_bytes(&proj, &proj.rooms[i]);
    if (canvas_cell(&cx, &cy)) {
        int k = room_hit(ROOM, cx, cy);
        if (k >= 0) {
            int code, attr;
            obj_cell(&ROOM->o[k], cx, cy, &code, &attr);
            ui_textf(8, SB_Y + 7, C_TEXT, "x %2d  y %2d   obj %d: tile %3d attr %3d%s", cx, cy, k, code, attr, coll[cy * proj.sw + cx] ? "  SOLID" : "");
        } else ui_textf(8, SB_Y + 7, C_TEXT, "x %2d  y %2d%s", cx, cy, (cx < proj.ax || cy < proj.ay || cx >= proj.ax + proj.aw || cy >= proj.ay + proj.ah) ? "   (outside the area)" : "");
    }
    ui_textf(400, SB_Y + 7, C_DIM, "%.56s", status);
    ui_textf(WIN_W - 250, SB_Y + 7, C_ACCENT, "room %d bytes, all %d bytes", room_bytes(&proj, ROOM), total);
}

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
    if (pending_action == A_DEL_ROOM) {
        begin_modal(380, 110, "Delete room", &x, &y);
        ui_textf(x + 16, y + 36, C_TEXT, "Delete room %d (%d objects)?", cur, ROOM->n);
        if (ui_button(x + 16, y + 70, 100, 24, "Delete", 0)) { modal = M_NONE; pending_action = 0; delete_room(); }
        else if (ui_button(x + 264, y + 70, 100, 24, "Cancel", 0)) { modal = M_NONE; pending_action = 0; }
        return;
    }
    begin_modal(380, 110, "Unsaved changes", &x, &y);
    ui_textf(x + 16, y + 36, C_TEXT, "Save changes to %s?", filename[0] ? base_name(filename) : "untitled");
    if (ui_button(x + 16, y + 70, 100, 24, "Save", 0)) {
        modal = M_NONE;
        if (filename[0]) { if (!save_path(filename)) { int a = pending_action; pending_action = 0; run_action(a); } }
        else do_save(1);
    } else if (ui_button(x + 140, y + 70, 100, 24, "Discard", 0)) {
        int a = pending_action;
        modal = M_NONE; modified = 0; pending_action = 0; run_action(a);
    } else if (ui_button(x + 264, y + 70, 100, 24, "Cancel", 0)) {
        modal = M_NONE; pending_action = 0;
    }
}

static void text_accept(void) {
    char *t = text_target ? text_target : ROOM->name;
    int n = text_target ? text_size : (int)sizeof(ROOM->name);
    snprintf(t, n, "%s", name_buf);
    modal = text_target ? M_SETTINGS : M_NONE;
    text_target = NULL;
    changed();
}

static void text_cancel(void) { modal = text_target ? M_SETTINGS : M_NONE; text_target = NULL; }

static void draw_name(void) {
    int x, y;
    begin_modal(380, 100, text_title, &x, &y);
    ui_rect(x + 16, y + 34, 348, 16, 0x15181E);
    ui_frame(x + 16, y + 34, 348, 16, C_ACCENT);
    ui_textf(x + 20, y + 38, C_TEXT, "%.42s_", name_buf);
    if (ui_button(x + 160, y + 66, 96, 22, "OK", 0)) text_accept();
    else if (ui_button(x + 268, y + 66, 96, 22, "Cancel", 0)) text_cancel();
}

/* settings: everything that ties the rooms to one game */
static void draw_settings(void) {
    int x, y, yy, v[8], ch = 0;
    char lbl[32];
    begin_modal(600, 470, "Settings (saved in the room file)", &x, &y);
    yy = y + 32;
    v[0] = proj.sw; v[1] = proj.sh; v[2] = proj.ax; v[3] = proj.ay; v[4] = proj.aw; v[5] = proj.ah;
    v[6] = proj.hdr_n; v[7] = proj.solid_bit;
    ui_text(x + 16, yy + 4, "screen (cells)", C_TEXT);
    ch |= spin(x + 160, yy, "w", &v[0], 1, MAX_SW);
    ch |= spin(x + 250, yy, "h", &v[1], 1, MAX_SH);
    ui_textf(x + 350, yy + 4, C_DIM, "%dx%d px, zoom %dx", v[0] * 8, v[1] * 8, zoom);
    yy += 22;
    ui_text(x + 16, yy + 4, "area (editable)", C_TEXT);
    ch |= spin(x + 160, yy, "x", &v[2], 0, v[0] - 1);
    ch |= spin(x + 250, yy, "y", &v[3], 0, v[1] - 1);
    ch |= spin(x + 340, yy, "w", &v[4], 1, v[0]);
    ch |= spin(x + 430, yy, "h", &v[5], 1, v[1]);
    yy += 22;
    ui_text(x + 16, yy + 4, "header bytes", C_TEXT);
    ch |= spin(x + 160, yy, "n", &v[6], 0, MAX_HDR);
    ui_text(x + 250, yy + 4, "byte 0 = background type", C_DIM);
    yy += 22;
    ui_text(x + 16, yy + 4, "solid bit", C_TEXT);
    ch |= spin(x + 160, yy, "b", &v[7], -1, 7);
    ui_text(x + 250, yy + 4, v[7] < 0 ? "no attribute bit means solid" : v[7] == 6 ? "BRIGHT means obstacle" :
            v[7] == 7 ? "FLASH means obstacle" : "attribute bit means obstacle", C_DIM);
    if (ch) {
        proj.sw = v[0]; proj.sh = v[1];
        proj.ax = clampi(v[2], 0, v[0] - 1); proj.ay = clampi(v[3], 0, v[1] - 1);
        proj.aw = clampi(v[4], 1, v[0] - proj.ax); proj.ah = clampi(v[5], 1, v[1] - proj.ay);
        proj.hdr_n = v[6]; proj.solid_bit = v[7];
        load_background(); changed();
    }
    yy += 22;
    ui_text(x + 16, yy + 4, "encoding", C_TEXT);
    if (ui_button(x + 160, yy, 70, 18, "plain", !proj.compact)) { proj.compact = 0; changed(); }
    if (ui_button(x + 234, yy, 70, 18, "compact", proj.compact)) { proj.compact = 1; changed(); }
    {
        int i, ox = x + 316;
        char t[24];
        ui_text(ox, yy + 4, "bg params:", C_DIM); ox += 88;
        for (i = 0; i < MAX_BG; i++) if (proj.bg_params[i] >= 0) {
            snprintf(t, sizeof(t), "%d:%d", i, proj.bg_params[i]);
            ox += ui_text(ox, yy + 4, t, C_ACCENT) + 8;
        }
        if (ox == x + 404) ui_text(ox, yy + 4, "(all)", C_DIM);
    }
    yy += 28;
    ui_text(x + 16, yy + 4, "export name", C_TEXT);
    ui_textf(x + 160, yy + 4, C_ACCENT, "%s", proj.export_name);
    ui_textf(x + 300, yy + 4, C_DIM, "-> %s_data, %s_off", proj.export_name, proj.export_name);
    if (ui_button(x + 520, yy, 64, 18, "change", 0)) {
        snprintf(name_buf, sizeof(name_buf), "%s", proj.export_name);
        text_target = proj.export_name; text_size = sizeof(proj.export_name); text_title = "Export name";
        modal = M_NAME;
    }
    yy += 28;
    ui_text(x + 16, yy + 4, "tileset", C_TEXT);
    ui_textf(x + 100, yy + 4, tiles.loaded ? C_ACCENT : C_RED, "%.44s", proj.tileset);
    if (ui_button(x + 470, yy, 114, 18, "choose...", 0)) choose_file(F_TILESET, 0, "Tileset (PNG, 8x8 tiles row by row)", "Images", "png;bmp;gif", tileset_full);
    yy += 16;
    ui_textf(x + 100, yy + 4, C_DIM, "%d tiles, 8x8, numbered row by row", tiles.loaded ? tiles.count : 0);
    yy += 26;
    ui_text(x + 16, yy + 4, "backdrop", C_TEXT);
    ui_textf(x + 100, yy + 4, C_ACCENT, "%.36s", proj.background[0] ? proj.background : "(none: black)");
    if (ui_button(x + 410, yy, 90, 18, "choose...", 0)) { bg_choose_type = -1; choose_file(F_BG, 0, "Backdrop (screen-sized picture)", "Images", "png;bmp;gif;jpg", NULL); }
    if (ui_button(x + 506, yy, 78, 18, "none", 0)) { proj.background[0] = 0; load_background(); changed(); }
    yy += 16;
    ui_text(x + 100, yy + 4, "drawn under every room, unless its type has its own:", C_DIM);
    yy += 22;
    snprintf(lbl, sizeof(lbl), "type");
    spin(x + 16, yy, lbl, &set_bg_type, 0, MAX_BG - 1);
    ui_textf(x + 130, yy + 4, C_ACCENT, "%.32s", proj.bg_type[set_bg_type][0] ? proj.bg_type[set_bg_type] : "(the default)");
    if (ui_button(x + 410, yy, 90, 18, "choose...", 0)) { bg_choose_type = set_bg_type; choose_file(F_BG, 0, "Backdrop for this background type", "Images", "png;bmp;gif;jpg", NULL); }
    if (ui_button(x + 506, yy, 78, 18, "default", 0)) { proj.bg_type[set_bg_type][0] = 0; load_background(); changed(); }
    yy += 26;
    ui_text(x + 16, yy, "Paths are relative to the room file. Each room shows the backdrop", C_DIM);
    ui_text(x + 16, yy + 12, "of its header byte 0 (its type); %1-%7 = that header byte.", C_DIM);
    yy += 32;
    ui_text(x + 16, yy + 4, "objects", C_TEXT);
    if (!proj.ncust) ui_text(x + 100, yy + 4, "(none)", C_DIM);
    {
        int i, ox = x + 100;
        for (i = 0; i < proj.ncust; i++) {
            char t[64];
            snprintf(t, sizeof(t), "%d %s %dx%d%s%s", proj.cust[i].type, proj.cust[i].name, proj.cust[i].w, proj.cust[i].h,
                     proj.cust[i].solid ? " solid" : "", proj.cust[i].number ? " number" : "");
            if (ox + ui_text_width(t) > x + 584) { ox = x + 100; yy += 12; }
            ox += ui_text(ox, yy + 4, t, C_ACCENT) + 16;
        }
    }
    yy += 16;
    ui_text(x + 100, yy + 4, "declare: object TYPE NAME W H [solid] [number]", C_DIM);
    yy += 14;
    ui_text(x + 100, yy + 4, "and header bytes per background type: bg_params TYPE N", C_DIM);
    if (ui_button(x + 250, y + 438, 100, 22, "Close", 0)) modal = M_NONE;
}

static void draw_filebrowser(void) {
    int x, y, i, rows = 24, lx, ly, lw;
    begin_modal(620, 420, fb_title, &x, &y);
    ui_textf(x + 12, y + 30, C_DIM, "%.66s", fb_dir);
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
        "TOOLS     1 char: click (or drag to paint) one cell per object",
        "          2 rect: drag a rectangle filled with the tile",
        "          3 pattern: drag a rectangle filled with a WxH figure made of",
        "            tiles CODE .. CODE+W*H-1 (row by row). Set W and H in the panel",
        "          4 object: click to place a custom object (declared in the file,",
        "            e.g. CJMC's crate); < > in the panel choose which one",
        "          5 zigzag: drag a rectangle filled with tiles CODE and CODE+1 taking",
        "            turns, each row starting one cell on (staggered bricks)",
        "          S select: click an object, drag to move it, drag its corner",
        "            (the small square) to resize it",
        "          right click anywhere: pick tile, attribute (and W, H) from the cell",
        "ATTRIBUTE the solid bit (Settings; BRIGHT by default) makes cells obstacles",
        "          'solid' (toolbar) shows which cells will be obstacles",
        "OBJECTS   Del delete   arrows move   Ctrl+up/down: earlier/later in the list",
        "          A: apply the current tile + attribute (+ W, H) to the selection",
        "ROOMS     PgUp / PgDn previous / next room",
        "FILES     Ctrl+N new  Ctrl+O open  Ctrl+S save  Ctrl+Shift+S save as",
        "          Ctrl+E export C   R reload the tileset (also automatic)",
        "EDIT      Ctrl+Z undo   Ctrl+Y redo     G grid  K solid  B backdrop",
        "SETTINGS  screen and area size, header bytes, solid bit, export name,",
        "          tileset, backdrops per background type: all saved in the file",
        "",
        "Objects are drawn in list order, the way the engine's driver draws them.",
        "The tileset reloads by itself when a build rewrites it.",
    };
    begin_modal(680, 338, "roomed - help", &x, &y);
    for (i = 0; i < (int)(sizeof(lines) / sizeof(lines[0])); i++) ui_text(x + 14, y + 32 + i * 12, lines[i], i < 21 ? C_TEXT : C_DIM);
    if (ui_button(x + 290, y + 306, 100, 22, "Close", 0)) modal = M_NONE;
}

/* ------------------------------------------------------------------ */
/* canvas input                                                        */

static void canvas_input(void) {
    int cx = 0, cy = 0, in = canvas_cell(&cx, &cy);
    if (modal) return;
    cx = clampi(cx, 0, proj.sw - 1); cy = clampi(cy, 0, proj.sh - 1);

    /* end of a drag (the last position first: moves and the release may
       arrive in the same frame) */
    if (dragging && ui.released[drag_btn]) {
        drag_x1 = clampi(cx, proj.ax, proj.ax + proj.aw - 1);
        drag_y1 = clampi(cy, proj.ay, proj.ay + proj.ah - 1);
        if AREA_TOOL(tool) add_obj(drag_obj());
        dragging = 0;
    }
    if (moving && ui.released[BTN_LEFT]) {
        Obj *o = &ROOM->o[sel];
        if (resizing) {
            o->x2 = (uint8_t)clampi(cx, o->x, proj.sw - 1);
            o->y2 = (uint8_t)clampi(cy, o->y, proj.sh - 1);
        } else {
            int w = move_orig.x2 - move_orig.x, h = move_orig.y2 - move_orig.y;
            int nx = clampi(cx - move_ox, 0, proj.sw - 1 - w), ny = clampi(cy - move_oy, 0, proj.sh - 1 - h);
            o->x = (uint8_t)nx; o->y = (uint8_t)ny; o->x2 = (uint8_t)(nx + w); o->y2 = (uint8_t)(ny + h);
        }
        moving = resizing = 0;
        if (sel >= 0 && memcmp(&ROOM->o[sel], &move_orig, sizeof(Obj))) changed();
        else if (undo_n) undo_n--;          /* just a click: no undo step */
    }

    if (dragging) {
        drag_x1 = clampi(cx, proj.ax, proj.ax + proj.aw - 1);
        drag_y1 = clampi(cy, proj.ay, proj.ay + proj.ah - 1);
        if (tool == T_CHAR && in && (drag_x1 != paint_lx || drag_y1 != paint_ly)) {
            Obj o;
            memset(&o, 0, sizeof(o));
            o.type = OBJ_CHAR; o.x = (uint8_t)drag_x1; o.y = (uint8_t)drag_y1;
            o.code = (uint8_t)cur_code; o.attr = (uint8_t)cur_attr;
            add_obj(o);
            paint_lx = drag_x1; paint_ly = drag_y1;
        }
        return;
    }
    if (moving && sel >= 0) {
        Obj *o = &ROOM->o[sel];
        if (resizing) {
            o->x2 = (uint8_t)clampi(cx, o->x, proj.sw - 1);
            o->y2 = (uint8_t)clampi(cy, o->y, proj.sh - 1);
        } else {
            int w = move_orig.x2 - move_orig.x, h = move_orig.y2 - move_orig.y;
            int nx = clampi(cx - move_ox, 0, proj.sw - 1 - w), ny = clampi(cy - move_oy, 0, proj.sh - 1 - h);
            o->x = (uint8_t)nx; o->y = (uint8_t)ny; o->x2 = (uint8_t)(nx + w); o->y2 = (uint8_t)(ny + h);
        }
        return;
    }
    if (!in) return;

    if (ui.pressed[BTN_RIGHT]) { pick(cx, cy); return; }
    if (!ui.pressed[BTN_LEFT]) return;

    if (tool == T_SELECT) {
        Obj *o = (sel >= 0 && sel < ROOM->n) ? &ROOM->o[sel] : NULL;
        if (o && IS_AREA(o->type) && cx == o->x2 && cy == o->y2 &&
            (ui.mx - VX) % CELL >= CELL - 8 && (ui.my - VY) % CELL >= CELL - 8) {
            resizing = 1;
        } else {
            sel = room_hit(ROOM, cx, cy);
            if (sel < 0) return;
            o = &ROOM->o[sel];
        }
        undo_push();
        moving = 1; move_orig = *o;
        move_ox = cx - o->x; move_oy = cy - o->y;
        return;
    }
    if (cx < proj.ax || cy < proj.ay || cx >= proj.ax + proj.aw || cy >= proj.ay + proj.ah) {
        set_status("outside the area (set it in the panel)");
        return;
    }
    if (tool == T_OBJECT) {
        Obj o;
        CustObj *c;
        if (!proj.ncust) { set_status("no custom objects: declare them with \"object TYPE NAME W H\" in the room file"); return; }
        c = &proj.cust[cur_cust];
        memset(&o, 0, sizeof(o));
        o.type = (uint8_t)c->type; o.w = (uint8_t)c->w; o.h = (uint8_t)c->h;
        o.x = (uint8_t)clampi(cx, proj.ax, proj.ax + proj.aw - c->w);
        o.y = (uint8_t)clampi(cy, proj.ay, proj.ay + proj.ah - c->h);
        o.code = (uint8_t)cur_code; o.attr = (uint8_t)cur_attr;
        if (c->number) { o.code = (uint8_t)num_a; o.attr = (uint8_t)num_b; }
        add_obj(o);
        return;
    }
    dragging = 1; drag_btn = BTN_LEFT;
    drag_x0 = drag_x1 = cx; drag_y0 = drag_y1 = cy;
    if (tool == T_CHAR) {
        Obj o;
        memset(&o, 0, sizeof(o));
        o.type = OBJ_CHAR; o.x = (uint8_t)cx; o.y = (uint8_t)cy; o.code = (uint8_t)cur_code; o.attr = (uint8_t)cur_attr;
        add_obj(o);
        paint_lx = cx; paint_ly = cy;
    }
}

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
    if (modal == M_NAME) {
        if (k == K_ESC) text_cancel();
        else if (k == K_ENTER) text_accept();
        else if (k == K_BACKSPACE) { size_t l = strlen(name_buf); if (l) name_buf[l - 1] = 0; }
        return;
    }
    if (modal == M_HELP) { if (k == K_ESC || k == K_ENTER || k == K_F1) modal = M_NONE; return; }
    if (modal == M_SETTINGS) { if (k == K_ESC || k == K_ENTER) modal = M_NONE; return; }
    if (modal == M_CONFIRM) { if (k == K_ESC) { modal = M_NONE; pending_action = 0; } return; }

    if (ctrl) {
        switch (k) {
        case 'n': guarded(A_NEW); return;
        case 'o': guarded(A_OPEN); return;
        case 's': do_save(shift != 0); return;
        case 'e': choose_file(F_EXPORT_C, 1, "Export rooms as C", "C header", "h", "rooms.h"); return;
        case 'z': if (shift) redo(); else undo(); return;
        case 'y': redo(); return;
        case K_UP: move_sel(-1); return;
        case K_DOWN: move_sel(1); return;
        }
        return;
    }
    switch (k) {
    case K_F1: modal = M_HELP; return;
    case K_ESC: sel = -1; dragging = 0; return;
    case '1': tool = T_CHAR; return;
    case '2': tool = T_RECT; return;
    case '3': tool = T_PATTERN; return;
    case '4': tool = T_OBJECT; return;
    case '5': tool = T_ZIGZAG; return;
    case 's': tool = T_SELECT; return;
    case 'g': show_grid = !show_grid; return;
    case 'k': show_coll = !show_coll; return;
    case 'b': show_bg = !show_bg; return;
    case 'a': apply_to_sel(); return;
    case 'r': load_tileset(0); return;
    case K_PGUP: goto_room(cur - 1); return;
    case K_PGDN: goto_room(cur + 1); return;
    case K_DELETE: case K_BACKSPACE: delete_sel(); return;
    case K_LEFT: nudge_sel(-1, 0); return;
    case K_RIGHT: nudge_sel(1, 0); return;
    case K_UP: nudge_sel(0, -1); return;
    case K_DOWN: nudge_sel(0, 1); return;
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
        ui.blocked = 0;
        if (modal == M_CONFIRM) draw_confirm();
        else if (modal == M_FILE) draw_filebrowser();
        else if (modal == M_HELP) draw_help();
        else if (modal == M_NAME) draw_name();
        else if (modal == M_SETTINGS) draw_settings();
    }
}

static void handle_drop(const char *path) {
    if (!path || !path[0]) return;
    if (has_ext(path, "png;bmp;gif")) file_chosen(F_TILESET, path);
    else { snprintf(pending_path, sizeof(pending_path), "%s", path); guarded(A_OPEN_PATH); }
}

int app_run(const char *file) {
    PEvent e;
    int redraw = 1, quit_requests = 0;
    if (plat_init(WIN_W, WIN_H, "roomed")) {
        fprintf(stderr, "can't open a window\n");
        return 1;
    }
    if (file) {
        struct stat st;
        if (stat(file, &st)) {            /* a new file with that name */
            new_project();
            snprintf(filename, sizeof(filename), "%s", file);
            load_tileset(0); load_background(); update_title();
        } else open_path(file);
    } else new_project();
    if (!tiles.loaded) set_status("no tileset (%s): build the game once, it writes gfx/tileset.png", tileset_full);
    else if (!file) set_status("roomed: F1 for help");

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
                break;
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
            if (e.type == EV_CHAR && modal == M_NAME) {
                size_t l = strlen(name_buf);
                if (l + 1 < sizeof(name_buf)) { name_buf[l] = (char)e.ch; name_buf[l + 1] = 0; }
            }
            if (e.type == EV_DROP) { handle_drop(plat_drop_path()); break; }
        }
        if (modal != M_CONFIRM) quit_requests = 0;
        if (got || redraw) {
            frame();
            plat_present(fb);
            redraw = 0;
            if (got) redraw = 1;
        } else {
            plat_wait(50);
            /* the game's build rewrites the tileset: pick it up */
            if (plat_ticks() - last_poll > 1000) {
                last_poll = plat_ticks();
                if (tileset_full[0] && file_mtime(tileset_full) != tileset_mtime && file_mtime(tileset_full)) {
                    load_tileset(1);
                    set_status("tileset changed on disk: reloaded");
                    redraw = 1;
                }
                /* backdrops too (or a per-room preview that has just appeared) */
                {
                    char full[1300];
                    const char *rel = proj_backdrop(&proj, ROOM);
                    full[0] = 0;
                    if (rel[0]) proj_path(filename, rel, full, sizeof(full));
                    if ((full[0] && file_mtime(full) && strcmp(full, bg_loaded)) ||
                        (bg_loaded[0] && file_mtime(bg_loaded) != bg_mtime)) { bg_reload = 1; redraw = 1; }
                }
            }
            if (status[0] && plat_ticks() - status_time >= 8000 && plat_ticks() - status_time < 8100) redraw = 1;
        }
    }
    plat_shutdown();
    return 0;
}
