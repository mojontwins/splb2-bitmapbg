/* Beeptune - X11 platform layer (Linux, BSD, macOS + XQuartz). Link: -lX11 */
#include "plat.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>

static Display *dpy;
static Window win;
static GC gc;
static XImage *img;
static uint32_t *xbuf;
static int W, H, mods, mx, my;
static Atom wm_delete;

int plat_init(int w, int h, const char *title) {
    XSizeHints *sh;
    int scr;
    W = w; H = h;
    dpy = XOpenDisplay(NULL);
    if (!dpy) return -1;
    scr = DefaultScreen(dpy);
    win = XCreateSimpleWindow(dpy, RootWindow(dpy, scr), 0, 0, w, h, 0,
                              BlackPixel(dpy, scr), BlackPixel(dpy, scr));
    XSelectInput(dpy, win, ExposureMask | KeyPressMask | KeyReleaseMask | ButtonPressMask |
                 ButtonReleaseMask | PointerMotionMask | StructureNotifyMask);
    sh = XAllocSizeHints();
    sh->flags = PMinSize | PMaxSize;
    sh->min_width = sh->max_width = w;
    sh->min_height = sh->max_height = h;
    XSetWMNormalHints(dpy, win, sh);
    XFree(sh);
    wm_delete = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(dpy, win, &wm_delete, 1);
    XStoreName(dpy, win, title);
    XMapWindow(dpy, win);
    gc = XCreateGC(dpy, win, 0, 0);
    xbuf = (uint32_t *)malloc((size_t)w * h * 4);
    img = XCreateImage(dpy, DefaultVisual(dpy, scr), DefaultDepth(dpy, scr), ZPixmap, 0,
                       (char *)xbuf, w, h, 32, 0);
    XFlush(dpy);
    return 0;
}

void plat_shutdown(void) {
    if (!dpy) return;
    img->data = NULL;
    XDestroyImage(img);
    free(xbuf);
    XCloseDisplay(dpy);
    dpy = NULL;
}

static int conv_mods(unsigned state) {
    return ((state & ControlMask) ? KMOD_CTRL : 0) | ((state & ShiftMask) ? KMOD_SHIFT : 0) |
           ((state & Mod1Mask) ? KMOD_ALT : 0);
}

static int map_key(KeySym k) {
    switch (k) {
    case XK_Escape: return K_ESC;
    case XK_Return: case XK_KP_Enter: return K_ENTER;
    case XK_BackSpace: return K_BACKSPACE;
    case XK_Delete: case XK_KP_Delete: return K_DELETE;
    case XK_Tab: return K_TAB;
    case XK_Left: case XK_KP_Left: return K_LEFT;
    case XK_Right: case XK_KP_Right: return K_RIGHT;
    case XK_Up: case XK_KP_Up: return K_UP;
    case XK_Down: case XK_KP_Down: return K_DOWN;
    case XK_Home: return K_HOME;
    case XK_End: return K_END;
    case XK_Prior: return K_PGUP;
    case XK_Next: return K_PGDN;
    case XK_Insert: return K_INSERT;
    case XK_KP_Add: case XK_plus: return '+';
    case XK_KP_Subtract: case XK_minus: return '-';
    case XK_equal: return '=';
    case XK_space: return ' ';
    }
    if (k >= XK_F1 && k <= XK_F12) return K_F1 + (int)(k - XK_F1);
    if (k >= XK_a && k <= XK_z) return 'a' + (int)(k - XK_a);
    if (k >= XK_A && k <= XK_Z) return 'a' + (int)(k - XK_A);
    if (k >= XK_0 && k <= XK_9) return '0' + (int)(k - XK_0);
    if (k >= XK_KP_0 && k <= XK_KP_9) return '0' + (int)(k - XK_KP_0);
    return 0;
}

/* a key press produces up to two events (KEY + CHAR): keep the second one */
static PEvent pending;
static int has_pending;

int plat_next_event(PEvent *e) {
    XEvent ev;
    memset(e, 0, sizeof(*e));
    if (has_pending) { *e = pending; has_pending = 0; return 1; }
    while (XPending(dpy)) {
        XNextEvent(dpy, &ev);
        e->x = mx; e->y = my; e->mod = mods;
        switch (ev.type) {
        case Expose: e->type = EV_EXPOSE; return 1;
        case ClientMessage:
            if ((Atom)ev.xclient.data.l[0] == wm_delete) { e->type = EV_QUIT; return 1; }
            break;
        case MotionNotify:
            mx = e->x = ev.xmotion.x; my = e->y = ev.xmotion.y;
            mods = e->mod = conv_mods(ev.xmotion.state);
            e->type = EV_MMOVE;
            return 1;
        case ButtonPress:
        case ButtonRelease:
            mx = e->x = ev.xbutton.x; my = e->y = ev.xbutton.y;
            mods = e->mod = conv_mods(ev.xbutton.state);
            if (ev.xbutton.button == 4 || ev.xbutton.button == 5) {
                if (ev.type == ButtonRelease) break;
                e->type = EV_WHEEL; e->wheel = ev.xbutton.button == 4 ? 1 : -1;
                return 1;
            }
            e->type = ev.type == ButtonPress ? EV_MDOWN : EV_MUP;
            e->button = ev.xbutton.button == 1 ? BTN_LEFT : ev.xbutton.button == 3 ? BTN_RIGHT : BTN_MIDDLE;
            return 1;
        case KeyPress: {
            char buf[8];
            KeySym ks;
            int n = XLookupString(&ev.xkey, buf, sizeof(buf), &ks, NULL);
            mods = e->mod = conv_mods(ev.xkey.state);
            e->key = map_key(XLookupKeysym(&ev.xkey, 0));
            if (n == 1 && (unsigned char)buf[0] >= 32 && (unsigned char)buf[0] < 127 && !(mods & KMOD_CTRL)) {
                pending = *e; pending.type = EV_CHAR; pending.ch = (unsigned char)buf[0]; pending.key = 0;
                has_pending = 1;
            }
            if (e->key) { e->type = EV_KEY; return 1; }
            if (has_pending) { *e = pending; has_pending = 0; return 1; }
            break;
        }
        case KeyRelease:
            mods = conv_mods(ev.xkey.state);
            break;
        }
    }
    return 0;
}

void plat_wait(int ms) {
    int fd = ConnectionNumber(dpy);
    fd_set fds;
    struct timeval tv;
    if (has_pending || XPending(dpy)) return;
    FD_ZERO(&fds); FD_SET(fd, &fds);
    tv.tv_sec = ms / 1000; tv.tv_usec = (ms % 1000) * 1000;
    select(fd + 1, &fds, NULL, NULL, &tv);
}

void plat_present(const uint32_t *fb) {
    memcpy(xbuf, fb, (size_t)W * H * 4);
    XPutImage(dpy, win, gc, img, 0, 0, 0, 0, W, H);
    XFlush(dpy);
}

void plat_set_title(const char *t) { XStoreName(dpy, win, t); XFlush(dpy); }
int plat_mods(void) { return mods; }
uint32_t plat_ticks(void) { struct timeval tv; gettimeofday(&tv, NULL); return (uint32_t)(tv.tv_sec * 1000 + tv.tv_usec / 1000); }
const char *plat_drop_path(void) { return NULL; }

int plat_file_dialog(int save, const char *title, const char *desc, const char *exts, char *path, int n) {
    (void)save; (void)title; (void)desc; (void)exts; (void)path; (void)n;
    return -1;   /* use the built-in browser */
}
