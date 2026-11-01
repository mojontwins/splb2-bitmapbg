/*
 * PHANTOMASA - C + inline asm port for z88dk (classic, sccz80) +
 * splib2-bitmapbg, for the ZX Spectrum 128K, in ONE load.
 *
 * Original: na_th_an / The Mojon Twins + Black Hole, (C) WOPR2K & CEZGS
 * 2006. Two compiled-BASIC tapes for the 48K: part 1 (intro + stage 1,
 * "moving blocks") and part 2 (stage 2 "Jan-Ken-Do", stage 3 "the snotty
 * maze" and the ending), joined by an access code. The listings are in
 * orig/. Graphics and music were extracted from the original tapes by
 * tools/extract.py.
 *
 * This port:
 *   - pixel-by-pixel masked sprites (player, pushed box, moving
 *     platform) over the original bitmap screens, 25 fps, with splib2's
 *     BITMAPBG shadow screen;
 *   - every screen ZX0-packed, kept in the 128K's extra RAM banks and
 *     depacked straight to the screen when needed: the whole game (both
 *     tapes) loads at once (see README.md for why a 48K can't);
 *   - the original beeper music player and songs, at their original
 *     address ($F600), called note by note as the BASIC did.
 *
 * ------------------------------------------------------------------------
 * MEMORY MAP (128K; bank 0 normally paged at $C000)
 * ------------------------------------------------------------------------
 *   $5E88 - ~$B800  this program (org 24200)
 *   ...   - $BFFF   stack (grows down from $C000)
 *   $C000 - $C1D0   splib2 block memory (31 x 15)             } bank 0
 *   $C800 - $DFFF   SHADOW screen (BGSHADOW = $C8)            }
 *   $E000 - $E100   IM2 vector table                          }
 *   $E1E1 -         splib2's generic ISR                      }
 *   $E200 - $EFFF   splib2 rotation tables (SProtatetbl $E000)}
 *   $F000 - $F2FF   the font                                  }
 *   $F300 - $F5FF   sprite frames                             }
 *   $F600 - $FC57   the original music player + songs         }
 *   banks 1, 3, 4   the packed screens (build/bankN.bin)
 * The library is built for this layout as splib2hi.lib (build.sh does
 * it: ../../build.sh variant splib2hi BGSHADOW=$C8 SProtatetbl=$E000
 * IM2TABLE=$E000). The music player has to stay at $F600: it is the
 * original binary and its addresses are fixed. That is why the splib2
 * tables moved down to $E000.
 *
 * Paging: while another bank is at $C000, nothing of bank 0 is
 * there: not the stack, not the IM2 table. So show_screen() runs with
 * interrupts off and on a little stack below $C000.
 *
 * ------------------------------------------------------------------------
 * HOW TO KEEP DEVELOPING IT
 * ------------------------------------------------------------------------
 *   rounds.txt            the rounds (boxes, platforms, exits, snot...)
 *   gfx/screens/*.png     every screen; collisions come from their colours
 *   gfx/sprites.png       player, box, platform
 *   gfx/font.png          the font (the lower case letters are graphics:
 *                         box e f g h, portal i j, exits l m n o,
 *                         platform p q, sparkle z)
 *   this file             the game; tunable numbers are #defines below
 * build.sh converts everything and makes phantomasa.tap.
 *
 * ------------------------------------------------------------------------
 * CONVENTIONS (as in the other examples)
 * ------------------------------------------------------------------------
 *   - no local variables, few parameters: globals instead. The table
 *     "WHO CLOBBERS WHAT" says which globals each function overwrites.
 *   - sprite positions in pixels; the original worked in characters, so
 *     the round data (rounds.txt) is in characters: px = 8 * column.
 *   - never add a comparison to a number: in sccz80 a true comparison is
 *     not 1. Use bit arithmetic or an if.
 */

#pragma output STACKPTR=49152

#define SP_BGSHADOW 0xC800
#include <spritepack.h>

#define NUMBLOCKS   31                  /* 3 sprites x 10 + the ISR hook  */
#define AD_FREE     0xC000
#define ISR_ADDR    0xe1e1              /* IM2TABLE is $E000              */

#include "assets.h"

/* ------------------------------------------------------------------ */
/* TUNING                                                              */
/* ------------------------------------------------------------------ */

/* pixels per frame at 25 fps. The original moved a character per main
   loop; one loop is about TICK_FRAMES frames here.                    */
#define PLAYER_SPEED      2
#define FALL_SPEED        2
#define PUSH_SPEED        1         /* pushing a box: slower, as the
                                       original moved box and player on
                                       alternate loops                  */
#define PLATFORM_SPEED    1         /* half the player's, as original   */
#define TICK_FRAMES       4
#define START_LIVES       10
#define PORTAL_FLASH      8         /* frames between portal colour swaps */

/* jump: pixels risen on each frame. 48 px = the original's 6 characters,
   fast at first and slower at the top. JUMP_STEPS = its length.       */
#define JUMP_STEPS        18
unsigned char jump_tab [] = { 4, 4, 4, 4, 4, 4, 3, 3, 3, 3, 2, 2, 2, 2, 1, 1, 1, 1 };

/* collision values, in the display list's tile byte (BITMAPBG doesn't
   use it). From the screen attributes: 7 = free, 1 = deadly, else solid. */
#define C_FREE            0
#define C_SOLID           1
#define C_BOX             2
#define C_DEADLY          3

/* songs of the original player, in the order of its entry points */
#define SONG_INTRO        0         /* also the outro                   */
#define SONG_TITLE        1
#define SONG_NEWLEVEL     2
#define SONG_GAMEOVER     3
#define SONG_LIFELOST     4
#define SONG_FINAL        5

/* sprite frames (gfx/sprites.png) */
#define F_LEFT            0         /* +1: the other foot / jumping     */
#define F_RIGHT           2
#define F_BOX             4
#define F_PLATFORM        5

/* ------------------------------------------------------------------ */
/* splib2 housekeeping                                                 */
/* ------------------------------------------------------------------ */

void *my_malloc (uint bytes) { return sp_BlockAlloc (0); }
void *u_malloc = my_malloc;
void *u_free = sp_FreeBlock;

struct sp_Rect clip_full = {0, 0, 24, 32};
struct sp_Rect rect;

struct sp_SS *spr [3];               /* 0 player, 1 box, 2 platform       */
unsigned char *spr_cur [3];

unsigned char isrc;

extern void isr (void);
#asm
	defw 0
._isr
	ld  hl, _isrc
	inc (hl)
	ret
#endasm

/* ------------------------------------------------------------------ */
/* GLOBALS                                                             */
/*                                                                     */
/* WHO CLOBBERS WHAT                                                   */
/*   put_glyph ........ gp (reads t_row t_col t_attr t_ch to_shadow)   */
/*   print_str ........ sp_s, t_col + put_glyph                        */
/*   print_centered ... t_col t_row(+1) + print_str                    */
/*   print_num ........ t_num t_ch t_col                               */
/*   set_attr_rect .... scr_p ga gb (reads t_row t_col t_h t_w t_attr) */
/*   clear_rect ....... ga gb cr_r cr_c t_ch + put_glyph               */
/*   clear_text ....... ct_r + clear_rect, music_note                  */
/*   show_screen ...... (reads ss_n), registers only                   */
/*   beep ............. bp_*                                           */
/*   music_* .......... registers only                                 */
/*   typewriter ....... tw_x + put_glyph, music_note                   */
/*   coll_set/get ..... gp (reads t_row t_col, gv)                     */
/*   box_free ......... (reads bx by), registers                       */
/*   spr_put .......... gp (reads s_n s_f s_x s_y)                     */
/*   round code ....... gi gj gk mk + everything above                 */
/*   scenes (intro, title, jkd...) gi gj gk + everything above         */
/* ------------------------------------------------------------------ */

unsigned char gi, gj, gk, gv, ga, gb, mk;
unsigned int  gw;
unsigned char *gp, *gq;

unsigned char t_row, t_col, t_attr, t_ch, t_num, t_h, t_w, to_shadow;
unsigned char *sp_s;
unsigned char s_n, s_f, s_x, s_y;
unsigned char bx, by;
unsigned char *dlist;
unsigned char *font;                     /* FONT_ADDR, in bank 0 at $F000   */
unsigned char *sprite_frames;            /* SPRITES_ADDR, at $F310          */

