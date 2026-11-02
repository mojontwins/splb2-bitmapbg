/*
 * Beeptune - a two channel beeper music tracker for the ZX Spectrum.
 * Copyleft The Mojon Twins.
 *
 *   beeptune [song.bts | song.txt]                 the tracker
 *   beeptune -wav  in out.wav [-loop SECONDS]      render as the Spectrum plays it
 *   beeptune -asm  in out.asm [label]              z80asm source (defb)
 *   beeptune -c    in out.h   [label]              C array
 *   beeptune -bts  in out.bts                      the player's binary format
 *   beeptune -txt  in out.txt                      the text format
 *   beeptune -info in                              sizes, length
 * "in" may be a .bts or a .txt song.
 */
#include "song.h"
#include "app.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static void win_console(void) {
    if (AttachConsole(ATTACH_PARENT_PROCESS)) {
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
    }
}
#endif

static void usage(void) {
    puts("Beeptune - two channel beeper music tracker (The Mojon Twins)\n"
         "\n"
         "  beeptune [song.bts | song.txt]             open the tracker\n"
         "  beeptune -wav  in out.wav [-loop SECONDS]  render it as the Spectrum plays it\n"
         "  beeptune -asm  in out.asm [label]          z80asm source\n"
         "  beeptune -c    in out.h [label]            C array\n"
         "  beeptune -bts  in out.bts                  the player's binary format\n"
         "  beeptune -txt  in out.txt                  the text format\n"
         "  beeptune -info in                          size and length\n"
         "\n"
         "in: a .bts (binary) or .txt (text) song.");
}

static void label_from(const char *path, char *out, size_t n) {
    const char *b = path, *s;
    size_t i = 0;
    for (s = path; *s; s++) if (*s == '/' || *s == '\\') b = s + 1;
    while (*b && *b != '.' && i + 1 < n) {
        char c = *b++;
        out[i++] = isalnum((unsigned char)c) ? c : '_';
    }
    out[i] = 0;
    if (!i || isdigit((unsigned char)out[0])) { memmove(out + 5, out, i + 1); memcpy(out, "song_", 5); }
}

static int cli(int argc, char **argv) {
    static Song s;
    char msg[256], label[64];
    const char *cmd = argv[1];
    int r = 0;
    if (argc < 3) { usage(); return 1; }
    if (song_load(&s, argv[2], msg, sizeof(msg))) { fprintf(stderr, "%s: %s\n", argv[2], msg); return 1; }
    if (!strcmp(cmd, "-info")) {
        int i, rows = 0;
        for (i = 0; i < s.nord; i++) rows += s.pat[s.ord[i]].rows;
        printf("%s: %s\n%u bytes, %d rows played, %.1f s (+ loop from entry %d)\n", argv[2], msg,
               (unsigned)song_encode(&s, NULL, 0), rows, rows * s.speed / 50.0, s.loopto);
        return 0;
    }
    if (argc < 4) { usage(); return 1; }
    if (argc > 4 && argv[4][0] != '-') snprintf(label, sizeof(label), "%s", argv[4]);
    else label_from(argv[2], label, sizeof(label));
    if (!strcmp(cmd, "-wav")) {
        int16_t *smp;
        RowPos *rp;
        int nr, loop = 0, secs = 600, i;
        size_t n;
        for (i = 4; i < argc - 1; i++) if (!strcmp(argv[i], "-loop")) { loop = 1; secs = atoi(argv[i + 1]); }
        n = song_render(&s, 0, -1, loop, secs, 44100, &smp, &rp, &nr);
        r = wav_write(argv[3], smp, n, 44100);
        printf("wrote %s (%.1f s)\n", argv[3], n / 44100.0);
    } else if (!strcmp(cmd, "-asm")) {
        r = song_export_asm(&s, argv[3], label);
        printf("exported %s (label %s)\n", argv[3], label);
    } else if (!strcmp(cmd, "-c")) {
        r = song_export_c(&s, argv[3], label);
        printf("exported %s (label %s)\n", argv[3], label);
    } else if (!strcmp(cmd, "-bts")) {
        r = song_save(&s, argv[3]);
        printf("wrote %s (%u bytes)\n", argv[3], (unsigned)song_encode(&s, NULL, 0));
    } else if (!strcmp(cmd, "-txt")) {
        r = song_export_txt(&s, argv[3]);
        printf("wrote %s\n", argv[3]);
    } else { usage(); return 1; }
    if (r) fprintf(stderr, "can't write %s\n", argv[3]);
    return r ? 1 : 0;
}

int main(int argc, char **argv) {
#ifdef _WIN32
    if (argc > 1 && argv[1][0] == '-') win_console();
#endif
    if (argc > 1 && (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help") || !strcmp(argv[1], "/?"))) { usage(); return 0; }
    if (argc > 1 && argv[1][0] == '-') return cli(argc, argv);
    return app_run(argc > 1 ? argv[1] : NULL);
}
