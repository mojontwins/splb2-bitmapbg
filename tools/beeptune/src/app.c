/*
 * Beeptune - the tracker. Same look, platform layer and widgets as Suvleir 4.
 * Copyleft The Mojon Twins.
 */
#include "app.h"
#include "song.h"
#include "audio.h"
#include "plat.h"
#include "ui.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
#define SB_Y (WIN_H - 22)

/* the pattern editor */
#define EX 8
#define EY 40
#define EW 424
#define EH (SB_Y - 48)
#define ROW_H 13
#define ROWS_Y (EY + 38)
#define VIS_ROWS ((EH - 42) / ROW_H)
#define COL_A (EX + 48)
#define COL_B (EX + 236)
#define COL_W 180

/* side panels */
#define SX 440
#define SW 290
#define OX 738
#define OW 294

#define RATE 44100
#define MAX_UNDO 64

static uint32_t fb[WIN_W * WIN_H];

/* ------------------------------------------------------------------ */
/* state                                                               */

enum { M_NONE, M_CONFIRM, M_FILE, M_HELP };
enum { A_NONE, A_NEW, A_OPEN, A_QUIT, A_OPEN_PATH };
enum { F_OPEN, F_SAVE, F_ASM, F_C, F_TXT, F_WAV };

static Song song;
static Song *undo_stack[MAX_UNDO], *redo_stack[MAX_UNDO];
static int undo_n, redo_n;

static char filename[1024];
static int modified, running = 1;

static int cur_pat, cur_row, cur_ch, cur_ord, scroll;
static int octave = 4, edit_step = 1, new_rows = 32;
static int play_loop = 1, follow = 1;
static Pattern clip;
static int has_clip;

/* playback */
static int playing, play_only_pat;
static uint32_t play_start;
static size_t play_n;
static RowPos *play_rows;
static int play_nrows, play_idx = -1;

static char status[256];
static uint32_t status_time;
static int modal = M_NONE, pending_action = A_NONE;
static char pending_path[1024];

typedef struct { char name[256]; int dir; } Entry;
static Entry *entries;
static int nentries, fb_scroll, fb_sel = -1, fb_purpose, fb_save;
static char fb_dir[1400], fb_name[256], fb_exts[64], fb_title[64];
static int fb_fresh;

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
        size_t l = strcspn(s, ";"), i;
        if (strlen(dot) == l) {
            for (i = 0; i < l && tolower((unsigned char)dot[i]) == tolower((unsigned char)s[i]); i++);
            if (i == l) return 1;
        }
        s += l; if (*s) s++;
    }
    return 0;
}

static void update_title(void) {
    char t[1200];
    snprintf(t, sizeof(t), "Beeptune - %s%s", filename[0] ? base_name(filename) : "untitled", modified ? " *" : "");
    plat_set_title(t);
}

static Pattern *pat(void) { return &song.pat[cur_pat]; }

/* keep the cursor, pattern and order entry valid */
static void clamp_cursor(void) {
    if (cur_pat >= song.npat) cur_pat = song.npat - 1;
    if (cur_row >= pat()->rows) cur_row = pat()->rows - 1;
    if (cur_row < 0) cur_row = 0;
    if (cur_ord >= song.nord) cur_ord = song.nord - 1;
    if (cur_ord < 0) cur_ord = 0;
    if (scroll > pat()->rows - VIS_ROWS) scroll = pat()->rows - VIS_ROWS;
    if (scroll < 0) scroll = 0;
}

/* scroll so that the cursor is on view */
static void show_cursor(void) {
    clamp_cursor();
    if (scroll > cur_row) scroll = cur_row;
    if (scroll < cur_row - VIS_ROWS + 1) scroll = cur_row - VIS_ROWS + 1;
    clamp_cursor();
}

static void changed(void) {
    if (!modified) { modified = 1; update_title(); }
}

static void undo_push(void) {
    int i;
    if (undo_n == MAX_UNDO) { free(undo_stack[0]); for (i = 1; i < MAX_UNDO; i++) undo_stack[i - 1] = undo_stack[i]; undo_n--; }
    undo_stack[undo_n] = (Song *)malloc(sizeof(Song));
    *undo_stack[undo_n++] = song;
    for (i = 0; i < redo_n; i++) free(redo_stack[i]);
    redo_n = 0;
}

