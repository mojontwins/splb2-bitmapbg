/* Beeptune - songs: data, files, and sound */
#include "song.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void pattern_clear(Pattern *p, int rows) {
    memset(p, 0, sizeof(*p));
    p->rows = rows;
}

void song_init(Song *s) {
    memset(s, 0, sizeof(*s));
    s->speed = 6;
    s->nord = 1;
    s->npat = 1;
    pattern_clear(&s->pat[0], 32);
}

/* ------------------------------------------------------------------ */
/* notes                                                               */

static const char *NAMES[12] = {"C-", "C#", "D-", "D#", "E-", "F-", "F#", "G-", "G#", "A-", "A#", "B-"};

const char *note_name(int code, char out[4]) {
    if (code == N_HOLD) strcpy(out, "---");
    else if (code == N_OFF) strcpy(out, "===");
    else if (code >= N_FIRST && code < N_FIRST + NOTES) {
        int n = code - N_FIRST;
        snprintf(out, 4, "%s%d", NAMES[n % 12], n / 12 + 1);
    } else strcpy(out, "???");
    return out;
}

uint16_t note_step(int code) {
    double f;
    if (code < N_FIRST || code >= N_FIRST + NOTES) return 0;
    f = 440.0 * pow(2.0, (code - N_FIRST - 45) / 12.0);
    return (uint16_t)(f * 65536.0 * TURN_T / CPU_HZ + 0.5);
}

/* "C#4", "c-4", "---", "===", "." -> code, or -1 */
static int parse_note(const char *t) {
    int i;
    if (!strcmp(t, "---") || !strcmp(t, ".") || !strcmp(t, "...")) return N_HOLD;
    if (!strcmp(t, "===") || !strcmp(t, "off")) return N_OFF;
    if (strlen(t) != 3 || t[2] < '1' || t[2] > '7') return -1;
    for (i = 0; i < 12; i++)
        if (toupper((unsigned char)t[0]) == NAMES[i][0] && t[1] == NAMES[i][1])
            return N_FIRST + (t[2] - '1') * 12 + i;
    return -1;
}

/* ------------------------------------------------------------------ */
/* the binary format                                                   */

size_t song_encode(const Song *s, uint8_t *out, size_t max) {
    size_t n = 4 + s->nord + 2 * s->npat, i;
    int p, r;
    for (p = 0; p < s->npat; p++) n += 1 + 2 * s->pat[p].rows;
    if (!out) return n;
    if (n > max) return 0;
    out[0] = (uint8_t)s->speed; out[1] = (uint8_t)s->nord;
    out[2] = (uint8_t)s->loopto; out[3] = (uint8_t)s->npat;
    for (i = 0; i < (size_t)s->nord; i++) out[4 + i] = s->ord[i];
    n = 4 + s->nord + 2 * s->npat;
    for (p = 0; p < s->npat; p++) {
        out[4 + s->nord + 2 * p] = (uint8_t)(n & 255);
        out[4 + s->nord + 2 * p + 1] = (uint8_t)(n >> 8);
        out[n++] = (uint8_t)s->pat[p].rows;
        for (r = 0; r < s->pat[p].rows; r++) { out[n++] = s->pat[p].n[r][0]; out[n++] = s->pat[p].n[r][1]; }
    }
    return n;
}

int song_decode(Song *s, const uint8_t *b, size_t n, char *msg, size_t msglen) {
    int p, r, i;
    song_init(s);
    if (n < 6 || !b[0] || !b[1] || !b[3] || b[2] >= b[1] || b[3] > MAX_PAT ||
        (size_t)(4 + b[1] + 2 * b[3]) > n) {
        snprintf(msg, msglen, "not a Beeptune song");
        return 1;
    }
    s->speed = b[0]; s->nord = b[1]; s->loopto = b[2]; s->npat = b[3];
    for (i = 0; i < s->nord; i++) {
        s->ord[i] = b[4 + i];
        if (s->ord[i] >= s->npat) { snprintf(msg, msglen, "order entry %d uses pattern %d, there are %d", i, s->ord[i], s->npat); return 1; }
    }
    for (p = 0; p < s->npat; p++) {
        size_t o = b[4 + s->nord + 2 * p] | (b[4 + s->nord + 2 * p + 1] << 8);
        if (o >= n || !b[o] || b[o] > MAX_ROWS || o + 1 + 2 * (size_t)b[o] > n) {
            snprintf(msg, msglen, "pattern %d is damaged (or longer than %d rows)", p, MAX_ROWS);
            return 1;
        }
        pattern_clear(&s->pat[p], b[o]);
        for (r = 0; r < b[o]; r++) {
            s->pat[p].n[r][0] = b[o + 1 + 2 * r];
            s->pat[p].n[r][1] = b[o + 2 + 2 * r];
        }
    }
    snprintf(msg, msglen, "%d patterns, %d in the order list, speed %d", s->npat, s->nord, s->speed);
    return 0;
}

