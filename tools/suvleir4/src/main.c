/*
 * Suvleir 4 - vector screen editor for ZX Spectrum engines.
 * Copyleft The Mojon Twins.
 *
 *   suvleir4 [screen.svl|legacy.bin] [-bg image] [-patterns pats.bin]   GUI
 *   suvleir4 -convert in.bin out.svl [-keep-trailing-plot]               legacy -> SVL
 *   suvleir4 -legacy in.svl out.bin                                     SVL -> legacy
 *   suvleir4 -render in out.bmp [-bg image] [-hard] [-patterns p.bin]   render to BMP
 *   suvleir4 -info in                                                   print stats
 *   suvleir4 -asm in out.asm [label] / -c in out.h [label]              export as source
 */
#include "vec.h"
#include "img.h"
#include "app.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
/* the .exe is a GUI program: attach to the console it was started from so
   the command line modes can print */
static void win_console(void) {
    if (AttachConsole(ATTACH_PARENT_PROCESS)) {
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
    }
}
#endif

static void usage(void) {
    puts("Suvleir 4 - vector screen editor (The Mojon Twins)\n"
         "\n"
         "  suvleir4 [file] [-bg image] [-patterns pats.bin]   open the editor\n"
         "  suvleir4 -convert in.bin out.svl [-keep-trailing-plot]\n"
         "  suvleir4 -legacy in.svl out.bin\n"
         "  suvleir4 -render in out.bmp [-bg image] [-hard] [-patterns pats.bin]\n"
         "  suvleir4 -info in\n"
         "  suvleir4 -asm in out.asm [label]\n"
         "  suvleir4 -c in out.h [label]\n"
         "\n"
         "Input files may be SVL (Suvleir 4) or legacy Suvleir 3 (.bin from draw.exe);\n"
         "the format is detected automatically.");
}

static const char *opt(int argc, char **argv, const char *name) {
    int i;
    for (i = 1; i < argc - 1; i++) if (!strcmp(argv[i], name)) return argv[i + 1];
    return NULL;
}
static int flag(int argc, char **argv, const char *name) {
    int i;
    for (i = 1; i < argc; i++) if (!strcmp(argv[i], name)) return 1;
    return 0;
}

static void label_from(const char *path, char *out, size_t n) {
    const char *b = path, *s;
    size_t i = 0;
    for (s = path; *s; s++) if (*s == '/' || *s == '\\') b = s + 1;
    while (*b && *b != '.' && i + 1 < n) {
        char c = *b++;
        out[i++] = ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) ? c : '_';
    }
    out[i] = 0;
    if (out[0] >= '0' && out[0] <= '9' && i + 2 < n) { memmove(out + 4, out, i + 1); memcpy(out, "svl_", 4); }
}

