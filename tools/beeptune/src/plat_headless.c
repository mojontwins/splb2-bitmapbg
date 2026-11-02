/*
 * Beeptune - headless platform layer for automated tests.
 * Reads a script from the file named by BEEPTUNE_SCRIPT, one command per
 * frame:
 *   move X Y | down X Y [l|r|m] | up X Y [l|r|m] | click X Y [l|r|m]
 *   drag X0 Y0 X1 Y1 [l|r|m]      (press, 8 moves, release)
 *   key NAME [ctrl] [shift] [alt] (NAME: a..z, 0..9, esc, enter, del, bksp, ins,
 *                                  tab, left, right, up, down, home, end,
 *                                  pgup, pgdn, plus, minus, space, f1..f12)
 *   type TEXT                     (EV_CHAR for every character)
 *   wheel X Y +1|-1
 *   drop PATH
 *   shot FILE.bmp                 (screenshot after the previous commands)
 *   wait N                        (N idle frames)
 *   quit
 */
#include "plat.h"
#include "bmp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int W, H, mx, my, idle;
static uint32_t ticks;
static FILE *script;
static char shot_path[512], drop[512];
static PEvent q[64];
static int qn, qi, done;

static void add(int type, int key, int ch, int mod, int x, int y, int button, int wheel) {
    PEvent *e = &q[qn++];
    memset(e, 0, sizeof(*e));
    e->type = type; e->key = key; e->ch = ch; e->mod = mod;
    e->x = x; e->y = y; e->button = button; e->wheel = wheel;
}

static int keyname(const char *s) {
    static const struct { const char *n; int k; } t[] = {
        {"esc", K_ESC}, {"enter", K_ENTER}, {"del", K_DELETE}, {"bksp", K_BACKSPACE},
        {"tab", K_TAB}, {"left", K_LEFT}, {"right", K_RIGHT}, {"up", K_UP}, {"down", K_DOWN},
        {"home", K_HOME}, {"end", K_END}, {"pgup", K_PGUP}, {"pgdn", K_PGDN},
        {"plus", '+'}, {"minus", '-'}, {"space", ' '}, {"ins", K_INSERT}, {NULL, 0}};
    int i;
    for (i = 0; t[i].n; i++) if (!strcmp(s, t[i].n)) return t[i].k;
    if (s[0] == 'f' && s[1] >= '1' && s[1] <= '9') return K_F1 + atoi(s + 1) - 1;
    if (strlen(s) == 1) return s[0];
    return 0;
}

static int btn(const char *s) {
    if (!s || !*s) return BTN_LEFT;
    return s[0] == 'r' ? BTN_RIGHT : s[0] == 'm' ? BTN_MIDDLE : BTN_LEFT;
}