static uint8_t *read_all(const char *path, size_t *len) {
    FILE *f = fopen(path, "rb");
    uint8_t *b;
    long n;
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
    b = (uint8_t *)malloc((size_t)n + 1);
    if (b && fread(b, 1, (size_t)n, f) != (size_t)n) { free(b); b = NULL; }
    fclose(f);
    if (b) { b[n] = 0; *len = (size_t)n; }
    return b;
}

int song_load(Song *s, const char *path, char *msg, size_t msglen) {
    size_t n;
    uint8_t *b = read_all(path, &n);
    const char *dot = strrchr(path, '.');
    int r;
    if (!b) { snprintf(msg, msglen, "can't read %s", path); return 1; }
    if (dot && (!strcmp(dot, ".txt") || !strcmp(dot, ".TXT"))) r = song_import_txt(s, (const char *)b, msg, msglen);
    else r = song_decode(s, b, n, msg, msglen);
    free(b);
    return r;
}

int song_save(const Song *s, const char *path) {
    uint8_t buf[70000];
    size_t n = song_encode(s, buf, sizeof(buf));
    FILE *f = fopen(path, "wb");
    if (!f) return 1;
    if (fwrite(buf, 1, n, f) != n) { fclose(f); return 1; }
    return fclose(f) != 0;
}

int song_export_asm(const Song *s, const char *path, const char *label) {
    uint8_t buf[70000];
    size_t n = song_encode(s, buf, sizeof(buf)), i;
    FILE *f = fopen(path, "w");
    if (!f) return 1;
    fprintf(f, "; Beeptune song (%u bytes): play it with beeptune.h\n", (unsigned)n);
    fprintf(f, "%s:\n", label);
    for (i = 0; i < n; i++)
        fprintf(f, "%s%u%s", i % 16 ? "," : "\tdefb ", buf[i], (i % 16 == 15 || i == n - 1) ? "\n" : "");
    return fclose(f) != 0;
}

int song_export_c(const Song *s, const char *path, const char *label) {
    uint8_t buf[70000];
    size_t n = song_encode(s, buf, sizeof(buf)), i;
    FILE *f = fopen(path, "w");
    if (!f) return 1;
    fprintf(f, "/* Beeptune song (%u bytes): bt_play (%s), bt_loop (%s) or bt_init (%s) */\n",
            (unsigned)n, label, label, label);
    fprintf(f, "unsigned char %s[%u] = {\n", label, (unsigned)n);
    for (i = 0; i < n; i++)
        fprintf(f, "%s%u%s", i % 16 ? "," : "\t", buf[i], i == n - 1 ? "\n" : i % 16 == 15 ? ",\n" : "");
    fprintf(f, "};\n");
    return fclose(f) != 0;
}

/* ------------------------------------------------------------------ */
/* the text format: readable, diffable, easy to write by hand          */

int song_export_txt(const Song *s, const char *path) {
    FILE *f = fopen(path, "w");
    int i, p, r;
    char a[4], b[4];
    if (!f) return 1;
    fprintf(f, "# Beeptune song. Notes: C-4 C#4 ... (octaves 1-7), --- keep playing, === silence\n");
    fprintf(f, "speed %d\nloop %d\norder", s->speed, s->loopto);
    for (i = 0; i < s->nord; i++) fprintf(f, " %d", s->ord[i]);
    fprintf(f, "\n");
    for (p = 0; p < s->npat; p++) {
        fprintf(f, "\npattern %d %d\n", p, s->pat[p].rows);
        for (r = 0; r < s->pat[p].rows; r++)
            fprintf(f, "%s %s\n", note_name(s->pat[p].n[r][0], a), note_name(s->pat[p].n[r][1], b));
    }
    return fclose(f) != 0;
}