/* ------------------------------------------------------------------ */
/* LOW LEVEL                                                           */
/* ------------------------------------------------------------------ */

unsigned char __FASTCALL__ inport (unsigned int port) {
	#asm
		ld  b, h
		ld  c, l
		in  a, (c)
		ld  l, a
		ld  h, 0
	#endasm
}

void wait_frame (void) {
	while (isrc < 2);
	isrc = 0;
}

unsigned char wf_n;
void wait_frames (void) {             /* wf_n frames (25 fps) */
	while (wf_n --) { isrc = 0; while (isrc < 2); }
}

unsigned int rnd_seed = 0x5a3c;
unsigned char rnd (void) {
	#asm
		ld  hl, (_rnd_seed)
		ld  a, h
		rra
		ld  a, l
		rra
		xor h
		ld  h, a
		ld  a, l
		rra
		ld  a, h
		rra
		xor l
		ld  l, a
		xor h
		ld  h, a
		ld  (_rnd_seed), hl
		ld  h, 0
	#endasm
}

unsigned char any_key (void) {
	return ((inport (0x00FE) & 0x1F) != 0x1F);
}

/* wait until no key is pressed, then for a key (the original's 1370) */
void wait_key (void) {
	while (any_key ());
	while (!any_key ()) rnd ();
}

/* the keys of the original: O P Q (+ R to give up the round) */
unsigned char k_left, k_right, k_up, k_restart, k_fire;
void read_keys (void) {
	gv = inport (0xDFFE);                /* Y U I O P              */
	k_right = !(gv & 1);
	k_left = !(gv & 2);
	if (k_right && k_left) { k_right = k_left = 0; }
	k_up = !(inport (0xFBFE) & 1);       /* Q                      */
	k_restart = !(inport (0xFBFE) & 8);  /* R                      */
	k_fire = 0;                          /* SPACE or ENTER */
	if (!(inport (0x7FFE) & 1)) k_fire = 1;
	if (!(inport (0xBFFE) & 1)) k_fire = 1;
}

/* ------------------------------------------------------------------ */
/* SOUND                                                               */
/* ------------------------------------------------------------------ */

/* The original player (gfx/music.bin, loaded at $F600): song n starts
   at $F701 + 5n, one note is played by $FC14 (it uses IX).          */
void __FASTCALL__ music_init (unsigned char song) {
	#asm
		ld  a, l
		ld  hl, $F701
		ld  de, 5
		or  a
	.mi_l
		jr  z, mi_go
		add hl, de
		dec a
		jr  mi_l
	.mi_go
		push ix
		ld  de, mi_ret
		push de
		jp  (hl)
	.mi_ret
		pop ix
	#endasm
}

void music_note (void) {
	#asm
		push ix
		call $FC14
		pop ix
	#endasm
}

/* BEEP bp_ms / 1000, bp_note (semitones from middle C, BEEP_MIN..63):
   the table (tools/conv.py) has the delay for half a period and the
   frequency / 10.                                                     */
signed char bp_note;
unsigned int bp_ms, bp_count, bp_halves;
void beep (void) {
	gp = beep_tab + ((bp_note - BEEP_MIN) << 2);
	bp_count = gp [0] | (gp [1] << 8);
	bp_halves = ((gp [2] | (gp [3] << 8)) * bp_ms) / 50;   /* 2 x Hz/10 x ms / 100 */
	if (bp_halves == 0) bp_halves = 1;
	#asm
		di
		ld  hl, (_bp_halves)
		ld  de, (_bp_count)
		xor a                       ; speaker bit / border black
	.bp_half
		xor 16
		out ($FE), a
		push af
		ld  b, d
		ld  c, e
	.bp_wait
		dec bc
		ld  a, b
		or  c
		jr  nz, bp_wait
		pop af
		dec hl
		ld  b, a
		ld  a, h
		or  l
		ld  a, b
		jr  nz, bp_half
		ei
	#endasm
}

/* ------------------------------------------------------------------ */
/* SCREENS AND 128K PAGING                                             */
/* ------------------------------------------------------------------ */

unsigned char ss_n;                     /* screen to show (SCR_*)      */
unsigned char ss_bank;
unsigned int  ss_src;
unsigned int  saved_sp;
unsigned char tmp_stack [48];           /* below $C000: safe to use     */
                                        /* while another bank is paged  */

/* depack screen ss_n (from its RAM bank) to the real screen. The bitmap
   is written before the attributes, so if the screen was black it
   appears in one go. */
void show_screen (void) {
	ss_bank = scr_bank [ss_n];
	ss_src = scr_addr [ss_n];
	#asm
		LIB SPZX0Depack
		di
		ld  (_saved_sp), sp
		ld  sp, _tmp_stack + 48
		ld  a, (_ss_bank)
		or  $10                     ; keep the 48K BASIC ROM paged
		ld  bc, $7FFD
		out (c), a
		ld  hl, (_ss_src)
		ld  de, $4000
		call SPZX0Depack
		ld  a, $10                  ; bank 0 back
		ld  bc, $7FFD
		out (c), a
		ld  sp, (_saved_sp)
		ei
	#endasm
}

/* whole screen black */
void cls (void) {
	#asm
		ld  hl, $4000
		ld  de, $4001
		ld  bc, 6911
		ld  (hl), 0
		ldir
	#endasm
}

/* ------------------------------------------------------------------ */
/* TEXT. put_glyph draws character t_ch of the game font at (t_row,     */
/* t_col) with t_attr: on the real screen, or (to_shadow = 1) in the    */
/* splib2 background, which is redrawn at the next sp_UpdateNow.        */
/* ------------------------------------------------------------------ */

unsigned char *scr_p;
void put_glyph (void) {
	gp = font + ((t_ch - 32) << 3);
	if (to_shadow) { sp_BgPutTile (t_row, t_col, t_attr, gp); return; }
	scr_p = (unsigned char *) (0x4000 + ((t_row & 0x18) << 8) + ((t_row & 7) << 5) + t_col);
	for (gk = 0; gk < 8; ++ gk) { *scr_p = gp [gk]; scr_p += 256; }
	scr_p = (unsigned char *) (0x5800 + (t_row << 5) + t_col);
	*scr_p = t_attr;
}

void print_str (void) {
	while (*sp_s) { t_ch = *sp_s ++; put_glyph (); ++ t_col; }
}

/* the original's "Imprime frase centrada": at row t_row, then next row */
unsigned char str_len;
void print_centered (void) {
	gp = sp_s; str_len = 0;
	while (*gp ++) ++ str_len;
	t_col = 16 - (str_len >> 1);
	print_str ();
	++ t_row;
}

void print_num (void) {
	if (t_num >= 10) { t_ch = '0' + t_num / 10; put_glyph (); ++ t_col; }
	t_ch = '0' + t_num % 10; put_glyph (); ++ t_col;
}

/* attributes of a rectangle on the real screen (the original's PRINT
   OVER 1 with spaces: colour changes, pixels stay)                    */
void set_attr_rect (void) {
	for (ga = 0; ga < t_h; ++ ga) {
		scr_p = (unsigned char *) (0x5800 + ((t_row + ga) << 5) + t_col);
		for (gb = 0; gb < t_w; ++ gb) *scr_p ++ = t_attr;
	}
}

/* blank cells on the real screen: rows t_row.., t_h rows, t_w columns */
unsigned char cr_r, cr_c;
void clear_rect (void) {
	t_ch = ' ';
	cr_r = t_row; cr_c = t_col;
	for (ga = 0; ga < t_h; ++ ga)
		for (gb = 0; gb < t_w; ++ gb) {
			t_row = cr_r + ga; t_col = cr_c + gb;
			put_glyph ();
		}
	t_row = cr_r; t_col = cr_c;
}

/* ------------------------------------------------------------------ */
/* MUSICAL TEXT (the intro / outro "typewriter")                       */
/* ------------------------------------------------------------------ */

unsigned char tw_x, skipped;

/* The original's "Imprime frase con musica": prints sp_s from column 2
   of row t_row, wrapping every 28 characters two rows down, playing a
   note per character. A key press stops it (skipped = 1).            */