static void next_command(void) {
    char line[600], a[512] = "", b[64] = "", c[64] = "", d[64] = "", e[64] = "", f[64] = "";
    int x, y;
    qn = qi = 0;
    if (idle > 0) { idle--; add(EV_EXPOSE, 0, 0, 0, mx, my, 0, 0); return; }
    if (!script || !fgets(line, sizeof(line), script)) { done = 1; return; }
    line[strcspn(line, "\r\n")] = 0;
    if (!strncmp(line, "type ", 5)) {
        const char *s;
        for (s = line + 5; *s && qn < 60; s++) add(EV_CHAR, 0, *s, 0, mx, my, 0, 0);
        return;
    }
    sscanf(line, "%511s %63s %63s %63s %63s %63s", a, b, c, d, e, f);
    if (!strcmp(a, "move") || !strcmp(a, "down") || !strcmp(a, "up") || !strcmp(a, "click")) {
        x = atoi(b); y = atoi(c); mx = x; my = y;
        add(EV_MMOVE, 0, 0, 0, x, y, 0, 0);
        if (!strcmp(a, "down") || !strcmp(a, "click")) add(EV_MDOWN, 0, 0, 0, x, y, btn(d), 0);
        if (!strcmp(a, "up") || !strcmp(a, "click")) add(EV_MUP, 0, 0, 0, x, y, btn(d), 0);
    } else if (!strcmp(a, "drag")) {
        int x0 = atoi(b), y0 = atoi(c), x1 = atoi(d), y1 = atoi(e), i;
        add(EV_MMOVE, 0, 0, 0, x0, y0, 0, 0);
        add(EV_MDOWN, 0, 0, 0, x0, y0, btn(f), 0);
        for (i = 1; i <= 8; i++) add(EV_MMOVE, 0, 0, 0, x0 + (x1 - x0) * i / 8, y0 + (y1 - y0) * i / 8, 0, 0);
        add(EV_MUP, 0, 0, 0, x1, y1, btn(f), 0);
        mx = x1; my = y1;
    } else if (!strcmp(a, "key")) {
        int mod = 0;
        const char *m[3] = {c, d, e};
        int i;
        for (i = 0; i < 3; i++) {
            if (!strcmp(m[i], "ctrl")) mod |= KMOD_CTRL;
            if (!strcmp(m[i], "shift")) mod |= KMOD_SHIFT;
            if (!strcmp(m[i], "alt")) mod |= KMOD_ALT;
        }
        add(EV_KEY, keyname(b), 0, mod, mx, my, 0, 0);
    } else if (!strcmp(a, "wheel")) {
        mx = atoi(b); my = atoi(c);
        add(EV_WHEEL, 0, 0, 0, mx, my, 0, atoi(d));
    } else if (!strcmp(a, "drop")) {
        snprintf(drop, sizeof(drop), "%s", b);
        add(EV_DROP, 0, 0, 0, mx, my, 0, 0);
    } else if (!strcmp(a, "shot")) {
        snprintf(shot_path, sizeof(shot_path), "%s", b);
        add(EV_EXPOSE, 0, 0, 0, mx, my, 0, 0);
    } else if (!strcmp(a, "wait")) {
        idle = atoi(b);
        add(EV_EXPOSE, 0, 0, 0, mx, my, 0, 0);
    } else if (!strcmp(a, "quit")) {
        done = 1;
    }
}

int plat_init(int w, int h, const char *title) {
    const char *s = getenv("BEEPTUNE_SCRIPT");
    (void)title;
    W = w; H = h;
    script = s ? fopen(s, "r") : NULL;
    if (!script) fprintf(stderr, "headless: set BEEPTUNE_SCRIPT to a script file\n");
    next_command();
    return 0;
}

void plat_shutdown(void) { if (script) fclose(script); script = NULL; }

int plat_next_event(PEvent *e) {
    if (done && qi >= qn) {
        memset(e, 0, sizeof(*e));
        e->type = EV_QUIT;
        done = 2;
        return 1;
    }
    if (qi < qn) {
        *e = q[qi++];
        if (e->type == EV_MDOWN || e->type == EV_MUP || e->type == EV_MMOVE) { mx = e->x; my = e->y; }
        return 1;
    }
    return 0;
}

void plat_wait(int ms) { (void)ms; if (qi >= qn && !done) next_command(); }

void plat_present(const uint32_t *fb) {
    ticks += 100;
    if (shot_path[0]) {
        bmp_save(shot_path, fb, W, H);
        printf("headless: wrote %s\n", shot_path);
        shot_path[0] = 0;
    }
    if (qi >= qn && done != 2) next_command();
}

void plat_set_title(const char *t) { printf("headless: title \"%s\"\n", t); }
int plat_mods(void) { return 0; }
uint32_t plat_ticks(void) { return ticks; }
const char *plat_drop_path(void) { return drop; }

int plat_file_dialog(int save, const char *title, const char *desc, const char *exts, char *path, int n) {
    (void)save; (void)title; (void)desc; (void)exts; (void)path; (void)n;
    return -1;
}
