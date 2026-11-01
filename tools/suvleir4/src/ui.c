/* Suvleir 4 - minimal immediate-mode UI on a 32-bit framebuffer */
#include "ui.h"
#include "plat.h"
#include "../third_party/font8x8_basic.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

UI ui;

void ui_begin(uint32_t *fb, int w, int h) { ui.fb = fb; ui.w = w; ui.h = h; }

void ui_pixel(int x, int y, uint32_t c) {
    if (x >= 0 && y >= 0 && x < ui.w && y < ui.h) ui.fb[y * ui.w + x] = c;
}

void ui_rect(int x, int y, int w, int h, uint32_t c) {
    int i, j;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > ui.w) w = ui.w - x;
    if (y + h > ui.h) h = ui.h - y;
    for (j = 0; j < h; j++) {
        uint32_t *p = ui.fb + (y + j) * ui.w + x;
        for (i = 0; i < w; i++) p[i] = c;
    }
}

uint32_t ui_mix(uint32_t a, uint32_t b, int t) {
    uint32_t r = (((a >> 16) & 255) * (256 - t) + ((b >> 16) & 255) * t) >> 8;
    uint32_t g = (((a >> 8) & 255) * (256 - t) + ((b >> 8) & 255) * t) >> 8;
    uint32_t bl = ((a & 255) * (256 - t) + (b & 255) * t) >> 8;
    return (r << 16) | (g << 8) | bl;
}

void ui_blend_rect(int x, int y, int w, int h, uint32_t c, int alpha) {
    int i, j;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > ui.w) w = ui.w - x;
    if (y + h > ui.h) h = ui.h - y;
    for (j = 0; j < h; j++) {
        uint32_t *p = ui.fb + (y + j) * ui.w + x;
        for (i = 0; i < w; i++) p[i] = ui_mix(p[i], c, alpha);
    }
}

void ui_hline(int x, int y, int w, uint32_t c) { ui_rect(x, y, w, 1, c); }
void ui_vline(int x, int y, int h, uint32_t c) { ui_rect(x, y, 1, h, c); }

void ui_frame(int x, int y, int w, int h, uint32_t c) {
    ui_hline(x, y, w, c); ui_hline(x, y + h - 1, w, c);
    ui_vline(x, y, h, c); ui_vline(x + w - 1, y, h, c);
}

int ui_text(int x, int y, const char *s, uint32_t c) {
    int x0 = x;
    for (; *s; s++, x += 8) {
        unsigned ch = (unsigned char)*s;
        int r, b;
        if (ch >= 128) ch = '?';
        for (r = 0; r < 8; r++) {
            unsigned char bits = font8x8_basic[ch][r];
            for (b = 0; b < 8; b++) if (bits & (1 << b)) ui_pixel(x + b, y + r, c);
        }
    }
    return x - x0;
}

int ui_textf(int x, int y, uint32_t c, const char *fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    return ui_text(x, y, buf, c);
}

int ui_text_width(const char *s) { return (int)strlen(s) * 8; }

int ui_inside(int x, int y, int w, int h) {
    if (ui.blocked && !(ui.mx >= ui.modal_x && ui.my >= ui.modal_y &&
                        ui.mx < ui.modal_x + ui.modal_w && ui.my < ui.modal_y + ui.modal_h))
        return 0;
    return ui.mx >= x && ui.my >= y && ui.mx < x + w && ui.my < y + h;
}

int ui_clicked(int x, int y, int w, int h, int button) {
    return ui.pressed[button] && ui_inside(x, y, w, h);
}

int ui_button(int x, int y, int w, int h, const char *label, int active) {
    int hot = ui_inside(x, y, w, h);
    uint32_t bg = active ? C_ACCENT : hot ? C_BUTTONH : C_BUTTON;
    uint32_t fg = active ? 0x1A1A1A : C_TEXT;
    ui_rect(x, y, w, h, bg);
    ui_frame(x, y, w, h, active ? C_ACCENT : C_BORDER);
    ui_text(x + (w - ui_text_width(label)) / 2, y + (h - 8) / 2, label, fg);
    return hot && ui.pressed[BTN_LEFT];
}

int ui_check(int x, int y, const char *label, int *value) {
    int w = 12 + 6 + ui_text_width(label), hot = ui_inside(x, y - 2, w, 14);
    ui_rect(x, y - 2, 12, 12, hot ? C_BUTTONH : C_BUTTON);
    ui_frame(x, y - 2, 12, 12, C_BORDER);
    if (*value) ui_rect(x + 3, y + 1, 6, 6, C_ACCENT);
    ui_text(x + 18, y, label, C_TEXT);
    if (hot && ui.pressed[BTN_LEFT]) { *value = !*value; return 1; }
    return 0;
}
