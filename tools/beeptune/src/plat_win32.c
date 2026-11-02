/* Beeptune - Win32 platform layer. Link: -lgdi32 -lcomdlg32 -lshell32 (-mwindows) */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>
#include <stdio.h>
#include <string.h>
#include "plat.h"

static HWND hwnd;
static int W, H, mx, my, quit_req;
static const uint32_t *lastfb;
static char drop_path[MAX_PATH];

#define QN 256
static PEvent q[QN];
static int qh, qt;

static void push(PEvent e) {
    int n = (qt + 1) % QN;
    if (n == qh) return;
    q[qt] = e; qt = n;
}

static int cur_mods(void) {
    return ((GetKeyState(VK_CONTROL) & 0x8000) ? KMOD_CTRL : 0) |
           ((GetKeyState(VK_SHIFT) & 0x8000) ? KMOD_SHIFT : 0) |
           ((GetKeyState(VK_MENU) & 0x8000) ? KMOD_ALT : 0);
}

static int map_vk(WPARAM vk) {
    switch (vk) {
    case VK_ESCAPE: return K_ESC;
    case VK_RETURN: return K_ENTER;
    case VK_BACK: return K_BACKSPACE;
    case VK_DELETE: return K_DELETE;
    case VK_TAB: return K_TAB;
    case VK_LEFT: return K_LEFT;
    case VK_RIGHT: return K_RIGHT;
    case VK_UP: return K_UP;
    case VK_DOWN: return K_DOWN;
    case VK_HOME: return K_HOME;
    case VK_END: return K_END;
    case VK_PRIOR: return K_PGUP;
    case VK_NEXT: return K_PGDN;
    case VK_INSERT: return K_INSERT;
    case VK_SPACE: return ' ';
    case VK_ADD: case VK_OEM_PLUS: return '+';
    case VK_SUBTRACT: case VK_OEM_MINUS: return '-';
    }
    if (vk >= VK_F1 && vk <= VK_F12) return K_F1 + (int)(vk - VK_F1);
    if (vk >= 'A' && vk <= 'Z') return (int)(vk - 'A') + 'a';
    if (vk >= '0' && vk <= '9') return (int)vk;
    if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9) return '0' + (int)(vk - VK_NUMPAD0);
    return 0;
}

static void blit(HDC dc) {
    BITMAPINFO bi;
    if (!lastfb) return;
    memset(&bi, 0, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biWidth = W;
    bi.bmiHeader.biHeight = -H;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    SetDIBitsToDevice(dc, 0, 0, W, H, 0, 0, 0, H, lastfb, &bi, DIB_RGB_COLORS);
}

static LRESULT CALLBACK wndproc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    PEvent e;
    memset(&e, 0, sizeof(e));
    e.mod = cur_mods();
    e.x = mx; e.y = my;
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        blit(dc);
        EndPaint(h, &ps);
        return 0;
    }
    case WM_ERASEBKGND: return 1;
    case WM_CLOSE: e.type = EV_QUIT; push(e); return 0;
    case WM_DESTROY: quit_req = 1; PostQuitMessage(0); return 0;
    case WM_MOUSEMOVE:
        mx = e.x = (short)LOWORD(lp); my = e.y = (short)HIWORD(lp);
        e.type = EV_MMOVE; push(e); return 0;
    case WM_LBUTTONDOWN: case WM_RBUTTONDOWN: case WM_MBUTTONDOWN:
    case WM_LBUTTONUP: case WM_RBUTTONUP: case WM_MBUTTONUP:
        mx = e.x = (short)LOWORD(lp); my = e.y = (short)HIWORD(lp);
        e.type = (msg == WM_LBUTTONDOWN || msg == WM_RBUTTONDOWN || msg == WM_MBUTTONDOWN) ? EV_MDOWN : EV_MUP;
        e.button = (msg == WM_LBUTTONDOWN || msg == WM_LBUTTONUP) ? BTN_LEFT :
                   (msg == WM_RBUTTONDOWN || msg == WM_RBUTTONUP) ? BTN_RIGHT : BTN_MIDDLE;
        if (e.type == EV_MDOWN) SetCapture(h); else ReleaseCapture();
        push(e);
        return 0;
    case WM_MOUSEWHEEL:
        e.type = EV_WHEEL; e.wheel = GET_WHEEL_DELTA_WPARAM(wp) > 0 ? 1 : -1; push(e); return 0;
    case WM_KEYDOWN: case WM_SYSKEYDOWN:
        e.key = map_vk(wp);
        if (e.key) { e.type = EV_KEY; push(e); }
        if (msg == WM_SYSKEYDOWN && wp != VK_F4) return 0;
        break;
    case WM_CHAR:
        if (wp >= 32 && wp < 127 && !(e.mod & KMOD_CTRL)) { e.type = EV_CHAR; e.ch = (int)wp; push(e); }
        return 0;
    case WM_DROPFILES: {
        HDROP d = (HDROP)wp;
        if (DragQueryFileA(d, 0, drop_path, sizeof(drop_path))) { e.type = EV_DROP; push(e); }
        DragFinish(d);
        return 0;
    }
    }
    return DefWindowProcA(h, msg, wp, lp);
}