static int cli(int argc, char **argv) {
    VecDoc d;
    VecPatterns pats;
    char msg[256] = "";
    const char *cmd = argv[1];
    int r = 0;
    vec_init(&d);
    vec_default_patterns(&pats);
    if (opt(argc, argv, "-patterns") && vec_load_patterns(&pats, opt(argc, argv, "-patterns"))) {
        fprintf(stderr, "can't load patterns %s\n", opt(argc, argv, "-patterns"));
        return 1;
    }
    if (argc < 3) { usage(); return 1; }

    if (!strcmp(cmd, "-convert")) {
        size_t len;
        uint8_t *b;
        char warn[160];
        if (argc < 4) { usage(); return 1; }
        b = read_file(argv[2], &len);
        if (!b || vec_legacy_decode(&d, b, len, flag(argc, argv, "-keep-trailing-plot"), warn, sizeof(warn))) {
            fprintf(stderr, "can't read legacy file %s\n", argv[2]); return 1;
        }
        if (warn[0]) fprintf(stderr, "%s: %s\n", argv[2], warn);
        free(b);
        r = vec_save_file(&d, argv[3], msg, sizeof(msg));
        puts(msg);
    } else if (!strcmp(cmd, "-legacy")) {
        uint8_t *b = (uint8_t *)malloc(65536);
        size_t n;
        if (argc < 4 || vec_load_file(&d, argv[2], 0, msg, sizeof(msg))) { fprintf(stderr, "%s\n", msg); return 1; }
        n = vec_legacy_encode(&d, b, 65536);
        r = write_file(argv[3], b, n);
        printf("wrote %s (%u bytes, legacy Suvleir 3)\n", argv[3], (unsigned)n);
        free(b);
    } else if (!strcmp(cmd, "-render")) {
        static uint8_t pix[SCR_W * SCR_H];
        static uint32_t out[SCR_W * SCR_H], bg[SCR_W * SCR_H];
        int i, have_bg = 0;
        if (argc < 4 || vec_load_file(&d, argv[2], 0, msg, sizeof(msg))) { fprintf(stderr, "%s\n", msg); return 1; }
        if (opt(argc, argv, "-bg")) {
            char m2[200] = "";
            if (img_load(opt(argc, argv, "-bg"), bg, m2, sizeof(m2))) { fprintf(stderr, "%s\n", m2); return 1; }
            have_bg = 1;
        }
        vec_render(&d, &pats, -1, pix);
        for (i = 0; i < SCR_W * SCR_H; i++) {
            uint32_t paper = have_bg ? (((bg[i] >> 1) & 0x7F7F7F) + 0x808080) : 0xD7D7D7;
            out[i] = pix[i] ? 0x000000 : paper;
            if (flag(argc, argv, "-hard")) {
                int h = d.hard[(i / SCR_W / 8) * HARD_W + (i % SCR_W) / 8];
                if (h) {
                    static const uint32_t hc[4] = {0, 0xFF3030, 0x3060FF, 0x30C030};
                    uint32_t c = hc[h], o = out[i];
                    out[i] = ((((o >> 16) & 255) + ((c >> 16) & 255)) / 2) << 16 |
                             ((((o >> 8) & 255) + ((c >> 8) & 255)) / 2) << 8 | (((o & 255) + (c & 255)) / 2);
                }
            }
        }
        r = img_save_bmp(argv[3], out, SCR_W, SCR_H);
        printf("rendered %s -> %s\n", argv[2], argv[3]);
    } else if (!strcmp(cmd, "-info")) {
        int i, counts[4] = {0, 0, 0, 0}, hc[4] = {0, 0, 0, 0};
        if (vec_load_file(&d, argv[2], 0, msg, sizeof(msg))) { fprintf(stderr, "%s\n", msg); return 1; }
        puts(msg);
        for (i = 0; i < d.nops; i++) counts[d.ops[i].op]++;
        for (i = 0; i < HARD_CELLS; i++) hc[d.hard[i] & 3]++;
        printf("ops: %d (plot %d, line %d, fill %d, move %d)\n", d.nops, counts[OP_PLOT], counts[OP_LINE], counts[OP_FILL], counts[OP_MOVE]);
        printf("SVL size: %u bytes (2 header + %u commands + 1 end + %u hardness)\n", (unsigned)vec_svl_size(&d),
               (unsigned)vec_cmd_bytes(&d), (unsigned)vec_hard_bytes(&d));
        printf("hardness cells: 0:%d 1:%d 2:%d 3:%d\n", hc[0], hc[1], hc[2], hc[3]);
    } else if (!strcmp(cmd, "-asm") || !strcmp(cmd, "-c")) {
        char label[64];
        if (argc < 4 || vec_load_file(&d, argv[2], 0, msg, sizeof(msg))) { fprintf(stderr, "%s\n", msg); return 1; }
        if (argc > 4 && argv[4][0] != '-') snprintf(label, sizeof(label), "%s", argv[4]);
        else label_from(argv[2], label, sizeof(label));
        r = !strcmp(cmd, "-asm") ? vec_export_asm(&d, argv[3], label) : vec_export_c(&d, argv[3], label);
        printf("exported %s as %s (label %s)\n", argv[2], argv[3], label);
    } else {
        usage();
        r = 1;
    }
    vec_free(&d);
    return r ? 1 : 0;
}

int main(int argc, char **argv) {
#ifdef _WIN32
    if (argc > 1 && argv[1][0] == '-') win_console();
#endif
    if (argc > 1 && (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help") || !strcmp(argv[1], "/?"))) {
        usage();
        return 0;
    }
    if (argc > 1 && argv[1][0] == '-' && strcmp(argv[1], "-bg") && strcmp(argv[1], "-patterns"))
        return cli(argc, argv);
    {
        const char *file = NULL, *bg = NULL, *pats = NULL;
        int i;
        for (i = 1; i < argc; i++) {
            if (!strcmp(argv[i], "-bg") && i + 1 < argc) bg = argv[++i];
            else if (!strcmp(argv[i], "-patterns") && i + 1 < argc) pats = argv[++i];
            else if (argv[i][0] != '-') file = argv[i];
        }
        return app_run(file, bg, pats);
    }
}