static void do_undo(void) {
    if (!undo_n) { set_status("nothing to undo"); return; }
    redo_stack[redo_n] = (Song *)malloc(sizeof(Song));
    *redo_stack[redo_n++] = song;
    song = *undo_stack[--undo_n];
    free(undo_stack[undo_n]);
    clamp_cursor(); changed();
}

static void do_redo(void) {
    if (!redo_n) { set_status("nothing to redo"); return; }
    undo_stack[undo_n] = (Song *)malloc(sizeof(Song));
    *undo_stack[undo_n++] = song;
    song = *redo_stack[--redo_n];
    free(redo_stack[redo_n]);
    clamp_cursor(); changed();
}

static size_t song_bytes(void) { return song_encode(&song, NULL, 0); }

static int song_seconds(void) {
    int i, rows = 0;
    for (i = 0; i < song.nord; i++) rows += song.pat[song.ord[i]].rows;
    return rows * song.speed / 50;
}

/* is pattern p in the order list? */
static int pat_used(int p) {
    int i;
    for (i = 0; i < song.nord; i++) if (song.ord[i] == p) return 1;
    return 0;
}

/* ------------------------------------------------------------------ */
/* playback                                                            */

static void stop(void) {
    audio_stop();
    playing = 0;
    play_idx = -1;
}

static void play(int only_pat, int from_ord) {
    int16_t *smp;
    char msg[200];
    stop();
    free(play_rows); play_rows = NULL;
    play_n = song_render(&song, from_ord, only_pat, play_loop, 600, RATE, &smp, &play_rows, &play_nrows);
    if (audio_start(smp, play_n, RATE, msg, sizeof(msg))) { set_status("%s", msg); free(smp); return; }
    free(smp);
    playing = 1; play_only_pat = only_pat;
    play_start = plat_ticks();
    set_status(only_pat >= 0 ? "playing pattern %02d (F8 / Esc stops)" : "playing the song from entry %d (F8 / Esc stops)",
               only_pat >= 0 ? only_pat : from_ord);
}

/* where is the playback now? (sets play_idx; may follow it) */
static void follow_playback(void) {
    size_t s;
    int lo = 0, hi = play_nrows - 1;
    if (!playing) return;
    s = (size_t)((double)(plat_ticks() - play_start) * RATE / 1000.0);
    if (s >= play_n) { playing = 0; play_idx = -1; return; }
    while (lo < hi) {
        int mid = (lo + hi + 1) / 2;
        if (play_rows[mid].sample <= s) lo = mid; else hi = mid - 1;
    }
    play_idx = lo;
    if (follow && play_nrows) {
        /* show the pattern being played, its row in the middle */
        const RowPos *r = &play_rows[lo];
        cur_pat = r->pat;
        if (r->ord >= 0) cur_ord = r->ord;
        clamp_cursor();
        scroll = r->row - VIS_ROWS / 2;
        clamp_cursor();
    }
}


/* hear a note just entered */
static void preview(int code, int ch) {
    static Song t;
    int16_t *smp;
    RowPos *rp;
    int nr;
    size_t n;
    char msg[200];
    if (playing || code < N_FIRST) return;
    song_init(&t);
    t.speed = 10;
    t.pat[0].rows = 1;
    t.pat[0].n[0][ch] = (uint8_t)code;
    t.pat[0].n[0][!ch] = N_OFF;
    n = song_render(&t, 0, -1, 0, 2, RATE, &smp, &rp, &nr);
    audio_start(smp, n, RATE, msg, sizeof(msg));
    free(smp); free(rp);
}

/* ------------------------------------------------------------------ */
/* files                                                               */

static void new_song(void) {
    stop();
    song_init(&song);
    filename[0] = 0;
    modified = 0;
    cur_pat = cur_row = cur_ch = cur_ord = scroll = 0;
    update_title();
    set_status("new song");
}

static void open_path(const char *path) {
    Song t;
    char msg[256];
    if (song_load(&t, path, msg, sizeof(msg))) { set_status("%s: %s", base_name(path), msg); return; }
    stop();
    song = t;
    if (has_ext(path, "bts")) snprintf(filename, sizeof(filename), "%s", path);
    else filename[0] = 0;   /* imported text: "save" asks for a .bts name */
    modified = 0;
    cur_pat = song.ord[0]; cur_row = cur_ch = cur_ord = scroll = 0;
    update_title();
    set_status("loaded %s: %s", base_name(path), msg);
}