void typewriter (void) {
	t_col = 2; tw_x = 0;
	while (*sp_s) {
		t_ch = *sp_s ++; put_glyph (); ++ t_col;
		if (++ tw_x == 28) { tw_x = 0; t_row += 2; t_col = 2; }
		music_note ();
		if (any_key ()) { skipped = 1; return; }
	}
}

/* "Limpia lineas texto con musica": rows 16-21, a note per row */
unsigned char ct_r;
void clear_text (void) {
	for (ct_r = 16; ct_r < 22; ++ ct_r) {
		t_row = ct_r; t_col = 0; t_h = 1; t_w = 32; t_attr = 7; clear_rect ();
		music_note ();
	}
	t_row = 16;
}

/* notes (the "pause" after a sentence) */
unsigned char notes_n;
void play_notes (void) {
	while (notes_n --) {
		music_note ();
		if (any_key ()) { skipped = 1; return; }
	}
}

/* ------------------------------------------------------------------ */
/* SPRITES                                                             */
/* ------------------------------------------------------------------ */

void spr_put (void) {
	gp = sprite_frames + s_f * SPR_FRAME_SIZE;
	sp_MoveSprAbs (spr [s_n], &clip_full, gp - spr_cur [s_n], s_y >> 3, s_x >> 3, s_x & 7, s_y & 7);
	spr_cur [s_n] = gp;
}

void spr_hide (void) {
	sp_MoveSprAbs (spr [s_n], &clip_full, 0, 0, 34, 0, 0);
}

void spr_hide_all (void) {
	s_n = 0; spr_hide (); s_n = 1; spr_hide (); s_n = 2; spr_hide ();
}

extern unsigned int null_ptr;
#asm
	LIB SPNullSprPtr
._null_ptr
	defw SPNullSprPtr
#endasm

void spr_create (void) {                 /* sprite s_n on plane s_f */
	spr [s_n] = sp_CreateSpr (sp_MASK_SPRITE, 3, sprite_frames, s_f);
	sp_AddColSpr (spr [s_n], sprite_frames + 48);
	sp_AddColSpr (spr [s_n], sprite_frames + 48);
	gp = (unsigned char *) spr [s_n];
	gw = (gp [6] << 8) | gp [7];
	for (gi = 0; gi < 9; gi ++) {
		gp = (unsigned char *) gw;
		if (gi >= 6) { gp [7] = null_ptr & 255; gp [8] = null_ptr >> 8; }
		gw = (gp [0] << 8) | gp [1];
	}
	spr_cur [s_n] = sprite_frames;
}

/* ------------------------------------------------------------------ */
/* COLLISION MAP (display list tile byte, one per cell)                */
/* ------------------------------------------------------------------ */

unsigned char coll_get (void) {          /* cell (t_row, t_col) */
	return dlist [(((t_row << 5) + t_col) << 2) + 1];
}
void coll_set (void) {                   /* cell (t_row, t_col) = gv */
	dlist [(((t_row << 5) + t_col) << 2) + 1] = gv;
}

/* is the 16x16 box at (bx, by) free of anything solid? Cells first
   (asm), then the moving platform. by may be up to 176.              */
unsigned char plat_on, plat_x, plat_y;
unsigned char cells_free (void) {
	#asm
		ld  a, (_by)
		ld  c, a
		and 7
		ld  b, 2
		jr  z, cf_r2
		inc b
	.cf_r2
		ld  a, c
		and $F8
		ld  l, a
		ld  h, 0
		add hl, hl
		add hl, hl
		add hl, hl
		add hl, hl
		ld  de, (_dlist)
		add hl, de
		inc hl
		ld  a, (_bx)
		ld  c, a
		and $F8
		rrca
		ld  e, a
		ld  d, 0
		add hl, de
		ld  a, c
		and 7
		ld  c, 2
		jr  z, cf_c2
		inc c
	.cf_c2
	.cf_row
		push hl
		push bc
	.cf_col
		ld  a, (hl)
		or  a
		jr  nz, cf_hit
		inc hl
		inc hl
		inc hl
		inc hl
		dec c
		jr  nz, cf_col
		pop bc
		pop hl
		ld  de, 128
		add hl, de
		djnz cf_row
		ld  hl, 1
		ret
	.cf_hit
		pop bc
		pop hl
		ld  hl, 0
	#endasm
}

unsigned char box_free (void) {
	if (!cells_free ()) return 0;
	if (plat_on && bx < plat_x + 16 && bx + 16 > plat_x && by < plat_y + 8 && by + 16 > plat_y) return 0;
	return 1;
}

/* ------------------------------------------------------------------ */
/* THE ROUND BEING PLAYED                                              */
/* ------------------------------------------------------------------ */

unsigned char round_n;                   /* index in rounds.txt order    */
unsigned char *rd;                       /* its record (conv.py, ROUND)  */
unsigned char stage, round_num;
unsigned char lives;

unsigned char px, py, pfacing, pjump, pjc, pmoving;
unsigned char exit_w, exit_a;
unsigned char died, done;

/* snot (stage 3) */
signed char sn_row;                      /* last row covered (<0: none)  */
unsigned char sn_speed, sn_timer;

/* platform */
unsigned char plat_min, plat_max, plat_dir;  /* dir 1 right, 255 left */

/* portal */
unsigned char portal_y, portal_x, portal_t, portal_attr;

/* gate (stage 1, round 9) */
unsigned char gate;

/* The boxes: positions (top-left character) from rounds.txt, updated
   when they are pushed. Only these are boxes: red things in a picture
   are scenery (solid), whatever they look like.                       */
#define MAX_BOXES 24
unsigned char nboxes;
unsigned char box_y [MAX_BOXES], box_x [MAX_BOXES];
unsigned char box_fixed [MAX_BOXES];    /* covered by something: can't move */
unsigned char push_i;                   /* index of the box being pushed  */

/* the box being pushed */
unsigned char box_parity;               /* row parity of the boxes' tops */
unsigned char push_on, push_dir, push_px, push_py, push_left, push_col, push_row;

/* boxes are drawn as tiles; box_y/box_x say where each one is */
#define BOX_ATTR      2
#define PLAT_ATTR     6
#define PORTAL_ATTR   0x2A               /* PAPER 5, INK 2 (FLASH is done */
#define PORTAL_ATTR2  0x15               /* by hand: bit 7 means "no      */
                                         /* sprites here" in this splib2) */
#define HUD_ATTR      4

/* a box (2x2: font e f / g h) at character (t_row, t_col) */
void draw_box (void) {
	t_attr = BOX_ATTR;
	t_ch = 'e'; put_glyph (); ++ t_col;
	t_ch = 'f'; put_glyph (); ++ t_row;
	t_ch = 'h'; put_glyph (); -- t_col;
	t_ch = 'g'; put_glyph (); -- t_row;
}

void hud (void) {
	to_shadow = 1;
	t_row = 23; t_col = 24; t_attr = HUD_ATTR; sp_s = "LIVES:"; print_str ();
	t_num = lives; print_num ();
	/* one digit: blank the old second one. Never write past column 31:
	   in the shadow that would be past the end of the display list.  */
	if (t_col < 32) { t_ch = ' '; put_glyph (); }
}

/* everything at the start of a round: screen, decorations, collision
   map, sprites                                                         */