int song_import_txt(Song *s, const char *text, char *msg, size_t msglen) {
    const char *t = text;
    int line = 0, cur = -1, row = 0, i;
    song_init(s);
    s->npat = 0;
    s->nord = 0;
    while (*t) {
        char ln[512], w[4][32];
        size_t l = strcspn(t, "\r\n");
        int nw;
        line++;
        snprintf(ln, sizeof(ln), "%.*s", (int)(l < 511 ? l : 511), t);
        t += l;
        while (*t == '\r' || *t == '\n') { if (*t == '\n' && t[1] == '\r') {} t++; }
        if (strchr(ln, '#')) *strchr(ln, '#') = 0;
        nw = sscanf(ln, "%31s %31s %31s %31s", w[0], w[1], w[2], w[3]);
        if (nw <= 0) continue;
        if (!strcmp(w[0], "speed") && nw >= 2) s->speed = atoi(w[1]);
        else if (!strcmp(w[0], "loop") && nw >= 2) s->loopto = atoi(w[1]);
        else if (!strcmp(w[0], "order")) {
            const char *q = ln + 5;
            int v, k;
            while (sscanf(q, "%d%n", &v, &k) == 1 && s->nord < MAX_ORD) { s->ord[s->nord++] = (uint8_t)v; q += k; }
        } else if (!strcmp(w[0], "pattern") && nw >= 3) {
            cur = atoi(w[1]);
            if (cur < 0 || cur >= MAX_PAT || atoi(w[2]) < 1 || atoi(w[2]) > MAX_ROWS) {
                snprintf(msg, msglen, "line %d: pattern number 0-%d, rows 1-%d", line, MAX_PAT - 1, MAX_ROWS);
                return 1;
            }
            pattern_clear(&s->pat[cur], atoi(w[2]));
            if (cur + 1 > s->npat) s->npat = cur + 1;
            row = 0;
        } else if (cur >= 0 && nw >= 2) {
            int a = parse_note(w[0]), b = parse_note(w[1]);
            if (a < 0 || b < 0) { snprintf(msg, msglen, "line %d: bad note", line); return 1; }
            if (row >= s->pat[cur].rows) { snprintf(msg, msglen, "line %d: pattern %d has only %d rows", line, cur, s->pat[cur].rows); return 1; }
            s->pat[cur].n[row][0] = (uint8_t)a; s->pat[cur].n[row][1] = (uint8_t)b;
            row++;
        } else {
            snprintf(msg, msglen, "line %d: what is \"%s\"?", line, w[0]);
            return 1;
        }
    }
    if (!s->npat) { snprintf(msg, msglen, "no patterns"); return 1; }
    for (i = 0; i < s->npat; i++) if (!s->pat[i].rows) pattern_clear(&s->pat[i], 16);
    if (!s->nord) { s->ord[0] = 0; s->nord = 1; }
    for (i = 0; i < s->nord; i++)
        if (s->ord[i] >= s->npat) { snprintf(msg, msglen, "the order list uses pattern %d, which doesn't exist", s->ord[i]); return 1; }
    if (s->speed < 1 || s->speed > 255) { snprintf(msg, msglen, "speed must be 1-255"); return 1; }
    if (s->loopto < 0 || s->loopto >= s->nord) s->loopto = 0;
    snprintf(msg, msglen, "%d patterns, %d in the order list, speed %d", s->npat, s->nord, s->speed);
    return 0;
}

/* ------------------------------------------------------------------ */
/* sound: the Z80 loop, turn by turn                                   */

typedef struct {
    int16_t *out;
    size_t n, cap;
    double per;          /* T-states per output sample          */
    double t;            /* T-states into the current sample    */
    double acc;          /* speaker level * time in the sample  */
    double hp_x, hp_y;   /* DC filter                           */
    double lp_a, lp1, lp2; /* low pass                          */
} Synth;