static int save_path(const char *path) {
    if (song_save(&song, path)) { set_status("can't write %s", path); return 1; }
    snprintf(filename, sizeof(filename), "%s", path);
    modified = 0;
    update_title();
    set_status("saved %s (%u bytes)", base_name(path), (unsigned)song_bytes());
    return 0;
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
        snprintf(t, sizeof(t), "song_%s", out);
        snprintf(out, n, "%s", t);
    }
}

static void file_chosen(int purpose, const char *path) {
    char label[64];
    switch (purpose) {
    case F_OPEN: open_path(path); break;
    case F_SAVE: save_path(path); break;
    case F_ASM: case F_C:
        label_from(path, label, sizeof(label));
        if ((purpose == F_ASM ? song_export_asm : song_export_c)(&song, path, label)) set_status("can't write %s", path);
        else set_status("exported %s (label %s, %u bytes)", base_name(path), label, (unsigned)song_bytes());
        break;
    case F_TXT:
        set_status(song_export_txt(&song, path) ? "can't write %s" : "exported %s as text", base_name(path));
        break;
    case F_WAV: {
        int16_t *smp;
        RowPos *rp;
        int nr;
        size_t n = song_render(&song, 0, -1, 0, 600, RATE, &smp, &rp, &nr);
        set_status(wav_write(path, smp, n, RATE) ? "can't write %s" : "wrote %s", base_name(path));
        free(smp); free(rp);
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
        char pt[1500];
        WIN32_FIND_DATAA fd;
        HANDLE h;
        snprintf(pt, sizeof(pt), "%s\\*", fb_dir);
        h = FindFirstFileA(pt, &fd);
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
    if (fb_name[0] == PATHSEP || (fb_name[0] && fb_name[1] == ':')) snprintf(path, sizeof(path), "%s", fb_name);
    else snprintf(path, sizeof(path), "%s%c%s", fb_dir, PATHSEP, fb_name);
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

static void run_action(int a) {
    switch (a) {
    case A_NEW: new_song(); break;
    case A_OPEN: choose_file(F_OPEN, 0, "Open song", "Beeptune songs (.bts, or text .txt)", "bts;txt", filename); break;
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
    else choose_file(F_SAVE, 1, "Save song as", "Beeptune songs", "bts", filename[0] ? filename : "song.bts");
}

/* ------------------------------------------------------------------ */
/* editing                                                             */

static void set_note(int code) {
    undo_push();
    pat()->n[cur_row][cur_ch] = (uint8_t)code;
    changed();
    preview(code, cur_ch);
    cur_row += edit_step;
    if (cur_row >= pat()->rows) cur_row = pat()->rows - 1;
    show_cursor();
}

static void transpose(int d) {
    int c = pat()->n[cur_row][cur_ch];
    if (c < N_FIRST) return;
    c += d;
    if (c < N_FIRST || c >= N_FIRST + NOTES) return;
    undo_push();
    pat()->n[cur_row][cur_ch] = (uint8_t)c;
    changed();
    preview(c, cur_ch);
}

static void set_rows(int rows) {
    int r;
    if (rows < 1 || rows > MAX_ROWS) return;
    undo_push();
    for (r = pat()->rows; r < rows; r++) pat()->n[r][0] = pat()->n[r][1] = N_HOLD;
    pat()->rows = rows;
    new_rows = rows;
    changed(); clamp_cursor();
}

static void new_pattern(int clone) {
    if (song.npat >= MAX_PAT) { set_status("%d patterns at most", MAX_PAT); return; }
    undo_push();
    if (clone) song.pat[song.npat] = *pat();
    else pattern_clear(&song.pat[song.npat], new_rows);
    cur_pat = song.npat++;
    cur_row = scroll = 0;
    changed();
    set_status(clone ? "pattern %02d is a copy" : "new pattern %02d (Ins adds it to the order list)", cur_pat);
}

static void delete_pattern(void) {
    int i;
    if (song.npat == 1) { set_status("a song needs a pattern"); return; }
    if (pat_used(cur_pat)) { set_status("pattern %02d is in the order list: take it out first", cur_pat); return; }
    undo_push();
    for (i = cur_pat; i < song.npat - 1; i++) song.pat[i] = song.pat[i + 1];
    song.npat--;
    for (i = 0; i < song.nord; i++) if (song.ord[i] > cur_pat) song.ord[i]--;
    changed(); clamp_cursor();
}

static void order_insert(void) {
    int i;
    if (song.nord >= MAX_ORD) return;
    undo_push();
    for (i = song.nord; i > cur_ord + 1; i--) song.ord[i] = song.ord[i - 1];
    song.ord[cur_ord + 1] = (uint8_t)cur_pat;
    song.nord++;
    if (song.loopto > cur_ord) song.loopto++;
    cur_ord++;
    changed();
}

static void order_delete(void) {
    int i;
    if (song.nord == 1) { set_status("the order list needs an entry"); return; }
    undo_push();
    for (i = cur_ord; i < song.nord - 1; i++) song.ord[i] = song.ord[i + 1];
    song.nord--;
    if (song.loopto > cur_ord || song.loopto >= song.nord) song.loopto = song.loopto ? song.loopto - 1 : 0;
    changed(); clamp_cursor();
    cur_pat = song.ord[cur_ord];
}

static void order_move(int d) {
    int j = cur_ord + d, t;
    if (j < 0 || j >= song.nord) return;
    undo_push();
    t = song.ord[cur_ord]; song.ord[cur_ord] = song.ord[j]; song.ord[j] = (uint8_t)t;
    if (song.loopto == cur_ord) song.loopto = j; else if (song.loopto == j) song.loopto = cur_ord;
    cur_ord = j;
    changed();
}

static void order_pattern(int d) {
    int p = song.ord[cur_ord] + d;
    if (p < 0 || p >= song.npat) return;
    undo_push();
    song.ord[cur_ord] = (uint8_t)p;
    cur_pat = p;
    changed(); clamp_cursor();
}

/* ------------------------------------------------------------------ */
/* drawing                                                             */

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
    if (TB("Play song", 80)) play(-1, 0);
    if (TB("Play pattern", 104)) play(cur_pat, 0);
    if (TB("Stop", 48)) stop();
    x += 8;
    if (TB("Undo", 48)) do_undo();
    if (TB("Redo", 48)) do_redo();
    x += 8;
    if (TB("Export .asm", 96)) choose_file(F_ASM, 1, "Export as z80asm source", "Assembler", "asm", "song.asm");
    if (TB("Export .h", 80)) choose_file(F_C, 1, "Export as C array", "C header", "h", "song.h");
    if (TB(".txt", 40)) choose_file(F_TXT, 1, "Export as text", "Text", "txt", "song.txt");
    if (TB(".wav", 40)) choose_file(F_WAV, 1, "Export as WAV", "Sound", "wav", "song.wav");
    if (ui_button(WIN_W - 56, 5, 48, 22, "Help", modal == M_HELP)) modal = M_HELP;
#undef TB
}

/* a little bar showing how high a note is (the shape of the tune) */
static void note_bar(int x, int y, int code, uint32_t c) {
    if (code < N_FIRST) return;
    ui_rect(x, y + 3, 1 + (code - N_FIRST) * 76 / NOTES, 5, c);
}

static void draw_editor(void) {
    int r, ch;
    Pattern *p = pat();
    int prow = -1;
    char nb[4];
    if (playing && play_idx >= 0 && play_rows[play_idx].pat == cur_pat) prow = play_rows[play_idx].row;
    ui_rect(EX, EY, EW, EH, C_PANEL);
    ui_frame(EX, EY, EW, EH, C_BORDER);
    ui_textf(EX + 8, EY + 7, C_ACCENT, "Pattern %02d", cur_pat);
    ui_textf(EX + 104, EY + 7, C_DIM, "%d rows%s", p->rows, pat_used(cur_pat) ? "" : "   (not in the order list)");
    ui_text(COL_A, EY + 24, "Channel A", cur_ch == 0 ? C_TEXT : C_DIM);
    ui_text(COL_B, EY + 24, "Channel B", cur_ch == 1 ? C_TEXT : C_DIM);
    ui_hline(EX + 4, ROWS_Y - 3, EW - 8, C_BORDER);
    for (r = scroll; r < p->rows && r < scroll + VIS_ROWS; r++) {
        int y = ROWS_Y + (r - scroll) * ROW_H;
        uint32_t rowbg = (r % 16 == 0) ? 0x3A4150 : (r % 4 == 0) ? C_PANEL2 : C_PANEL;
        ui_rect(EX + 2, y - 1, EW - 4, ROW_H, rowbg);
        if (r == prow) ui_rect(EX + 2, y - 1, EW - 4, ROW_H, 0x24603A);
        ui_textf(EX + 12, y + 2, r % 4 == 0 ? C_TEXT : C_DIM, "%02d", r);
        for (ch = 0; ch < 2; ch++) {
            int x = ch ? COL_B : COL_A, code = p->n[r][ch];
            int hot = ui_inside(x - 4, y - 1, COL_W, ROW_H);
            if (r == cur_row && ch == cur_ch) {
                ui_rect(x - 4, y - 1, COL_W, ROW_H, 0x5A4A20);
                ui_frame(x - 4, y - 1, COL_W, ROW_H, C_ACCENT);
            } else if (hot) ui_frame(x - 4, y - 1, COL_W, ROW_H, C_BORDER);
            ui_text(x, y + 2, note_name(code, nb), code == N_HOLD ? 0x5C6578 : code == N_OFF ? C_RED : C_TEXT);
            note_bar(x + 36, y, code, ch ? C_ACCENT2 : C_GREEN);
            if (hot && ui.pressed[BTN_LEFT]) { cur_row = r; cur_ch = ch; }
        }
    }
    if (ui_inside(EX, EY, EW, EH) && ui.wheel) {
        scroll -= ui.wheel * 4;
        if (scroll > p->rows - VIS_ROWS) scroll = p->rows - VIS_ROWS;
        if (scroll < 0) scroll = 0;
    }
    if (p->rows > VIS_ROWS) {   /* scroll bar */
        int h = EH - 42, by = ROWS_Y - 2 + h * scroll / p->rows, bh = h * VIS_ROWS / p->rows;
        ui_rect(EX + EW - 6, by, 3, bh, C_BORDER);
    }
}

/* "label  [-] value [+]" */
static int stepper(int x, int y, const char *label, int value, int *delta) {
    ui_text(x, y + 4, label, C_TEXT);
    *delta = 0;
    if (ui_button(x + 168, y, 20, 16, "-", 0)) *delta = -1;
    ui_textf(x + 194, y + 4, C_ACCENT, "%3d", value);
    if (ui_button(x + 224, y, 20, 16, "+", 0)) *delta = 1;
    return *delta;
}

static void draw_song_panel(void) {
    int x = SX, y = EY, d, i;
    ui_rect(x, y, SW, 194, C_PANEL);
    ui_frame(x, y, SW, 194, C_BORDER);
    ui_text(x + 8, y + 7, "Song", C_ACCENT);
    y += 24;
    if (stepper(x + 8, y, "speed (frames/row)", song.speed, &d)) {
        if (song.speed + d >= 1 && song.speed + d <= 255) { undo_push(); song.speed += d; changed(); }
    }
    y += 20;
    if (stepper(x + 8, y, "rows (Shift: 8)", pat()->rows, &d)) set_rows(pat()->rows + d * ((plat_mods() & KMOD_SHIFT) ? 8 : 1));
    y += 20;
    if (stepper(x + 8, y, "octave (+ -)", octave, &d)) { octave += d; if (octave < 1) octave = 1; if (octave > 6) octave = 6; }
    y += 20;
    if (stepper(x + 8, y, "edit step", edit_step, &d)) { edit_step += d; if (edit_step < 0) edit_step = 0; if (edit_step > 16) edit_step = 16; }
    y += 24;
    ui_check(x + 8, y, "loop when playing", &play_loop);
    ui_check(x + 184, y, "follow", &follow);
    y += 18;
    ui_textf(x + 8, y, C_DIM, "%u bytes   %d:%02d   %.2f s/row", (unsigned)song_bytes(),
             song_seconds() / 60, song_seconds() % 60, song.speed / 50.0);
    y += 14;
    ui_textf(x + 8, y, C_DIM, "sound: %s", audio_backend());
    (void)i;
}

static void draw_patterns_panel(void) {
    int x = SX, y = EY + 202, h = SB_Y - 8 - y, i;
    ui_rect(x, y, SW, h, C_PANEL);
    ui_frame(x, y, SW, h, C_BORDER);
    ui_textf(x + 8, y + 7, C_ACCENT, "Patterns (%d)", song.npat);
    ui_text(x + 136, y + 7, "dim: not used", C_DIM);
    for (i = 0; i < song.npat; i++) {
        int bx = x + 8 + (i % 8) * 34, by = y + 24 + (i / 8) * 22;
        char lab[12];
        snprintf(lab, sizeof(lab), "%02d", i & 63);
        if (i == cur_pat) {
            if (ui_button(bx, by, 30, 18, lab, 1)) {}
        } else {
            int hot = ui_inside(bx, by, 30, 18);
            ui_rect(bx, by, 30, 18, hot ? C_BUTTONH : C_BUTTON);
            ui_frame(bx, by, 30, 18, C_BORDER);
            ui_text(bx + 7, by + 5, lab, pat_used(i) ? C_TEXT : C_DIM);
            if (hot && ui.pressed[BTN_LEFT]) { cur_pat = i; clamp_cursor(); }
        }
    }
    y += 24 + 8 * 22 + 4;
    if (ui_button(x + 8, y, 64, 20, "New", 0)) new_pattern(0);
    if (ui_button(x + 76, y, 64, 20, "Clone", 0)) new_pattern(1);
    if (ui_button(x + 144, y, 64, 20, "Clear", 0)) { undo_push(); pattern_clear(pat(), pat()->rows); changed(); }
    if (ui_button(x + 212, y, 70, 20, "Delete", 0)) delete_pattern();
    y += 28;
    ui_text(x + 8, y, "Ctrl+C / Ctrl+V copy and paste", C_DIM);
    ui_text(x + 8, y + 12, "a pattern. Ins: put it in the", C_DIM);
    ui_text(x + 8, y + 24, "order list after the selected", C_DIM);
    ui_text(x + 8, y + 36, "entry.", C_DIM);
}

static void draw_order_panel(void) {
    int x = OX, y = EY, h = EH, i, vis, top, prow = -1;
    static int oscroll;
    ui_rect(x, y, OW, h, C_PANEL);
    ui_frame(x, y, OW, h, C_BORDER);
    ui_textf(x + 8, y + 7, C_ACCENT, "Order list (%d)", song.nord);
    ui_text(x + 150, y + 7, "L = loop point", C_DIM);
    if (playing && play_idx >= 0) prow = play_rows[play_idx].ord;
    vis = (h - 120) / 14;
    if (cur_ord < oscroll) oscroll = cur_ord;
    if (cur_ord >= oscroll + vis) oscroll = cur_ord - vis + 1;
    if (ui_inside(x, y, OW, h - 100) && ui.wheel) oscroll -= ui.wheel * 3;
    if (oscroll > song.nord - vis) oscroll = song.nord - vis;
    if (oscroll < 0) oscroll = 0;
    top = y + 24;
    for (i = oscroll; i < song.nord && i < oscroll + vis; i++) {
        int ry = top + (i - oscroll) * 14;
        int hot = ui_inside(x + 4, ry, OW - 8, 14);
        if (i == cur_ord) ui_rect(x + 4, ry, OW - 8, 14, 0x5A4A20);
        else if (hot) ui_rect(x + 4, ry, OW - 8, 14, C_PANEL2);
        if (i == prow) ui_rect(x + 4, ry, 4, 14, C_GREEN);
        ui_textf(x + 14, ry + 3, C_DIM, "%03d", i);
        ui_textf(x + 54, ry + 3, i == cur_ord ? C_ACCENT : C_TEXT, "pattern %02d", song.ord[i]);
        ui_textf(x + 150, ry + 3, C_DIM, "%3d rows", song.pat[song.ord[i]].rows);
        if (i == song.loopto) ui_text(x + OW - 20, ry + 3, "L", C_ACCENT2);
        if (hot && ui.pressed[BTN_LEFT]) { cur_ord = i; cur_pat = song.ord[i]; clamp_cursor(); if (ui.dbl) play(-1, i); }
    }
    y = y + h - 92;
    ui_hline(x + 4, y - 6, OW - 8, C_BORDER);
    if (ui_button(x + 8, y, 88, 20, "Insert", 0)) order_insert();
    if (ui_button(x + 100, y, 88, 20, "Delete", 0)) order_delete();
    if (ui_button(x + 192, y, 94, 20, "Loop here", 0)) { undo_push(); song.loopto = cur_ord; changed(); }
    y += 24;
    if (ui_button(x + 8, y, 88, 20, "Up", 0)) order_move(-1);
    if (ui_button(x + 100, y, 88, 20, "Down", 0)) order_move(1);
    if (ui_button(x + 192, y, 45, 20, "P-", 0)) order_pattern(-1);
    if (ui_button(x + 241, y, 45, 20, "P+", 0)) order_pattern(1);
    y += 26;
    ui_text(x + 8, y, "double click: play from there", C_DIM);
    ui_text(x + 8, y + 12, "F7: play from the selected entry", C_DIM);
}

static void draw_statusbar(void) {
    char right[200];
    ui_rect(0, SB_Y, WIN_W, 22, C_PANEL);
    ui_hline(0, SB_Y, WIN_W, C_BORDER);
    if (playing && play_idx >= 0) {
        const RowPos *r = &play_rows[play_idx];
        uint32_t ms = plat_ticks() - play_start;
        if (r->ord >= 0) snprintf(right, sizeof(right), "> entry %03d  pattern %02d  row %02d  |  %u:%02u", r->ord, r->pat, r->row, ms / 60000, ms / 1000 % 60);
        else snprintf(right, sizeof(right), "> pattern %02d  row %02d  |  %u:%02u", r->pat, r->row, ms / 60000, ms / 1000 % 60);
    } else {
        char nb[4];
        snprintf(right, sizeof(right), "pattern %02d  row %02d  ch %c  %s  |  %u bytes", cur_pat, cur_row, 'A' + cur_ch,
                 note_name(pat()->n[cur_row][cur_ch], nb), (unsigned)song_bytes());
    }
    ui_text(WIN_W - 8 - ui_text_width(right), SB_Y + 7, right, C_TEXT);
    if (status[0] && plat_ticks() - status_time < 8000) {
        char s[120];
        snprintf(s, sizeof(s), "%.*s", (int)((WIN_W - 24 - ui_text_width(right)) / 8), status);
        ui_text(8, SB_Y + 7, s, C_ACCENT);
    }
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
        "NOTES       Z S X D C V G B H N J M   the octave shown in the panel",
        "            Q 2 W 3 E R 5 T 6 Y 7 U   one octave up (I 9 O 0 P: two)",
        "            1 silence (===)   Del / Backspace: keep playing (---)",
        "            Ctrl+Up/Down a semitone, Ctrl+Shift+Up/Down an octave",
        "            + - octave        edit step: rows the cursor moves after a note",
        "MOVE        arrows, Tab (channel), PgUp/PgDn, Home/End, mouse, wheel",
        "PLAY        F5 song   F6 pattern   F7 song from the selected entry",
        "            Space: play / stop the pattern     F8 or Esc: stop",
        "PATTERNS    Ctrl+Left/Right previous / next   Ctrl+C / Ctrl+V copy, paste",
        "ORDER LIST  Ins: add the current pattern after the selected entry",
        "            Ctrl+Del remove the entry   Ctrl+PgUp/PgDn select entry",
        "FILES       Ctrl+N new  Ctrl+O open  Ctrl+S save  Ctrl+Shift+S save as",
        "            Ctrl+Z undo   Ctrl+Y redo",
        "",
        "What you hear is the Z80 player, turn by turn (130 T-states each): two",
        "square waves sharing the speaker. A row lasts `speed` frames of 1/50 s.",
        "Songs are saved in the player's own format (.bts): incbin it, or export",
        "z80asm / C source. Play them with beeptune.h (bt_play, bt_loop, bt_step).",
    };
    begin_modal(660, 316, "Beeptune - help", &x, &y);
    for (i = 0; i < (int)(sizeof(lines) / sizeof(lines[0])); i++) ui_text(x + 14, y + 32 + i * 13, lines[i], i < 13 ? C_TEXT : C_DIM);
    if (ui_button(x + 280, y + 286, 100, 22, "Close", 0)) modal = M_NONE;
}

/* ------------------------------------------------------------------ */
/* keyboard                                                            */

/* piano keys -> semitones from C of the current octave */
static int piano(int k) {
    static const char *low = "zsxdcvgbhnjm", *high = "q2w3er5t6y7ui9o0p";
    const char *p;
    if (k < 32 || k > 126) return -1;
    if ((p = strchr(low, k))) return (int)(p - low);
    if ((p = strchr(high, k))) return 12 + (int)(p - high);
    return -1;
}

static void key_event(const PEvent *e) {
    int k = e->key, ctrl = e->mod & KMOD_CTRL, shift = e->mod & KMOD_SHIFT, n;
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
        case 'c': clip = *pat(); has_clip = 1; set_status("pattern %02d copied", cur_pat); return;
        case 'v':
            if (!has_clip) return;
            undo_push(); *pat() = clip; changed(); clamp_cursor();
            set_status("pasted into pattern %02d", cur_pat);
            return;
        case K_LEFT: if (cur_pat > 0) cur_pat--; show_cursor(); return;
        case K_RIGHT: if (cur_pat + 1 < song.npat) cur_pat++; show_cursor(); return;
        case K_UP: transpose(shift ? 12 : 1); return;
        case K_DOWN: transpose(shift ? -12 : -1); return;
        case K_DELETE: order_delete(); return;
        case K_PGUP: if (cur_ord > 0) { cur_ord--; cur_pat = song.ord[cur_ord]; clamp_cursor(); } return;
        case K_PGDN: if (cur_ord + 1 < song.nord) { cur_ord++; cur_pat = song.ord[cur_ord]; clamp_cursor(); } return;
        }
        return;
    }
    switch (k) {
    case K_F1: modal = M_HELP; return;
    case K_F5: play(-1, 0); return;
    case K_F6: play(cur_pat, 0); return;
    case K_F7: play(-1, cur_ord); return;
    case K_F8: case K_ESC: stop(); return;
    case ' ': if (playing) stop(); else play(cur_pat, 0); return;
    case K_UP: cur_row--; if (cur_row < 0) cur_row = pat()->rows - 1; break;
    case K_DOWN: cur_row++; if (cur_row >= pat()->rows) cur_row = 0; break;
    case K_LEFT: case K_RIGHT: case K_TAB: cur_ch = !cur_ch; break;
    case K_PGUP: cur_row -= 16; if (cur_row < 0) cur_row = 0; break;
    case K_PGDN: cur_row += 16; if (cur_row >= pat()->rows) cur_row = pat()->rows - 1; break;
    case K_HOME: cur_row = 0; break;
    case K_END: cur_row = pat()->rows - 1; break;
    case K_INSERT: order_insert(); return;
    case K_DELETE: case K_BACKSPACE: set_note(N_HOLD); return;
    case '1': set_note(N_OFF); return;
    case '+': case '=': if (octave < 6) octave++; return;
    case '-': if (octave > 1) octave--; return;
    default: goto other;
    }
    show_cursor();
    return;
other:
    n = piano(k);
    if (n >= 0) {
        int code = N_FIRST + (octave - 1) * 12 + n;
        if (code < N_FIRST + NOTES) set_note(code);
        return;
    }
}

