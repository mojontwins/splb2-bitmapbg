/* Beeptune - songs: the data, the files and the sound (an exact model of
   the Z80 player in ../player/beeptune.h) */
#ifndef SONG_H
#define SONG_H
#include <stdint.h>
#include <stddef.h>

#define MAX_PAT    64          /* patterns in a song                          */
#define MAX_ROWS   128         /* rows in a pattern                           */
#define MAX_ORD    255         /* entries in the order list                   */
#define NOTES      84          /* C-1 .. B-7                                  */

/* note codes, as in the Z80 data */
#define N_HOLD     0           /* keep playing what was playing               */
#define N_OFF      1           /* silence                                     */
#define N_FIRST    2           /* C-1; note n (0 = C-1) is N_FIRST + n        */

/* the Z80 player's timing */
#define CPU_HZ     3500000
#define TURN_T     130         /* T-states per turn of the player's loop      */
#define TURN_A_T   64          /* ...of which channel A is on the speaker     */
#define FRAME_TURNS 538        /* turns per 50 Hz frame (bt_play)             */
#define FRAME_GAP_T 344        /* bt_play's work between frames (key check, setup;
                                  measured in an emulator), speaker unchanged */

typedef struct {
    int rows;
    uint8_t n[MAX_ROWS][2];    /* [row][channel]                              */
} Pattern;

typedef struct {
    int speed;                 /* frames per row                              */
    int loopto;                /* order entry to go back to                   */
    int nord;
    uint8_t ord[MAX_ORD];
    int npat;
    Pattern pat[MAX_PAT];
} Song;

void song_init(Song *s);                       /* one empty pattern            */
void pattern_clear(Pattern *p, int rows);

/* the binary format of the player (see beeptune.h). encode returns the size */
size_t song_encode(const Song *s, uint8_t *out, size_t max);
int    song_decode(Song *s, const uint8_t *b, size_t n, char *msg, size_t msglen);

int song_load(Song *s, const char *path, char *msg, size_t msglen);   /* .bts / .txt */
int song_save(const Song *s, const char *path);                       /* .bts        */
int song_export_asm(const Song *s, const char *path, const char *label);
int song_export_c(const Song *s, const char *path, const char *label);
int song_export_txt(const Song *s, const char *path);
int song_import_txt(Song *s, const char *text, char *msg, size_t msglen);

const char *note_name(int code, char out[4]);   /* "C#4", "---" hold, "===" off */
uint16_t note_step(int code);                   /* the player's step (0 off)    */

/* Sound. Renders like the Z80 does, at `rate` Hz, 16-bit mono.
   from_ord: order entry to start at; only_pat >= 0: play only that pattern
   (repeatedly if loop). loop: at the end go back to the loop point and
   go on, until max_seconds. Rows of the result are reported in *rowpos (sample
   where each played row starts) with their order entry / row, so the
   editor can follow the playback. Returns the number of samples. */
typedef struct { size_t sample; int ord, pat, row; } RowPos;
size_t song_render(const Song *s, int from_ord, int only_pat, int loop, int max_seconds,
                   int rate, int16_t **out, RowPos **rowpos, int *nrows);
int    wav_write(const char *path, const int16_t *smp, size_t n, int rate);

#endif
