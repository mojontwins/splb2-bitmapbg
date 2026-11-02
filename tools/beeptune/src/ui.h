/* Beeptune - minimal immediate-mode UI on a 32-bit framebuffer */
#ifndef UI_H
#define UI_H
#include <stdint.h>

typedef struct {
    uint32_t *fb;
    int w, h;
    int mx, my;           /* mouse position                         */
    int down[4];          /* button held (index MB_*)               */
    int pressed[4];       /* went down this frame                   */
    int released[4];      /* went up this frame                     */
    int dbl;              /* left press was a double click          */
    int wheel;
    int blocked;          /* a modal is open: widgets outside ignore input */
    int modal_x, modal_y, modal_w, modal_h;
} UI;

extern UI ui;

/* colours */
#define C_BG       0x20242C
#define C_PANEL    0x2C313B
#define C_PANEL2   0x353B47
#define C_BORDER   0x4A5263
#define C_TEXT     0xE6E9EF
#define C_DIM      0x9AA3B5
#define C_ACCENT   0xF2B134
#define C_ACCENT2  0x4FA3F7
#define C_BUTTON   0x404858
#define C_BUTTONH  0x525C70
#define C_RED      0xE5534B
#define C_GREEN    0x57C26A

void ui_begin(uint32_t *fb, int w, int h);
void ui_rect(int x, int y, int w, int h, uint32_t c);
void ui_frame(int x, int y, int w, int h, uint32_t c);
void ui_hline(int x, int y, int w, uint32_t c);
void ui_vline(int x, int y, int h, uint32_t c);
void ui_pixel(int x, int y, uint32_t c);
void ui_blend_rect(int x, int y, int w, int h, uint32_t c, int alpha /*0..256*/);
int  ui_text(int x, int y, const char *s, uint32_t c);           /* returns width */
int  ui_textf(int x, int y, uint32_t c, const char *fmt, ...);
int  ui_text_width(const char *s);
int  ui_inside(int x, int y, int w, int h);                        /* mouse inside (respects modal) */
int  ui_button(int x, int y, int w, int h, const char *label, int active);
int  ui_check(int x, int y, const char *label, int *value);
int  ui_clicked(int x, int y, int w, int h, int button);          /* pressed inside this frame */
uint32_t ui_mix(uint32_t a, uint32_t b, int t /*0..256 towards b*/);

#endif