int plat_init(int w, int h, const char *title) {
    WNDCLASSA wc;
    RECT r = {0, 0, w, h};
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    HINSTANCE inst = GetModuleHandleA(NULL);
    W = w; H = h;
    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = wndproc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = LoadIcon(inst, MAKEINTRESOURCE(1));
    wc.lpszClassName = "Suvleir4";
    RegisterClassA(&wc);
    AdjustWindowRect(&r, style, FALSE);
    hwnd = CreateWindowExA(WS_EX_ACCEPTFILES, "Suvleir4", title, style, CW_USEDEFAULT, CW_USEDEFAULT,
                           r.right - r.left, r.bottom - r.top, NULL, NULL, inst, NULL);
    if (!hwnd) return -1;
    ShowWindow(hwnd, SW_SHOWNORMAL);
    UpdateWindow(hwnd);
    return 0;
}

void plat_shutdown(void) { if (hwnd) DestroyWindow(hwnd); hwnd = NULL; }

int plat_next_event(PEvent *e) {
    MSG m;
    while (qh == qt && PeekMessageA(&m, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&m);
        DispatchMessageA(&m);
    }
    if (qh == qt) {
        if (quit_req) { memset(e, 0, sizeof(*e)); e->type = EV_QUIT; quit_req = 0; return 1; }
        return 0;
    }
    *e = q[qh]; qh = (qh + 1) % QN;
    return 1;
}

void plat_wait(int ms) {
    if (qh != qt) return;
    MsgWaitForMultipleObjects(0, NULL, FALSE, (DWORD)ms, QS_ALLINPUT);
}

void plat_present(const uint32_t *fb) {
    HDC dc = GetDC(hwnd);
    lastfb = fb;
    blit(dc);
    ReleaseDC(hwnd, dc);
}

void plat_set_title(const char *t) { SetWindowTextA(hwnd, t); }
int plat_mods(void) { return cur_mods(); }
uint32_t plat_ticks(void) { return (uint32_t)GetTickCount(); }
const char *plat_drop_path(void) { return drop_path; }

int plat_file_dialog(int save, const char *title, const char *desc, const char *exts, char *path, int n) {
    OPENFILENAMEA ofn;
    char filter[512], pat[256], defext[16], *p;
    const char *s;
    int k = 0, ok;
    /* "desc\0*.a;*.b\0All files\0*.*\0\0" */
    k += snprintf(filter + k, sizeof(filter) - k, "%s", desc) + 1;
    p = pat; *p = 0;
    for (s = exts; *s;) {
        const char *e = strchr(s, ';');
        size_t l = e ? (size_t)(e - s) : strlen(s);
        p += snprintf(p, sizeof(pat) - (p - pat), "%s*.%.*s", p == pat ? "" : ";", (int)l, s);
        s += l; if (*s == ';') s++;
    }
    k += snprintf(filter + k, sizeof(filter) - k, "%s", pat) + 1;
    k += snprintf(filter + k, sizeof(filter) - k, "All files") + 1;
    k += snprintf(filter + k, sizeof(filter) - k, "*.*") + 1;
    filter[k] = 0;
    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = path;
    ofn.nMaxFile = (DWORD)n;
    ofn.lpstrTitle = title;
    snprintf(defext, sizeof(defext), "%.*s", (int)strcspn(exts, ";"), exts);
    ofn.lpstrDefExt = defext;
    ofn.Flags = OFN_NOCHANGEDIR | (save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);
    ok = save ? GetSaveFileNameA(&ofn) : GetOpenFileNameA(&ofn);
    return ok ? 1 : 0;
}
