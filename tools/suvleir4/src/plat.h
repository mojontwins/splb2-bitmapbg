/*
 * Suvleir 4 - tiny platform layer: one fixed-size window with a 32-bit
 * framebuffer, mouse/keyboard events, native file dialogs where available.
 * Implementations: plat_win32.c (Windows, GDI), plat_x11.c (Linux/BSD/macOS
 * with XQuartz), plat_headless.c (scripted, for automated tests).
 */
#ifndef PLAT_H
#define PLAT_H
#include <stdint.h>

enum {
    K_ESC = 256, K_ENTER, K_BACKSPACE, K_DELETE, K_TAB, K_LEFT, K_RIGHT, K_UP, K_DOWN,
    K_HOME, K_END, K_PGUP, K_PGDN, K_INSERT, K_F1, K_F2, K_F3, K_F4, K_F5, K_F6, K_F7,
    K_F8, K_F9, K_F10, K_F11, K_F12
};
/* letter keys are reported as 'a'..'z', digits as '0'..'9', plus ' ', '+', '-', '=' */

#define KMOD_CTRL  1
#define KMOD_SHIFT 2
#define KMOD_ALT   4

enum { EV_NONE, EV_KEY, EV_CHAR, EV_MDOWN, EV_MUP, EV_MMOVE, EV_WHEEL, EV_QUIT, EV_DROP, EV_EXPOSE };
enum { BTN_LEFT = 1, BTN_RIGHT = 2, BTN_MIDDLE = 3 };

typedef struct {
    int type;
    int key;      /* EV_KEY  */
    int ch;       /* EV_CHAR: printable character */
    int mod;      /* modifiers at the time of the event */
    int x, y;     /* mouse position (all events) */
    int button;   /* EV_MDOWN / EV_MUP */
    int wheel;    /* EV_WHEEL: +1 up, -1 down */
} PEvent;

int  plat_init(int w, int h, const char *title);
void plat_shutdown(void);
int  plat_next_event(PEvent *e);           /* 1 if an event was returned */
void plat_wait(int ms);                     /* sleep until an event or timeout */
void plat_present(const uint32_t *fb);      /* 0x00RRGGBB, w*h */
void plat_set_title(const char *title);
int  plat_mods(void);
uint32_t plat_ticks(void);                   /* milliseconds */
const char *plat_drop_path(void);           /* valid right after EV_DROP */

/* native file dialog. Returns 1 = chosen, 0 = cancelled, -1 = not available
   (the editor then uses its built-in file browser).
   exts: semicolon separated, e.g. "svl;bin" */
int  plat_file_dialog(int save, const char *title, const char *desc, const char *exts,
                      char *path, int pathlen);

#endif