void setup_round (void) {
	rd = rounds + round_offs [round_n];
	stage = rd [0]; round_num = rd [1];

	/* 1. the picture, straight to the real screen, plus what the BASIC
	      printed over it: door, tiles, portal, boxes                    */
	spr_hide_all ();
	sp_UpdateNow ();
	cls ();
	ss_n = rd [2]; show_screen ();
	to_shadow = 0;

	if (rd [17] != 255) {                       /* door: stage 3 exit */
		t_row = rd [17]; t_col = 22; t_h = 2; t_w = 10; t_attr = 7; clear_rect ();
		t_row = rd [17] - 1; t_col = 22; t_attr = 0x0D; sp_s = "lmlmlmlmlm"; print_str ();
		t_row = rd [17] + 2; t_col = 22; sp_s = "nonononono"; print_str ();
	}

	gp = rd + 19 + (rd [18] << 1);              /* tile strings */
	gi = *gp ++;
	while (gi --) {
		t_row = gp [0]; t_col = gp [1]; t_attr = gp [2]; gj = gp [3]; gp += 4;
		while (gj --) { t_ch = *gp ++; gq = gp; put_glyph (); gp = gq; ++ t_col; }
	}

	/* boxes, then the portal over them (in that order, as the BASIC) */
	nboxes = rd [18];
	for (gi = 0; gi < nboxes; ++ gi) {
		box_y [gi] = t_row = rd [19 + (gi << 1)];
		box_x [gi] = t_col = rd [20 + (gi << 1)];
		draw_box ();
	}

	portal_y = rd [14];
	if (portal_y != 255) {
		portal_x = rd [15];
		t_row = portal_y; t_col = portal_x; t_attr = PORTAL_ATTR;
		t_ch = 'i'; put_glyph (); ++ t_col; t_ch = 'j'; put_glyph ();
	}
	portal_t = PORTAL_FLASH; portal_attr = PORTAL_ATTR;

	/* 2. screen -> splib2 background, then the collision map from the
	      attributes                                                       */
	sp_BgCapture ();
	for (gw = 0; gw < 768; ++ gw) {
		gv = dlist [gw << 2];
		if (gv == 7) gb = C_FREE;
		else if (gv == 1) gb = C_DEADLY;
		else gb = C_SOLID;
		dlist [(gw << 2) + 1] = gb;
	}
	/* box cells, from the box list. A box with something printed over
	   it (the portal in stage 1 round 2) can't move, as in the original:
	   its cells stay just solid.                                        */
	for (gi = 0; gi < nboxes; ++ gi) {
		box_fixed [gi] = 0;
		t_row = box_y [gi]; t_col = box_x [gi];
		for (ga = 0; ga < 2; ++ ga)
			for (gb = 0; gb < 2; ++ gb)
				if (dlist [(((t_row + ga) << 5) + t_col + gb) << 2] != BOX_ATTR) box_fixed [gi] = 1;
		if (box_fixed [gi]) continue;
		gv = C_BOX;
		coll_set (); ++ t_col; coll_set (); ++ t_row; coll_set (); -- t_col; coll_set ();
	}

	/* 3. state */
	px = rd [4] << 3; py = (rd [3] - 1) << 3;
	pfacing = F_RIGHT; pjump = 0; pmoving = 0;
	exit_w = rd [5]; exit_a = rd [6];
	died = done = 0;
	sn_row = rd [7]; sn_speed = rd [8]; sn_timer = 0;
	plat_on = 0;
	if (rd [9] != 255) {
		plat_on = 1;
		plat_y = rd [9] << 3; plat_x = rd [10] << 3;
		plat_min = rd [11] << 3; plat_max = rd [12] << 3; plat_dir = rd [13];
	}
	gate = rd [16];
	push_on = 0;
	box_parity = 0;                         /* stage 1: even rows */
	if (stage == 3) box_parity = 1;          /* stage 3: odd rows  */
	if (rd [18]) box_parity = rd [19] & 1;   /* or as the first box */

	hud ();
	isrc = 0;
}

/* ---------------------------------------------------------------- */
/* pushing boxes. The player walks into a box with his rows exactly on */
/* the box's rows (the original tested the row parity), and          */
/* the box moves one character if the two cells beyond it are free.     */
/* While it moves, it's a sprite and both its old and new cells are     */
/* marked C_BOX; at the end it's painted as tiles again and the column  */
/* it left becomes empty (blank, free), as in the original.             */
/* ---------------------------------------------------------------- */

/* try to start pushing the box at character column push_col (the cell
   column the player walks into); push_dir 255 left, 1 right          */
void try_push (void) {
	if (push_on || (py & 7)) return;
	push_row = py >> 3;
	/* boxes sit on rows of one parity (even in stage 1, odd in stage 3:
	   the original's INT (W/2) test). A player standing between two
	   stacked boxes must not push the lower half of one and the upper
	   half of the other as if they were a box.                         */
	if ((push_row & 1) != box_parity) return;
	/* the box's left column, and the column it moves into */
	if (push_dir == 1) gb = push_col + 2;
	else { if (push_col < 2) return; gb = push_col - 2; push_col --; }
	if (gb > 31) return;
	/* is there a box (from the list) exactly there? */
	for (push_i = 0; push_i < nboxes; ++ push_i)
		if (box_y [push_i] == push_row && box_x [push_i] == push_col && !box_fixed [push_i]) break;
	if (push_i == nboxes) return;
	t_row = push_row; t_col = gb; if (coll_get () != C_FREE) return;
	++ t_row; if (coll_get () != C_FREE) return;
	/* platform in the way? */
	if (plat_on && plat_y >= (push_row << 3) && plat_y < (push_row << 3) + 16 &&
	    (gb << 3) < plat_x + 16 && (gb << 3) + 8 > plat_x) return;

	/* go: blank the box's tiles, mark the destination column busy */
	to_shadow = 1;
	t_row = push_row; t_col = push_col; t_h = 2; t_w = 2; t_attr = 7;
	t_ch = ' ';
	put_glyph (); ++ t_col; put_glyph (); ++ t_row; put_glyph (); -- t_col; put_glyph ();
	t_row = push_row; t_col = gb; gv = C_BOX; coll_set (); ++ t_row; coll_set ();
	push_px = push_col << 3; push_py = push_row << 3; push_left = 8;
	push_on = 1;
	bp_note = 15; bp_ms = 1; beep ();
}

/* one frame of box movement; at the end, tiles + collision update */
void move_push (void) {
	if (!push_on) return;
	if (push_dir == 1) push_px += PUSH_SPEED; else push_px -= PUSH_SPEED;
	push_left -= PUSH_SPEED;
	if (push_left) return;
	/* arrived: the column left behind becomes free */
	if (push_dir == 1) t_col = push_col; else t_col = push_col + 1;
	t_row = push_row; gv = C_FREE; coll_set (); ++ t_row; coll_set ();
	box_x [push_i] = t_col = push_px >> 3;
	t_row = push_row;
	to_shadow = 1; draw_box ();
	push_on = 0;
	s_n = 1; spr_hide ();
}

/* ------------------------------------------------------------------ */
/* the player                                                          */
/* ------------------------------------------------------------------ */

void move_player (void) {
	pmoving = 0;
	/* gravity */
	if (!pjump) {
		for (mk = 0; mk < FALL_SPEED; mk ++) {
			bx = px; by = py + 1;
			if (py >= 176 || !box_free ()) break;
			++ py;
		}
	}
	/* jump: from anything solid under the feet */
	if (k_up && !pjump) {
		bx = px; by = py + 1;
		if (!box_free ()) { pjump = 1; pjc = 0; }
	}
	if (pjump) {
		for (mk = jump_tab [pjc]; mk; -- mk) {
			if (py == 0) break;
			bx = px; by = py - 1;
			if (!box_free ()) break;     /* head bump: keep rising time */
			-- py;
		}
		if ((pjc & 3) == 0) { bp_note = 10 + (py >> 4); bp_ms = 1; beep (); }
		if (++ pjc == JUMP_STEPS) pjump = 0;
	}

	/* walking (and pushing: the box goes first, the player follows) */
	if (k_left) {
		pfacing = F_LEFT;
		for (mk = 0; mk < PLAYER_SPEED; mk ++) {
			if (px == 0) break;
			bx = px - 1; by = py;
			if (!box_free ()) {
				if (!push_on) { push_col = (px - 1) >> 3; push_dir = 255; try_push (); }
				break;
			}
			-- px; pmoving = 1;
		}
	} else if (k_right) {
		pfacing = F_RIGHT;
		for (mk = 0; mk < PLAYER_SPEED; mk ++) {
			if (px >= 240) break;
			bx = px + 1; by = py;
			if (!box_free ()) {
				if (!push_on) { push_col = (px + 16) >> 3; push_dir = 1; try_push (); }
				break;
			}
			++ px; pmoving = 1;
		}
	}
}