static void emit(Synth *z, double v) {
    double y;
    /* the speaker doesn't follow the 26.9 kHz switching between the two
       channels (it would come out as a whistle near 17 kHz at 44.1 kHz):
       a gentle low pass, two poles at 6 kHz */
    z->lp1 += z->lp_a * (v - z->lp1);
    z->lp2 += z->lp_a * (z->lp1 - z->lp2);
    v = z->lp2;
    y = v - z->hp_x + 0.996 * z->hp_y;          /* and it has no DC   */
    z->hp_x = v; z->hp_y = y;
    if (z->n == z->cap) {
        z->cap = z->cap ? z->cap * 2 : 1 << 16;
        z->out = (int16_t *)realloc(z->out, z->cap * sizeof(int16_t));
    }
    y *= 22000.0;
    if (y > 32767) y = 32767;
    if (y < -32768) y = -32768;
    z->out[z->n++] = (int16_t)y;
}

/* the speaker at `level` for `tt` T-states */
static void level(Synth *z, int lv, double tt) {
    while (z->t + tt >= z->per) {
        double part = z->per - z->t;
        z->acc += lv * part;
        emit(z, z->acc / z->per);
        tt -= part;
        z->acc = 0; z->t = 0;
    }
    z->acc += lv * tt;
    z->t += tt;
}

size_t song_render(const Song *s, int from_ord, int only_pat, int loop, int max_seconds,
                   int rate, int16_t **out, RowPos **rowpos, int *nrows) {
    Synth z;
    uint16_t stepa = 0, stepb = 0, acca = 0, accb = 0;
    int ord = from_ord, row = 0, cap = 0, f, k;
    size_t limit = (size_t)max_seconds * rate;
    RowPos *rp = NULL;
    memset(&z, 0, sizeof(z));
    z.per = (double)CPU_HZ / rate;
    z.lp_a = 1.0 - exp(-2.0 * 3.14159265 * 6000.0 / rate);
    *nrows = 0;
    if (ord < 0 || ord >= s->nord) ord = 0;
    while (z.n < limit) {
        const Pattern *p;
        int pn = only_pat >= 0 ? only_pat : s->ord[ord];
        p = &s->pat[pn];
        if (row >= p->rows) {
            row = 0;
            if (only_pat >= 0) { if (!loop) break; continue; }
            if (++ord >= s->nord) { if (!loop) break; ord = s->loopto; }
            continue;
        }
        if (*nrows == cap) { cap = cap ? cap * 2 : 1024; rp = (RowPos *)realloc(rp, cap * sizeof(RowPos)); }
        rp[*nrows].sample = z.n; rp[*nrows].ord = only_pat >= 0 ? -1 : ord;
        rp[*nrows].pat = pn; rp[*nrows].row = row;
        (*nrows)++;
        if (p->n[row][0]) stepa = note_step(p->n[row][0]);
        if (p->n[row][1]) stepb = note_step(p->n[row][1]);
        for (f = 0; f < s->speed; f++) {
            for (k = 0; k < FRAME_TURNS; k++) {
                acca = (uint16_t)(acca + stepa);
                level(&z, acca >> 15, TURN_A_T);
                accb = (uint16_t)(accb + stepb);
                level(&z, accb >> 15, TURN_T - TURN_A_T);
            }
            level(&z, accb >> 15, FRAME_GAP_T);
        }
        row++;
    }
    for (k = 0; k < rate / 10; k++) emit(&z, z.hp_x);   /* a little tail */
    *out = z.out;
    *rowpos = rp;
    return z.n;
}

int wav_write(const char *path, const int16_t *smp, size_t n, int rate) {
    FILE *f = fopen(path, "wb");
    uint8_t h[44];
    uint32_t data = (uint32_t)(n * 2), v;
    size_t i;
    if (!f) return 1;
    memcpy(h, "RIFF", 4); v = 36 + data; memcpy(h + 4, &v, 4);
    memcpy(h + 8, "WAVEfmt ", 8); v = 16; memcpy(h + 16, &v, 4);
    h[20] = 1; h[21] = 0; h[22] = 1; h[23] = 0;               /* PCM, mono */
    v = (uint32_t)rate; memcpy(h + 24, &v, 4);
    v = (uint32_t)rate * 2; memcpy(h + 28, &v, 4);
    h[32] = 2; h[33] = 0; h[34] = 16; h[35] = 0;
    memcpy(h + 36, "data", 4); memcpy(h + 40, &data, 4);
    fwrite(h, 1, 44, f);
    for (i = 0; i < n; i++) { uint8_t b[2] = {(uint8_t)(smp[i] & 255), (uint8_t)((uint16_t)smp[i] >> 8)}; fwrite(b, 1, 2, f); }
    return fclose(f) != 0;
}
