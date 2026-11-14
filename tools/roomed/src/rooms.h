/*
 * roomed - the room model: room file load/save, binary/C/asm export, and a
 * renderer that draws a room the way the engine's driver does (in CJMC:
 * room_draw () in src/printer.h).
 *
 * Nothing here is tied to one game: screen and area size, header size,
 * tileset, backdrops per background type, which attribute bit means
 * "solid" and the export names are all settings stored in the room file.
 * What is fixed is the object set (char / rect / pattern / zigzag), 8x8 cells and
 * the Spectrum attribute model (INK, PAPER, BRIGHT, FLASH).
 */
#ifndef ROOMS_H
#define ROOMS_H
#include <stdint.h>

#define MAX_SW      96          /* screen size limits, cells */
#define MAX_SH      72
#define MAX_ROOMS   256
#define MAX_OBJ     255
#define MAX_HDR     8
#define MAX_BG      16          /* background types with their own backdrop */
#define MAX_CUST    16          /* custom object types */

/* object types. 3-254 are custom objects declared in the room file with
   "object TYPE NAME W H [solid] [number]": stored as X Y TYPE CODE ATTR
   and drawn as a W x H figure of the consecutive tiles CODE, CODE+1...
   (CJMC: "object 3 crate 2 2 solid", the pushable crate), or, with
   "number", as a W x H box showing CODE and ATTR (CJMC: "object 4 baddie
   3 2 number": baddie type, flags) */
enum { OBJ_CHAR = 0, OBJ_RECT = 1, OBJ_PATTERN = 2, OBJ_CUSTOM = 3, OBJ_ZIGZAG = 255 };
/* zigzag: a rectangle filled with the tiles CODE and CODE + 1 taking
   turns, each row starting one cell further on (CODE CODE+1 CODE...,
   then CODE+1 CODE CODE+1...): staggered bricks, checkerboards. Stored
   X Y 2 CODE ATTR X2 Y2 in the compact encoding (where 2 is free) and
   X Y 254 CODE ATTR X2 Y2 in the plain one (254 is reserved for it). */
#define OBJ_ZIGZAG_PLAIN 254
#define IS_CUST(t)  ((t) >= OBJ_CUSTOM && (t) != OBJ_ZIGZAG)
/* rect-like objects: X2 Y2 stored, resizable */
#define IS_AREA(t)  ((t) == OBJ_RECT || (t) == OBJ_PATTERN || (t) == OBJ_ZIGZAG)

typedef struct {
    int type;                    /* 3-253 (254: the plain zigzag) */
    char name[24];
    int w, h;                    /* size in cells */
    int solid;                   /* shown as solid whatever the attribute */
    int number;                  /* drawn as a box with CODE and ATTR as numbers
                                    (two parameters, e.g. a baddie's type and
                                    flags), not as tiles; never solid */
} CustObj;
#define OBJ_END 0xFF

typedef struct {
    uint8_t type, x, y, code, attr, x2, y2, w, h;   /* x2..h unused for some types */
} Obj;

typedef struct {
    char name[64];
    uint8_t hdr[MAX_HDR];
    int n;
    Obj o[MAX_OBJ];
} Room;

typedef struct {
    int nrooms;
    Room rooms[MAX_ROOMS];
    int sw, sh;                  /* screen, cells */
    int ax, ay, aw, ah;          /* editable area, cells */
    int hdr_n;                   /* header bytes per room (0-8) */
    int solid_bit;               /* attribute bit that means obstacle, -1 none */
    char export_name[32];        /* C/asm names: NAME_data, NAME_off */
    char tileset[512];           /* paths as written in the file (relative to it) */
    char background[512];        /* backdrop for every background type... */
    char bg_type[MAX_BG][512];   /* ...unless that type (header byte 0) has its own.
                                    %1..%7 in a path: that header byte, in decimal */
    int ncust;
    CustObj cust[MAX_CUST];
    int compact;                 /* "encoding compact" (see room_encode) */
    int bg_params[MAX_BG];       /* header bytes after the type, per background
                                    type; -1 = header_bytes - 1 (all of them) */
} Project;