/* the platform moves, and carries the player standing on it */
unsigned char on_plat;
void move_platform (void) {
	if (!plat_on) return;
	on_plat = 0;
	if (py + 16 == plat_y && px + 16 > plat_x && px < plat_x + 16) on_plat = 1;
	gb = plat_x;                                  /* where it was */
	if (plat_dir == 1) { plat_x += PLATFORM_SPEED; if (plat_x >= plat_max) plat_dir = 255; }
	else { plat_x -= PLATFORM_SPEED; if (plat_x <= plat_min) plat_dir = 1; }
	if (on_plat) {
		bx = px + plat_x - gb;                    /* the same step (mod 256) */
		by = py;
		if (bx <= 240 && cells_free ()) px = bx;
	}
}

/* ------------------------------------------------------------------ */
/* deaths and exits                                                    */
/* ------------------------------------------------------------------ */

/* standing (aligned) with every cell under the feet deadly */
void check_spikes (void) {
	if ((py & 7) || py >= 176) return;
	t_row = (py >> 3) + 2;
	for (t_col = px >> 3; t_col <= (px + 15) >> 3; ++ t_col)
		if (coll_get () != C_DEADLY) return;
	died = 1;
}

/* stage 3: the snot comes down one row every (sn_speed + 1) ticks */
void move_snot (void) {
	if (sn_speed == 255) return;
	if (++ sn_timer < (sn_speed + 1) * TICK_FRAMES) return;
	sn_timer = 0;
	++ sn_row;
	if (sn_row < 0 || sn_row > 23) return;
	/* PAPER 4, INK 8 (unchanged), OVER 1: the row turns green; it's solid */
	gp = dlist + (sn_row << 7);
	for (gi = 0; gi < 32; ++ gi) { gp [0] = (gp [0] & 0x47) | 0x20; gp [1] = C_SOLID; gp += 4; }
	rect.row_coord = sn_row; rect.col_coord = 0; rect.height = 1; rect.width = 32;
	sp_Invalidate (&rect, &clip_full);
	bp_note = 30; bp_ms = 3; beep ();
	/* touching the head: dead (the original's SN = W-1) */
	if ((unsigned char) sn_row >= (py >> 3)) died = 1;
}

/* round 9 of stage 1: the barrier at column 12 opens */
void check_gate (void) {
	if (!gate) return;
	if ((px + 4) >> 3 >= 25 && (py + 15) >> 3 <= 5) {
		to_shadow = 1; t_col = 12; t_attr = 7; t_ch = ' ';
		for (t_row = 0; t_row < 6; ++ t_row) {
			put_glyph (); gv = C_FREE; coll_set ();
			sp_UpdateNow ();
			for (gi = 0; gi < 6; ++ gi) { bp_note = 0; bp_ms = 20; beep (); }
		}
		gate = 0;
	}
}

void check_exit (void) {
	if (exit_w != 255 && ((py + 15) >> 3) != exit_w) return;
	if (((px + 4) >> 3) != exit_a) return;
	done = 1;
}

/* the death animation: sparkles flying up from the player, as the
   original (font 'z', red), a note of the "life lost" song per step */
unsigned char dn, dw, da;               /* step, player's W and A */
unsigned char saved [2 * 9];             /* two cells: 8 bytes + attribute */
unsigned char sv_r [2], sv_c [2], sv_n;

/* keep cell (t_row, t_col) of the background in saved slot sv_n */
void save_cell (void) {
	gq = (unsigned char *) sp_BgAddr (t_row, t_col);
	gp = saved + sv_n * 9;
	for (gk = 0; gk < 8; ++ gk) { gp [gk] = *gq; gq += 256; }
	gp [8] = dlist [((t_row << 5) + t_col) << 2];
	sv_r [sv_n] = t_row; sv_c [sv_n] = t_col;
}
/* and put it back */
void restore_cell (void) {
	if (sv_r [sv_n] == 255) return;
	gp = saved + sv_n * 9;
	sp_BgPutTile (sv_r [sv_n], sv_c [sv_n], gp [8], gp);
	sv_r [sv_n] = 255;
}

/* the death animation of the original: the player turns red and two
   sparkles (font 'z', red) fly up and away from him, one step per note
   of the "life lost" song. Each step puts the background back where
   the sparkles were (the original did it with OVER 1).                */
void death_animation (void) {
	music_init (SONG_LIFELOST);
	/* red player: ink 2 on the cells he covers */
	for (t_row = py >> 3; t_row <= (py + 15) >> 3; ++ t_row)
		for (t_col = px >> 3; t_col <= (px + 15) >> 3; ++ t_col) {
			gp = dlist + (((t_row << 5) + t_col) << 2);
			gp [0] = (gp [0] & 0xF8) | 2;
		}
	rect.row_coord = py >> 3; rect.col_coord = px >> 3; rect.height = 3; rect.width = 3;
	sp_Invalidate (&rect, &clip_full);
	to_shadow = 1; t_attr = 2; t_ch = 'z';
	sv_r [0] = sv_r [1] = 255;
	dw = (py + 15) >> 3;                   /* W */
	da = (px + 4) >> 3;                    /* A */
	for (dn = 1; dn < 8; ++ dn) {
		sv_n = 0; restore_cell (); sv_n = 1; restore_cell ();
		if (dw >= dn) {
			t_row = dw - dn;
			if (da >= dn) { t_col = da - dn; sv_n = 0; save_cell (); t_ch = 'z'; put_glyph (); }
			if (da + 1 + dn < 31) { t_col = da + 1 + dn; sv_n = 1; save_cell (); t_ch = 'z'; put_glyph (); }
		}
		sp_UpdateNow ();
		music_note ();
	}
	wf_n = 20; wait_frames ();
}

void update_sprites (void) {
	/* player: the step frame when walking on odd characters or jumping */
	s_n = 0; s_x = px; s_y = py; s_f = pfacing;
	if (pjump) ++ s_f;
	else if (pmoving) s_f += (px >> 3) & 1;
	spr_put ();
	if (push_on) { s_n = 1; s_f = F_BOX; s_x = push_px; s_y = push_py; spr_put (); }
	if (plat_on) { s_n = 2; s_f = F_PLATFORM; s_x = plat_x; s_y = plat_y; spr_put (); }
}

/* the portal flashes (by hand, see PORTAL_ATTR) */
void flash_portal (void) {
	if (portal_y == 255 || -- portal_t) return;
	portal_t = PORTAL_FLASH;
	if (portal_attr == PORTAL_ATTR) portal_attr = PORTAL_ATTR2; else portal_attr = PORTAL_ATTR;
	gp = dlist + (((portal_y << 5) + portal_x) << 2);
	gp [0] = portal_attr; gp [4] = portal_attr;
	rect.row_coord = portal_y; rect.col_coord = portal_x; rect.height = 1; rect.width = 2;
	sp_Invalidate (&rect, &clip_full);
}

/* play the current round. Returns with died or done set. */
void play_round (void) {
	setup_round ();
	while (1) {
		read_keys ();
		if (k_restart) { died = 1; }
		move_platform ();
		move_player ();
		move_push ();
		check_spikes ();
		move_snot ();
		check_gate ();
		check_exit ();
		flash_portal ();
		update_sprites ();
		sp_UpdateNow ();
		wait_frame ();
		if (died) { death_animation (); return; }
		if (done) return;
	}
}

/* ------------------------------------------------------------------ */
/* SCENES                                                              */
/* ------------------------------------------------------------------ */

/* the title picture with nothing else (the original's USR 47410) */
void title_screen (void) {
	spr_hide_all (); cls ();
	ss_n = SCR_TITLE; show_screen ();
	to_shadow = 0;
}

/* "STAGE n / name / ROUND n / GOOD LUCK!" + the new level jingle */
unsigned char *stage_names [] = { "", "MOVING BLOCKS", "JAN-KEN-DO!", "THE SNOTTY MAZE" };
unsigned char *stage_titles [] = { "", "STAGE 1", "STAGE 2", "STAGE 3" };
unsigned char card_stage, card_round;
unsigned char num_buf [8];
void round_card (void) {
	title_screen ();
	t_row = 12;
	t_attr = 4; sp_s = stage_titles [card_stage]; print_centered ();
	t_attr = 7; sp_s = stage_names [card_stage]; print_centered ();
	if (card_round) {
		t_row = 15;
		sp_s = "ROUND "; t_attr = 4; t_col = 13; print_str ();
		t_num = card_round; print_num ();
		t_row = 16; t_attr = 7; sp_s = " GOOD LUCK!"; print_centered ();
	}
	music_init (SONG_NEWLEVEL);
	for (gi = 0; gi < 16; ++ gi) music_note ();
	wf_n = 20; wait_frames ();
}