/* ------------------------------------------------------------------ */

static void frame(void) {
    ui_begin(fb, WIN_W, WIN_H);
    ui.blocked = modal != M_NONE;
    follow_playback();
    clamp_cursor();
    ui_rect(0, 0, WIN_W, WIN_H, C_BG);
    draw_editor();
    draw_song_panel();
    draw_patterns_panel();
    draw_order_panel();
    draw_toolbar();
    draw_statusbar();
    if (modal) {
        ui.blocked = 0;
        if (modal == M_CONFIRM) draw_confirm();
        else if (modal == M_FILE) draw_filebrowser();
        else if (modal == M_HELP) draw_help();
    }
}

static void handle_drop(const char *path) {
    if (!path || !path[0]) return;
    snprintf(pending_path, sizeof(pending_path), "%s", path);
    guarded(A_OPEN_PATH);
}

int app_run(const char *file) {
    PEvent e;
    int redraw = 1, quit_requests = 0;
    song_init(&song);
    if (plat_init(WIN_W, WIN_H, "Beeptune")) {
        fprintf(stderr, "can't open a window\n");
        return 1;
    }
    if (file) open_path(file); else update_title();
    if (!file) set_status("Welcome to Beeptune. F1 for help. Type notes with Z S X D C V... and Q 2 W 3 E...");

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
            if (e.type == EV_DROP) { handle_drop(plat_drop_path()); break; }
        }
        if (modal != M_CONFIRM) quit_requests = 0;
        if (got || redraw || playing) {
            frame();
            plat_present(fb);
            redraw = got;
            if (playing && !got) plat_wait(20);
        } else {
            plat_wait(50);
            if (status[0] && plat_ticks() - status_time >= 8000 && plat_ticks() - status_time < 8100) redraw = 1;
        }
    }
    stop();
    plat_shutdown();
    return 0;
}