/* the tileset: any PNG with 8x8 tiles, numbered row by row (up to 256) */
typedef struct {
    uint8_t bits[256][8];        /* 1 = ink */
    uint8_t ink[256];            /* colour of the tile in the PNG: INK | BRIGHT<<6 */
    uint8_t used[256];           /* has any ink */
    int count;                   /* tiles in the PNG */
    int loaded;
} Tileset;

extern const uint32_t zx_pal[16];

/* the Spectrum colour nearest to c (backdrops are shown in real colours) */
uint32_t zx_nearest(uint32_t c);
void proj_init(Project *p);
void room_init(Room *r, int index);
int  proj_load(Project *p, const char *path, char *err, int errlen);   /* 0 = ok */
int  proj_save(const Project *p, const char *path);                    /* 0 = ok */
/* Encodings.
   plain:   header_bytes header bytes, then X Y TYPE CODE ATTR (+ X2 Y2 for
            rect, + X2 Y2 W H for pattern), then 0xFF.
   compact: the header is the type byte plus bg_params [type] bytes (all
            header_bytes if not set); objects: char X Y 0 CODE ATTR, rect
            X Y 1 CODE ATTR X2 Y2, custom X Y TYPE CODE ATTR (TYPE 3-63),
            figure (a pattern holding one copy) X Y 0x80|(W-1)<<3|(H-1)
            CODE ATTR, pattern X Y 0x40|(W-1)<<3|(H-1) CODE ATTR X2 Y2
            (W, H 1-8), zigzag X Y 2 CODE ATTR X2 Y2; then 0xFF.
   plain zigzag: X Y 254 CODE ATTR X2 Y2. */
int  room_bytes(const Project *p, const Room *r);                      /* exported size */
int  room_encode(const Project *p, const Room *r, uint8_t *out);        /* returns size */
/* a problem that makes the rooms impossible to encode, or NULL */
const char *proj_check(const Project *p, int *room, int *obj);
int  proj_export_c(const Project *p, const char *path);
int  proj_export_asm(const Project *p, const char *path);
int  room_export_bin(const Project *p, const Room *r, const char *path);

int  tileset_load(Tileset *t, const char *path);                       /* 0 = ok */
/* resolve a path written in the project, relative to the project file */
void proj_path(const char *projfile, const char *rel, char *out, int outlen);
/* the backdrop path (as written, %N replaced) for a room: its type's, or
   the default. fallback: the default one */
const char *proj_backdrop(const Project *p, const Room *r);
const char *proj_backdrop_default(const Project *p);
/* custom object definition for a type, or NULL */
const CustObj *proj_cust(const Project *p, int type);
/* the name of an object's type: char, rect, pattern or the custom name */
const char *obj_name(const Project *p, const Obj *o);
/* loads a backdrop into out (sw*8 x sh*8), snapped to Spectrum colours */
int  backdrop_load(const Project *p, const char *full, uint32_t *out);

/* draw room r on out (sw*8 x sh*8 pixels). bg: backdrop or NULL (black).
   coll: if not NULL, sw*sh bytes, 1 where an object with the solid bit
   makes a cell solid. upto: draw only objects 0..upto-1 (-1 = all). */
void room_render(const Project *p, const Room *r, const Tileset *t, const uint32_t *bg,
                 uint32_t *out, uint8_t *coll, int upto);
/* which object is on top at cell (cx, cy)? -1 = none */
int  room_hit(const Room *r, int cx, int cy);
/* the tile and attribute an object puts in cell (cx, cy) (which must be inside it) */
void obj_cell(const Obj *o, int cx, int cy, int *code, int *attr);
void obj_normalize(Obj *o);        /* custom objects need w, h already set */

#endif