/* game over: the original part 2's message for the round you lost in */
unsigned char *over_msgs [] = {
	"I BET YOU HATE JAN-KEN-DO!", "YOU HAVEN'T HAD THE GUTS!", "YOU'D BETTER DO KNITTING",
	"IF IT MOVES, IT HURTS", "MY GRANDMA PLAYS BETTER!", "RUN AND DO NOT STOP!",
	"JUMP AND REACH THE SKY!", "FALLING INTO OBLIVION!", "YOU ALMOST GOT THERE!"
};
void game_over (void) {
	if (stage == 1) {
		/* part 1's: plain GAME OVER and wait for a key */
		spr_hide_all (); cls ();
		to_shadow = 0; t_row = 10; t_col = 11; t_attr = 7; sp_s = "GAME OVER"; print_str ();
		music_init (SONG_GAMEOVER);
		for (gi = 0; gi < 12; ++ gi) music_note ();
		wait_key ();
		return;
	}
	title_screen ();
	gv = round_num; if (stage == 2) gv = 0;
	t_row = 11; t_attr = 7; sp_s = over_msgs [gv]; print_centered ();
	t_row = 13; t_attr = 6; sp_s = "* GAME OVER *"; print_centered ();
	music_init (SONG_GAMEOVER);
	for (gi = 0; gi < 12; ++ gi) music_note ();
	wf_n = 75; wait_frames ();
}

/* ---------------------------------------------------------------- */
/* intro (part 1, lines 340-710)                                     */
/* ---------------------------------------------------------------- */

unsigned char *intro_texts [] = {
	/* picture 1 */ "UNKNOWN PLANET              ANDROMEDA GALAXY...", 0,
	/* picture 2 */ "THOUSANDS OF DROIDS ARE     MASS-PRODUCED HERE BY SOME  MYSTERIOUS EVIL RACE.",
	"YOU ARE ONE OF THOSE DROIDS.",
	"YOU WAKE UP AND WONDER WHY  ALL THE OTHERS ARE INACTIVE.",
	"YOU FEEL STRANGE...         YOU LOOK AT YOURSELF...", 0,
	/* picture 3 */ "\"SOMETHING WENT WRONG.      WHY AM I DIFFERENT?\"",
	"A MECHANIC ARRIVES.",
	"\"GOD! SOMETHING WENT WRONG  WITH THIS ANDROID!\"",
	"\"RECYCLERS!                 THROW HER TO THE DUST BIN!\"", 0,
	/* dark */ "ALL IS DARK.",
	"THE NEXT THING YOU REMEMBER IS THAT YOU WAKE UP IN A    SMALL HOUSE.", 0,
	/* picture 4 */ "NOW YOU LIVE WITH A SORCERERHE ALWAYS SAYS THAT YOU     ARE SPECIAL...",
	"\"UNLIKE THE OTHER THIEF     ANDROIDS\"",
	"-THAT'S PRETTY OBVIOUS,     YOU THINK-",
	"\"THE FACT THAT YOU ARE A    WOMAN HAS A SECRET MEANING\",HE ASSURES.",
	"YOU NEED TO KNOW...         YOU FEEL THE NEED TO KNOW!",
	"\"YOU MUST FIND PHANTOMAS\",  HE SAYS.",
	"\"PHANTOMAS IS ANOTHER       SPECIAL ANDROID\"",
	"\"HE MANAGED TO ESCAPE FROM  ANDROMEDA AND ITS CONTROL\"", 0,
	/* picture 5 */ "\"NOW HE IS NOT JUST AN      AUTHOMATON. NEITHER YOU ARE!",
	"IF YOU MANAGE TO ATTACK HIM WITH YOUR BREASTS ...       YOU'LL KNOW THE TRUTH!\"",
	"YOU KNOW WHAT YOU SHOULD DO!NOW GO FIND PHANTOMAS!!!", 0
};
/* picture before each group (255 = clear the screen instead) */
unsigned char intro_pics [] = { SCR_INTRO1, SCR_INTRO2, SCR_INTRO3, 255, SCR_INTRO4, SCR_INTRO5 };

void intro (void) {
	skipped = 0;
	music_init (SONG_INTRO);
	spr_hide_all ();
	gj = 0;                                   /* index in intro_texts */
	for (gi = 0; gi < 6; ++ gi) {
		cls ();
		if (intro_pics [gi] != 255) { ss_n = intro_pics [gi]; show_screen (); }
		to_shadow = 0; t_attr = 7;
		while (intro_texts [gj]) {
			t_row = 16; sp_s = intro_texts [gj ++];
			typewriter (); if (skipped) return;
			notes_n = 16; play_notes (); if (skipped) return;
			if (intro_texts [gj]) clear_text ();
		}
		++ gj;
	}
	/* "Espera tecla con musica" */
	while (any_key ());
	while (!any_key ()) music_note ();
}

/* ---------------------------------------------------------------- */
/* title / menu (part 1, lines 1460-1690)                            */
/* returns 1 to play, 0 if nobody pressed a key (back to the intro)   */
/* ---------------------------------------------------------------- */

unsigned char title (void) {
	title_screen ();
	t_attr = 0x46; t_row = 10; sp_s = "MAIN CODE & GFX: NA*TH*AN"; print_centered ();
	t_row = 12; sp_s = "ADDITIONAL CODE: BLACK HOLE"; print_centered ();
	t_attr = 7; t_row = 14; sp_s = "BEST BOY: TONY BRAZIL"; print_centered ();
	t_row = 16; sp_s = "SPECIAL THANKS TO IFO & WYZ"; print_centered ();
	t_attr = 5; t_row = 18; sp_s = "(C) WOPR2K & CEZGS 2006"; print_centered ();
	sp_s = "CEZGS.COMPUTEREMUZONE.COM"; print_centered ();

	/* the frame of 'z's, drawn from two opposite corners */
	t_ch = 'z';
	bp_ms = 1; bp_note = 15;
	for (gi = 0; gi <= 30; gi += 2) {
		t_row = 8; t_col = gi; t_attr = 0x32; put_glyph ();
		t_row = 21; t_col = 30 - gi; t_attr = 0x22; put_glyph (); beep (); wf_n = 1; wait_frames ();
	}
	for (gi = 9; gi <= 19; gi += 2) {
		t_row = gi; t_col = 31; t_attr = 0x32; put_glyph ();
		t_row = 28 - gi; t_col = 0; t_attr = 0x22; put_glyph (); beep (); wf_n = 1; wait_frames ();
	}
	for (gi = 0; gi < 16; ++ gi) {                  /* N = 31, 29 .. 1 */
		t_row = 21; t_col = 31 - (gi << 1); t_attr = 0x32; put_glyph ();
		t_row = 8; t_col = 1 + (gi << 1); t_attr = 0x22; put_glyph (); beep (); wf_n = 1; wait_frames ();
	}
	for (gi = 20; gi >= 10; gi -= 2) {
		t_row = gi; t_col = 0; t_attr = 0x32; put_glyph ();
		t_row = 30 - gi; t_col = 31; t_attr = 0x22; put_glyph (); beep (); wf_n = 1; wait_frames ();
	}
	t_row = 23; t_col = 0; t_attr = 0x41; sp_s = "PRESS ANY KEY TO PLAY PHANTOMASA"; print_str ();
	wf_n = 20; wait_frames ();

	music_init (SONG_TITLE);
	for (gw = 0; gw < 320; ++ gw) {           /* 320 notes = 1 minute */
		music_note ();
		if (any_key ()) return 1;
	}
	return 0;
}

/* ---------------------------------------------------------------- */
/* between stage 1 and 2 (part 1's end, part 2's access code)        */
/* ---------------------------------------------------------------- */

void chambers (void) {
	title_screen ();
	t_attr = 5;
	t_row = 9; sp_s = "YOU MANAGED TO BREAK INTO"; print_centered ();
	t_row = 11; sp_s = "PHANTOMAS SECRET CHAMBERS"; print_centered ();
	wait_key ();
}

