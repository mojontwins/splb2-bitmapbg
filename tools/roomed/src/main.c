/*
 * roomed - room editor for 8x8-cell tile engines (made for CJMC).
 *
 *   roomed [rooms.txt]                       the editor
 *   roomed -render rooms.txt N out.bmp       draw room N (with the tileset and
 *                                            frame named in the file)
 *   roomed -export rooms.txt out.h           C arrays (NAME_data, NAME_off)
 *   roomed -asm rooms.txt out.asm            the same as assembler (defb / defw)
 *   roomed -bin rooms.txt N out.bin          room N in binary
 *   roomed -info rooms.txt                   rooms, objects, bytes
 */
#include "app.h"
#include "rooms.h"
#include "img.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Project proj;
static Tileset tiles;

static int load(const char *path) {
    char err[256];
    if (proj_load(&proj, path, err, sizeof(err))) { fprintf(stderr, "roomed: %s\n", err); return -1; }
    return 0;
}

/* exports need rooms the encoding can express */
static int check(void) {
    int r, o;
    const char *e = proj_check(&proj, &r, &o);
    if (!e) return 0;
    if (r >= 0) fprintf(stderr, "roomed: room %d, object %d: %s\n", r, o, e);
    else fprintf(stderr, "roomed: %s\n", e);
    return -1;
}

static int room_arg(const char *s) {
    int n = atoi(s);
    if (n < 0 || n >= proj.nrooms) { fprintf(stderr, "roomed: no room %s (0-%d)\n", s, proj.nrooms - 1); exit(1); }
    return n;
}

int main(int argc, char **argv) {
    if (argc >= 5 && !strcmp(argv[1], "-render")) {
        static uint32_t out[MAX_SW * 8 * MAX_SH * 8], bg[MAX_SW * 8 * MAX_SH * 8];
        char path[1200];
        const char *rel;
        int n, hasbg = 0;
        if (load(argv[2])) return 1;
        n = room_arg(argv[3]);
        proj_path(argv[2], proj.tileset, path, sizeof(path));
        if (tileset_load(&tiles, path)) { fprintf(stderr, "roomed: can't load the tileset %s\n", path); return 1; }
        rel = proj_backdrop(&proj, &proj.rooms[n]);
        if (rel[0]) {
            proj_path(argv[2], rel, path, sizeof(path));
            FILE *t = fopen(path, "rb");
            if (t) fclose(t);
            else if (proj.background[0]) {                      /* missing: the default */
                fprintf(stderr, "roomed: no %s, using the default backdrop\n", path);
                proj_path(argv[2], proj.background, path, sizeof(path));
            }
            hasbg = !backdrop_load(&proj, path, bg);
            if (!hasbg) fprintf(stderr, "roomed: can't load the backdrop %s\n", path);
        }
        room_render(&proj, &proj.rooms[n], &tiles, hasbg ? bg : NULL, out, NULL, -1);
        if (img_save_bmp(argv[4], out, proj.sw * 8, proj.sh * 8)) { fprintf(stderr, "roomed: can't write %s\n", argv[4]); return 1; }
        return 0;
    }
    if (argc >= 4 && !strcmp(argv[1], "-asm")) {
        if (load(argv[2]) || check()) return 1;
        if (proj_export_asm(&proj, argv[3])) { fprintf(stderr, "roomed: can't write %s\n", argv[3]); return 1; }
        return 0;
    }
    if (argc >= 4 && !strcmp(argv[1], "-export")) {
        if (load(argv[2]) || check()) return 1;
        if (proj_export_c(&proj, argv[3])) { fprintf(stderr, "roomed: can't write %s\n", argv[3]); return 1; }
        return 0;
    }
    if (argc >= 5 && !strcmp(argv[1], "-bin")) {
        if (load(argv[2]) || check()) return 1;
        if (room_export_bin(&proj, &proj.rooms[room_arg(argv[3])], argv[4])) { fprintf(stderr, "roomed: can't write %s\n", argv[4]); return 1; }
        return 0;
    }
    if (argc >= 3 && !strcmp(argv[1], "-info")) {
        int i, total = 0;
        if (load(argv[2])) return 1;
        printf("screen %dx%d  area %d,%d %dx%d  header %d bytes  solid bit %d  encoding %s  export %s\n",
               proj.sw, proj.sh, proj.ax, proj.ay, proj.aw, proj.ah, proj.hdr_n, proj.solid_bit,
               proj.compact ? "compact" : "plain", proj.export_name);
        for (i = 0; i < MAX_BG; i++) if (proj.bg_params[i] >= 0) printf("background type %d: %d parameter bytes\n", i, proj.bg_params[i]);
        printf("tileset %s  backdrop %s\n", proj.tileset, proj.background[0] ? proj.background : "(none)");
        for (i = 0; i < MAX_BG; i++) if (proj.bg_type[i][0]) printf("backdrop for type %d: %s\n", i, proj.bg_type[i]);
        for (i = 0; i < proj.ncust; i++) printf("object type %d: %s %dx%d%s%s\n", proj.cust[i].type, proj.cust[i].name, proj.cust[i].w, proj.cust[i].h, proj.cust[i].solid ? " solid" : "", proj.cust[i].number ? " number" : "");
        for (i = 0; i < proj.nrooms; i++) {
            printf("room %d \"%s\": %d objects, %d bytes\n", i, proj.rooms[i].name, proj.rooms[i].n, room_bytes(&proj, &proj.rooms[i]));
            total += room_bytes(&proj, &proj.rooms[i]);
        }
        printf("total %d bytes\n", total);
        return 0;
    }
    if (argc >= 2 && argv[1][0] == '-') {
        fprintf(stderr, "usage: roomed [rooms.txt]\n"
                        "       roomed -render rooms.txt N out.bmp | -export rooms.txt out.h |\n"
                        "              -asm rooms.txt out.asm |\n"
                        "              -bin rooms.txt N out.bin | -info rooms.txt\n");
        return 1;
    }
    return app_run(argc >= 2 ? argv[1] : NULL);
}