/* ---------------------------------------------------------------- */
/* STAGE 2: JAN-KEN-DO (part 2, lines 1710-2160). Everything on the   */
/* real screen; rock / scissors / paper are three areas of the        */
/* picture that change colour.                                        */
/* returns 1 if you won, 0 if you ran out of lives                    */
/* ---------------------------------------------------------------- */

unsigned char jkd_col [] = { 0, 4, 9, 13 };
unsigned char jkd_w [] = { 0, 5, 4, 5 };
unsigned char *jkd_names [] = { "", "ROCK!    ", "SCISSORS!", "PAPER!   " };
unsigned char jc, jc1, jp1, jp2;

void jkd_area (void) {                   /* choice jc in colour t_attr */
	t_row = 12; t_col = jkd_col [jc]; t_h = 5; t_w = jkd_w [jc]; set_attr_rect ();
}
void jkd_clear_panel (void) {
	t_row = 1; t_col = 7; t_h = 7; t_w = 17; t_attr = 7; clear_rect ();
}
void jkd_counters (void) {
	t_row = 14; t_col = 23; t_attr = 7; sp_s = "PLR:"; print_str (); t_attr = 5; t_num = jp1; print_num ();
	t_row = 15; t_col = 23; t_attr = 7; sp_s = "CPU:"; print_str (); t_attr = 5; t_num = jp2; print_num ();
	t_row = 15; t_col = 29; t_h = 2; t_w = 1; t_attr = 1; set_attr_rect ();
	t_h = 1;
	if (jp1 > jp2) { t_row = 15; t_attr = 0x31; set_attr_rect (); }
	if (jp2 > jp1) { t_row = 16; t_attr = 0x11; set_attr_rect (); }
	t_row = 23; t_col = 24; t_attr = 4; sp_s = "LIVES:"; print_str (); t_num = lives; print_num ();
	if (t_col < 32) { t_ch = ' '; put_glyph (); }
}
void jkd_tune (void) {                  /* bp_note, then gj */
	bp_ms = 100; beep (); bp_note = gj; bp_ms = 200; beep ();
}

unsigned char jankendo (void) {
	spr_hide_all (); cls ();
	ss_n = SCR_JANKENDO; show_screen ();
	to_shadow = 0;
	jp1 = jp2 = 0;
	t_attr = 0x47; t_row = 1; t_col = 8; sp_s = "SECURITY SYSTEM"; print_str ();
	t_attr = 7;
	t_row = 3; t_col = 8; sp_s = "I WILL ONLY LET"; print_str ();
	t_row = 4; t_col = 8; sp_s = "YOU PASS IF YOU"; print_str ();
	t_row = 5; t_col = 8; sp_s = "WIN 3 MATCHES"; print_str ();
	t_row = 6; t_col = 8; sp_s = "OF JAN-KEN-DO!"; print_str ();
	wait_key ();
	jkd_clear_panel ();
	t_attr = 1; for (jc = 1; jc < 4; ++ jc) jkd_area ();
	jkd_counters ();
	jc = 1;
	while (1) {
		/* the player picks with O / P, SPACE or ENTER */
		t_attr = 6; jkd_area ();
		t_attr = 7; t_row = 1; t_col = 8; sp_s = "PLR SELECTION:"; print_str ();
		while (1) {
			t_attr = 0x46; t_row = 2; t_col = 11; sp_s = jkd_names [jc]; print_str ();
			while (any_key ());
			while (1) {
				read_keys ();
				if (k_right && jc < 3) { bp_note = 20; bp_ms = 10; beep (); t_attr = 1; jkd_area (); ++ jc; t_attr = 6; jkd_area (); break; }
				if (k_left && jc > 1) { bp_note = 20; bp_ms = 10; beep (); t_attr = 1; jkd_area (); -- jc; t_attr = 6; jkd_area (); break; }
				if (k_fire) break;
				rnd ();
			}
			if (k_fire) break;
		}
		jc1 = jc;
		/* the CPU "thinks": 10 random picks */
		for (gi = 0; gi < 10; ++ gi) {
			t_attr = 1; jkd_area ();
			jc = rnd () % 3 + 1;
			t_attr = 7; t_row = 4; t_col = 8; sp_s = "CPU SELECTION:"; print_str ();
			t_attr = 0x42; t_row = 5; t_col = 11; sp_s = jkd_names [jc]; print_str ();
			t_attr = 2; jkd_area ();
			bp_note = -12; bp_ms = 4; beep ();
			wf_n = 2; wait_frames ();
		}
		t_attr = 7; t_row = 7; t_col = 8;
		if (jc == jc1) {
			sp_s = "IT'S A TIE!!"; print_str ();
			bp_note = -2; gj = -2; jkd_tune ();
		} else if ((jc1 == 1 && jc == 2) || (jc1 == 2 && jc == 3) || (jc1 == 3 && jc == 1)) {
			sp_s = "YOU WIN!!"; print_str ();
			bp_note = 0; gj = 7; jkd_tune ();
			++ jp1;
		} else {
			sp_s = "YOU LOSE!!"; print_str ();
			bp_note = 7; gj = 0; jkd_tune ();
			++ jp2;
		}
		jkd_counters (); wait_key (); jkd_clear_panel ();
		if (jp1 == 3) {
			t_attr = 7;
			t_row = 2; t_col = 8; sp_s = "YOU WON THRICE!"; print_str ();
			t_row = 4; t_col = 9; sp_s = "YOU MAY ENTER"; print_str ();
			t_row = 6; t_col = 8; sp_s = "THE SNOTTY MAZE"; print_str ();
			wait_key ();
			return 1;
		}
		if (jp2 == 3) {
			t_attr = 7;
			t_row = 3; t_col = 9; sp_s = "I WON THRICE!"; print_str ();
			t_row = 5; t_col = 8; sp_s = "YOU LOST 1 LIFE"; print_str ();
			music_init (SONG_LIFELOST);
			for (gi = 0; gi < 7; ++ gi) music_note ();
			if (lives == 0) { stage = 2; return 0; }
			-- lives; jp1 = jp2 = 0;
			jkd_counters (); wait_key (); jkd_clear_panel ();
		}
		t_attr = 1; for (jc = 1; jc < 4; ++ jc) jkd_area ();
		jc = jc1;
	}
}

/* ---------------------------------------------------------------- */
/* OUTRO (part 2, lines 3150-4390)                                   */
/* ---------------------------------------------------------------- */

/* texts: 1 = short line at column 12, 2 = long line at column 2,
   3 = pause, 4 = clear short lines, 5 = clear long lines,
   6 = clear the screen (no picture), 10+n = show picture n            */
#define O_SHORT 1
#define O_LONG  2
#define O_PAUSE 3
#define O_CLS   4
#define O_CLL   5
#define O_BLANK 6
#define O_PIC   10
unsigned char outro_ops [] = {
	O_PIC + 0, O_SHORT, O_SHORT, O_PAUSE, O_CLS, O_SHORT, O_SHORT, O_PAUSE, O_CLS,
	O_SHORT, O_SHORT, O_PAUSE, O_CLS, O_SHORT, O_SHORT, O_PAUSE, O_CLS, O_SHORT, O_PAUSE,
	O_PIC + 1, O_LONG, O_PAUSE, O_CLL, O_LONG, O_PAUSE,
	O_PIC + 2, O_LONG, O_LONG, O_PAUSE, O_CLL, O_LONG, O_LONG, O_LONG, O_PAUSE, O_CLL,
	O_LONG, O_LONG, O_PAUSE, O_CLL, O_LONG, O_PAUSE,
	O_BLANK, O_LONG, O_LONG, O_PAUSE,
	O_PIC + 3, O_SHORT, O_SHORT, O_PAUSE, O_CLS, O_SHORT, O_SHORT, O_SHORT, O_PAUSE, O_CLS,
	O_SHORT, O_SHORT, O_SHORT, O_PAUSE, O_CLS, O_SHORT, O_SHORT, O_SHORT, O_PAUSE, O_CLS,
	O_SHORT, O_SHORT, O_PAUSE, O_CLS, O_SHORT, O_SHORT, O_PAUSE, O_CLS, O_SHORT, O_SHORT,
	O_PAUSE, O_CLS, O_SHORT, O_SHORT, O_SHORT, O_PAUSE, O_CLS, O_SHORT, O_SHORT, O_PAUSE,
	O_PIC + 1, O_LONG, O_PAUSE, O_CLL, O_LONG, O_PAUSE,
	O_BLANK, O_LONG, O_LONG, 0
};
unsigned char *outro_texts [] = {
	"HERE YOU ARE,", "PHANTOMAS!!", "NO, DON'T EVEN", "THINK TO ESCAPE.",
	"HAH! DON'T MOAN", "AS I'M MERCYLESS!", "WITH THIS I WILL BE", "ABLE TO UNDERSTAND!",
	"BREAST ATTACK!",
	"IEAAAAAAARGH!!!", "WHAT? UH ...",
	"HE'S PASSED OUT.", "AND I FEEL NOTHING...", "WHAT'S HAPPENNING HERE?",
	"I WAS SUPPOSED TO LEARN ALL", "THE TRUTH BY DOING THIS!",
	"NOW I DO REALISE THAT THE", "SORCERER WAS CHEATING ME!!!", "I HAVE TO FIND OUT MYSELF.",
	"PHANTOMASA TRAVELS BACK TO", "THE SORCERER'S HOUSE.",
	"\"YOU CHEATED ME TO", "ATTACK PHANTOMAS!\"", "\"WHY? I TRUSTED YOU", "AND YOU JUST LIED",
	"TO USE ME!!\"", "SORCERER: \"I DON'T", "CARE AT ALL IF YOU", "ARE UPSET ...",
	"AND ANDROMEDA ONLY", "NEEDED YOU TO DO", "THE DIRTY STUFF ...", "THIS WAY PHANTOMAS",
	"IS OUT OF BUSINESS\"", "\"THIS AIN'T OVER,", "DAMN SORCERER ...\"", "\"WHAT YOU MADE ME",
	"DO WAS NOT RIGHT\"", "\"I NEED TO DO A", "COUPLE OF THINGS", "TO FIX MY FAULT\"",
	"\"FIRST OF ALL ...", "BREAST ATTACK!!\"",
	"IEAAAAAAARGH!!!", "THE SORCERER IS DEAD.",
	"\"WHAT ELSE COULD I DO", "TO FIX MY FAULT?\" ..."
};
unsigned char outro_pics [] = { SCR_OUTRO1, SCR_OUTRO2, SCR_OUTRO3, SCR_OUTRO4 };
unsigned char oy, op;
unsigned char *op_p;

/* a pause of about 2.5 s of music (the interrupt-driven music of the
   original is played note by note here); a key skips the outro */
void outro_pause (void) {
	notes_n = 10; play_notes ();
}

void outro (void) {
	skipped = 0;
	spr_hide_all (); cls ();
	music_init (SONG_INTRO);
	to_shadow = 0; t_attr = 7;
	op_p = outro_ops; gj = 0; oy = 16;
	while (*op_p && !skipped) {
		op = *op_p ++;
		if (op >= O_PIC) { cls (); ss_n = outro_pics [op - O_PIC]; show_screen (); oy = 16; }
		else if (op == O_BLANK) cls ();          /* the text goes on below */
		else if (op == O_SHORT || op == O_LONG) {
			t_row = oy; t_col = 12; if (op == O_LONG) t_col = 2;
			t_attr = 7; sp_s = outro_texts [gj ++]; print_str ();
			oy += 2;
			notes_n = 2; play_notes ();
		} else if (op == O_PAUSE) outro_pause ();
		else {
			t_row = 16; t_h = 1; t_attr = 7;
			if (op == O_CLS) { t_col = 12; t_w = 20; } else { t_col = 0; t_w = 32; }
			clear_rect (); t_row = 18; clear_rect (); t_row = 20; clear_rect ();
			oy = 16;
		}
	}
	if (!skipped) { notes_n = 4; play_notes (); }
	/* the end */
	cls (); ss_n = SCR_OUTRO5; show_screen ();
	t_row = 16; t_col = 1; t_attr = 6; sp_s = "C O N G R A T U L A T I O N S"; print_str ();
	t_row = 18; t_col = 4; t_attr = 0x47; sp_s = "YOU FINISHED THIS QUEST!"; print_str ();
	t_row = 20; t_col = 3; t_attr = 4; sp_s = "LOOK FOR NEW ADVENTURES AT"; print_str ();
	t_row = 21; t_col = 1; sp_s = "THE LAND OF THE GRUNGE WIZARDS"; print_str ();
	music_init (SONG_FINAL);
	while (any_key ());
	while (!any_key ()) music_note ();
}

/* ---------------------------------------------------------------- */
/* THE GAME                                                          */
/* ---------------------------------------------------------------- */

/* play all rounds of one stage, from round_n on; 1 = cleared,
   0 = game over                                                       */
unsigned char cur_stage;
unsigned char play_stage (void) {
	cur_stage = rounds [round_offs [round_n]];
	while (round_n < NROUNDS && rounds [round_offs [round_n]] == cur_stage) {
		card_stage = cur_stage; card_round = rounds [round_offs [round_n] + 1];
		round_card ();
		play_round ();
		if (died) {
			if (lives == 0) { game_over (); return 0; }
			-- lives;
			continue;                             /* same round again */
		}
		++ round_n;
	}
	return 1;
}

void game (void) {
	lives = START_LIVES;
	round_n = 0;
	if (!play_stage ()) return;                  /* stage 1 */
	chambers ();
	lives = START_LIVES;                         /* part 2 started with 10 */
	card_stage = 2; card_round = 0; round_card ();
	if (!jankendo ()) { game_over (); return; }
	if (!play_stage ()) return;                  /* stage 3 */
	outro ();
}

/* ---------------------------------------------------------------- */
/* the CEZ logo, the very first thing (part 1, lines 120-150)        */
/* ---------------------------------------------------------------- */
void cez (void) {
	cls (); ss_n = SCR_CEZLOGO; show_screen ();
	bp_note = 3; bp_ms = 400; beep ();
	bp_note = -2; beep ();
	bp_note = 10; bp_ms = 600; beep ();
	wf_n = 50; wait_frames ();
}

/* ---------------------------------------------------------------- */
/* LOADING THE REST. The BASIC loader only loads this program; this    */
/* reads the blocks that follow it on the tape (headerless, flag $FF)  */
/* with the ROM's LD-BYTES: the music to $F600 in bank 0, then the     */
/* screens into banks 1, 3, 4... load_table (conv.py) lists them as    */
/* bank, address, length; $FF ends it. Needs the 48K ROM paged (bit 4  */
/* of $7FFD) and runs before splib2 takes over the interrupts.         */
/* ---------------------------------------------------------------- */
void load_blocks (void) {
	#asm
		ld  hl, _load_table
	.lb_next
		ld  a, (hl)
		cp  $FF
		jr  z, lb_done
		inc hl
		or  $10
		ld  bc, $7FFD
		di
		out (c), a
		ld  e, (hl)
		inc hl
		ld  d, (hl)
		inc hl
		push de
		pop ix                      ; where
		ld  e, (hl)
		inc hl
		ld  d, (hl)
		inc hl                      ; de = how much
		push hl
		ld  a, $FF                  ; data block
		scf                         ; load, not verify
		call $0556                  ; LD-BYTES
		pop hl
		jr  lb_next
	.lb_done
		di
		ld  a, $10                  ; bank 0, 48K ROM
		ld  bc, $7FFD
		out (c), a
	#endasm
}

main () {
	load_blocks ();
	font = (unsigned char *) FONT_ADDR;
	sprite_frames = (unsigned char *) SPRITES_ADDR;
	sp_AddMemory (0, NUMBLOCKS, 14, AD_FREE);
	sp_InitIM2 (ISR_ADDR);
	sp_CreateGenericISR (ISR_ADDR);
	sp_RegisterHook (255, isr);
	#asm
		ei
	#endasm
	sp_Initialize (7, 0);
	dlist = sp_CompDListAddr (0, 0);
	sp_Border (BLACK);
	*((unsigned char *) 23624) = 0;           /* BORDCR: black border */

	s_n = 0; s_f = 0; spr_create ();          /* player, in front */
	s_n = 1; s_f = 1; spr_create ();          /* box being pushed */
	s_n = 2; s_f = 2; spr_create ();          /* moving platform */
	spr_hide_all ();

	cez ();
	while (1) {
		intro ();
		while (title ()) game ();
	}
}
