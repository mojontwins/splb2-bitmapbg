/*
 * GHOST EMBUSTES 2 - (c) 1990 by Loko soft (jose luis, aged ~12)
 *
 * C + inline asm port for z88dk (classic, sccz80) + splib2 of a Sinclair
 * BASIC game (orig/chem2.bas.txt, listed from the snapshot orig/CHEM2.Z80).
 * The original moved everything a character at a time with PRINT AT; here
 * the player, the car, ghosts, rocks and the moving platform are masked
 * splib2 sprites moving pixel by pixel at 25 fps over a tile background.
 *
 * The original had a lot of bugs. BUGS.md lists every one we found, with
 * its BASIC line and what this port does about it; comments marked "BUG:"
 * point at them.
 *
 * ------------------------------------------------------------------------
 * THE GAME (each part is a function below, named after it)
 * ------------------------------------------------------------------------
 *   intro       the typewriter texts and the graphics       (lines 10-140)
 *   street      drive the Ghost-tipo home                   (170-390)
 *   shop        "Ghost-casa": buy things, E to leave        (1000-1120)
 *   song        the logo and "Let's go" tune                (3000-3100)
 *   prueba      shoot the ghost when it's over Tracy        (3200-3290)
 *   trampa      night falls on the city (cut scene)         (3410-3610)
 *   gregory     three platform screens                      (3620-4170)
 *   jefe        5 rooms of spikes with gaps                 (4180-4240)
 *   ayudante    mines, rocks, electric fences, come-come,
 *               the sea and its ghost, a field of spikes    (4250-5130)
 *   the_end     choose a character                          (5140-5330)
 *
 * ------------------------------------------------------------------------
 * HOW TO KEEP DEVELOPING IT
 * ------------------------------------------------------------------------
 *   gfx/screens/*.png  the static screens (street, shop, logo, city, the
 *                      three Gregory screens), drawn in Spectrum colours
 *   gfx/tiles.png      characters 128-255 (block graphics, the UDGs A-U...)
 *   gfx/font.png       characters 32-127
 *   gfx/sprites.png    sprite frames (see the F_* names below)
 *   texts.txt          every text
 *   this file          the game. Tunable numbers are the #defines below.
 * Run build.sh: tools/conv.py turns those into src/assets.h, then zcc
 * builds ghostembustes2.tap.
 *
 * Screens: the game draws them, then often prints on them (texts, the
 * spikes, the fences...). Anything a screen PNG shows in a cell becomes a
 * character: the font's, a tile's, or a new tile conv.py makes up.
 *
 * ------------------------------------------------------------------------
 * CODING CONVENTIONS (same as the other examples)
 * ------------------------------------------------------------------------
 *   - No local variables, few parameters: sccz80 handles them slowly and
 *     sometimes wrongly. Functions work on globals, listed with WHO
 *     CLOBBERS WHAT below.
 *   - Sprite positions are in PIXELS (x 0-255, y 0-191). The original's
 *     "a" (column) and "w" (row) are x / 8 and y / 8.
 *   - Never use a comparison as a number ((a == b) + 1): in sccz80 a true
 *     comparison isn't 1. Use an if.
 *
 * ------------------------------------------------------------------------
 * MEMORY MAP (48K, splib2t.lib: tile mode, no shadow screen)
 * ------------------------------------------------------------------------
 *   $5E88 - ...     this binary (org 24200): code, splib2, assets, the
 *                   display list. It may grow up to $D7FF.
 *   $D800 - $DA75   splib2 block memory (NUMBLOCKS x 15 bytes)
 *   $DA76 - $E075   screen buffer: a packed screen is depacked here
 *   $E076 - $EDFF   FREE
 *   $EE00 - $EFFF   stack (grows down from $F000)
 *   $F000 - $F100   IM2 vector table, $F1F1 the ISR    } fixed by splib2's
 *   $F200 - $FFFF   splib2 rotation tables             } SPconfig.def
 */

#pragma output STACKPTR=61440

#include <spritepack.h>

#define NUMBLOCKS     42                /* 4 sprites x (1 + 9 chars) + hook */
#define AD_FREE       0xD800            /* splib2 block memory              */
#define SCRBUF        (AD_FREE + NUMBLOCKS * 15)   /* = $DA76, 1536 bytes   */

#include "assets.h"

/* ------------------------------------------------------------------ */
/* TUNING                                                              */
/* ------------------------------------------------------------------ */

#define WALK_SPEED      2      /* px per frame, all "top view" parts and   */
                               /* Gregory. Must divide 8.                  */
#define GHOST_SPEED     2      /* the ghosts of the Prueba and the sea     */
#define ROCK_SPEED      2      /* falling rocks                            */
#define TICK            4      /* frames per "loop" of the original: the   */
                               /* fences, the come-come clock, messages    */
#define COME_TIME       824    /* come-come clock (22*32+120, as BASIC)    */
#define FALL_LIMIT      16     /* Gregory: falling further than this (px)  */
                               /* below where you left the ground kills    */
#define MINES           21     /* FOR n=0 TO 20                            */
#define SPIKE_ROOMS     5      /* the jefe's chamber: FOR y=1 TO 5         */
#define SEA_SPIKES      51     /* FOR n=0 TO 50                            */

/* Gregory's jump (the original's 9200: 3 characters up, 6 across, which
   here is 24 px up and 48 px forward). Pixels up per frame, then the fall
   (2 px per frame) brings you down: 24 frames at 2 px across = 48 px.
   Keep the numbers even: floors are multiples of 8.                    */
unsigned char jump_tab [] = { 4, 4, 2, 2, 2, 2, 2, 2, 2, 2, 0, 0 };
#define JUMP_LEN        12

/* ------------------------------------------------------------------ */
/* CHARACTERS (tile numbers). 32-127 are the font, as on the Spectrum.  */
/* ------------------------------------------------------------------ */

#define T_SPACE    32
#define T_SPIKE    94          /* "^", the ROM's up arrow: spikes, mines   */
#define T_LINE     95          /* "_"                                      */
#define T_BLOCKS   128         /* 128-143 block graphics                   */
#define T_UDG      144         /* UDG A = 144 ... U = 164                  */
#define T_LOGO_A   144         /* A B / C D: the Ghost-tipo logo           */
#define T_E        148         /* line at the bottom: the sea's wall       */
#define T_F        149         /* line at the left: poles, ladders, beam   */
#define T_H        151         /* fence 1                                  */
#define T_K        154         /* little square: windows                   */
#define T_M        156         /* M / N: Tracy                             */
#define T_O        158         /* O / P: you (in the intro)                */
#define T_R        161         /* the Ghost "a secas"                      */
#define T_S        162         /* grass; fence 2                           */
#define T_T        163         /* bricks; walls                            */
#define T_WAVE     173         /* 173 174 175: a wave                      */

#define INK(n)     (n)
#define PAPER(n)   ((n) << 3)
/* BRIGHT (64) and FLASH (128) come from spritepack.h. FLASH also means:
   splib2 draws no sprites in that cell (FLASHASFG in SPconfig.def). */

/* sprite frames (gfx/sprites.png) */
#define F_RIGHT    0           /* +1: step                                 */
#define F_LEFT     2           /* +1: step                                 */
#define F_CLIMB    4           /* +1: the other hand                       */
#define F_DEAD     6
#define F_CAR      8           /* the Ghost-tipo (also the shop's cursor)  */
#define F_GHOST    9           /* +1: wobble                               */
#define F_PLATFORM 11
#define F_ROCK     12

/* ------------------------------------------------------------------ */
/* splib2 housekeeping (as in every splib2 program)                   */
/* ------------------------------------------------------------------ */

void *my_malloc (uint bytes) { return sp_BlockAlloc (0); }
void *u_malloc = my_malloc;
void *u_free = sp_FreeBlock;

struct sp_Rect clip_full = {0, 0, 24, 32};
struct sp_Rect rect;               /* scratch rectangle (paint_attr...)    */

struct sp_SS *spr [2];             /* 0 the player, 1 everything else      */
unsigned char *spr_cur [2];        /* frame each sprite shows now          */

unsigned char isrc;                /* interrupts since the last frame      */

/* ISR hook: counts interrupts. splib2 needs 2 spare bytes before it. */
extern void isr (void);
#asm
	defw 0
._isr
	ld  hl, _isrc
	inc (hl)
	ret
#endasm

/* addresses inside splib2: its empty sprite graphic and the display list */
extern unsigned int null_ptr;
extern unsigned int dl_addr;
#asm
	LIB SPNullSprPtr
	LIB SPDisplayList
._null_ptr
	defw SPNullSprPtr
._dl_addr
	defw SPDisplayList
#endasm

/* ------------------------------------------------------------------ */
/* GLOBALS                                                             */
/*                                                                     */
/* WHO CLOBBERS WHAT                                                   */
/*   wait_frame, pause50 ........ nothing                              */
/*   beep ....................... gp bp_count bp_half bp_spk            */
/*   read_keys .................. gv                                   */
/*   wait_key, any_key, KEY_* ... nothing (KEY_*: lk_port lk_bit)      */
/*   cell_get ................... gw gp (reads cr cc; sets c_tile c_attr) */
/*   cell_put ................... nothing (reads cr cc c_tile c_attr)  */
/*   paint_attr ................. gi gj gw gp (reads rect, c_attr)      */
/*   msg_flicker ................ rect c_attr + paint_attr (gi gj!)    */
/*   cls ........................ nothing (reads c_attr)                */
/*   recolour_blank ............. gw gp (reads gv = the ink)           */
/*   show_screen ................ gw gp tw_p (reads ss)                */
/*   put_ch, print_raw .......... tx_* cr cc c_tile c_attr             */
/*   type_text .................. the same + gv tw_p tw_len + beep     */
/*   print_num .................. gi tx_num + put_ch                   */
/*   clear_row .................. cc c_tile                            */
/*   msg ........................ cr + clear_row, type_text           */
/*   logo, header ............... cr cc c_tile c_attr + print_raw      */
/*   spr_put, spr_hide .......... gp (reads s_n s_f s_x s_y)            */
/*   cell_class ................. + cell_get                           */
/*   td_move, td_free ........... gk cr cc + cell_class                */
/*   pf_* (Gregory) ............. gi gk gv pr pc cr cc + cell_class    */
/*   player_dies ................ gi + beep, msg                       */
/*   scenes ..................... anything                             */
/* ------------------------------------------------------------------ */

unsigned char gi, gj, gk, gv;      /* general scratch                       */
unsigned int  gw;
unsigned char *gp;
unsigned char *tw_p;                /* a second pointer (texts, screens)    */

unsigned char *dl;                 /* splib2's display list: 4 bytes per    */
                                   /* cell: attribute, tile, sprites        */
unsigned char *scrbuf;             /* SCRBUF                                */

unsigned char cr, cc;              /* a cell: row, column                   */
unsigned char c_tile, c_attr;      /* ...its tile and attribute             */

unsigned char s_n, s_f, s_x, s_y;  /* spr_put: sprite, frame, x, y          */

unsigned int  money;
unsigned char stage;               /* progress inside a part (rooms, the jefe's chambers) */
unsigned char tick;                /* frames into the current TICK                         */
unsigned char restart_game;        /* set by the shop (RUN in the original) */

/* ------------------------------------------------------------------ */
/* LOW LEVEL                                                           */
/* ------------------------------------------------------------------ */

/* IN from a port. FASTCALL: the parameter arrives in HL. */
unsigned char __FASTCALL__ inport (unsigned int port) {
	#asm
		ld  b, h
		ld  c, l
		in  a, (c)
		ld  l, a
		ld  h, 0
	#endasm
}

/* 25 fps: wait until two interrupts have passed since the last frame */
void wait_frame (void) {
	while (isrc < 2);
	isrc = 0;
}

/* 16-bit xorshift random numbers (seeded from R in main) */
unsigned int rnd_seed = 0x1234;
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

/* BEEP: bp_ms milliseconds of note bp_n. bp_n is the BASIC's pitch minus
   BEEP_MIN: use P(n), e.g. bp_n = P(-10) for BEEP x, -10.             */
#define P(n)  ((n) - BEEP_MIN)
unsigned char bp_n;
unsigned int  bp_ms, bp_count, bp_half;
unsigned char bp_spk;              /* what goes out to $FE: border colour + */
unsigned char border;              /* the speaker bit                       */
void beep (void) {
	gp = beep_tab + (bp_n << 2);
	bp_count = gp [0] | (gp [1] << 8);
	/* half periods = Hz * ms / 500 */
	bp_half = ((unsigned long) (gp [2] | (gp [3] << 8)) * bp_ms) / 500;
	if (bp_half == 0) return;
	bp_spk = border;
	#asm
		ld  de, (_bp_count)
		ld  hl, (_bp_half)
	.beep_half
		ld  a, (_bp_spk)
		xor 16
		ld  (_bp_spk), a
		out ($FE), a
		ld  b, d
		ld  c, e
	.beep_wait                      ; 26 T-states a turn (see conv.py)
		dec bc
		ld  a, b
		or  c
		jr  nz, beep_wait
		dec hl
		ld  a, h
		or  l
		jr  nz, beep_half
	#endasm
}

/* border colour (sp_Border, and remembered for beep) */
void set_border (void) {
	sp_Border (border);
}

/* PAUSE n of the original: n/50 s, or until a key is pressed */
unsigned char any_key (void);
unsigned char pause_n;
void pause50 (void) {
	isrc = 0;
	while (isrc < pause_n) if (any_key ()) break;
	isrc = 0;
}

/* ------------------------------------------------------------------ */
/* KEYBOARD. The original used a Sinclair joystick (keys 6 7 8 9 0).    */
/* This port also takes Q A O P and SPACE.                             */
/* ------------------------------------------------------------------ */

unsigned char k_left, k_right, k_up, k_down, k_fire;
unsigned char k_fire_old, k_fire_new;      /* fire pressed this frame    */

void read_keys (void) {
	gv = inport (0xEFFE);                    /* 6 7 8 9 0: bits 4..0  */
	k_left = k_right = k_up = k_down = k_fire = 0;
	if (!(gv & 16)) k_left = 1;
	if (!(gv & 8)) k_right = 1;
	if (!(gv & 4)) k_down = 1;
	if (!(gv & 2)) k_up = 1;
	if (!(gv & 1)) k_fire = 1;
	gv = inport (0xDFFE);                    /* P O: bits 0, 1         */
	if (!(gv & 2)) k_left = 1;
	if (!(gv & 1)) k_right = 1;
	if (!(inport (0xFBFE) & 1)) k_up = 1;    /* Q                      */
	if (!(inport (0xFDFE) & 1)) k_down = 1;  /* A                      */
	if (!(inport (0x7FFE) & 1)) k_fire = 1;  /* SPACE                  */
	k_fire_new = 0;
	if (k_fire && !k_fire_old) k_fire_new = 1;
	k_fire_old = k_fire;
}

/* any key at all? (reads the 8 half rows at once) */
unsigned char any_key (void) {
	if ((inport (0x00FE) & 0x1F) != 0x1F) return 1;
	return 0;
}

/* PAUSE 0 of the original: wait for a key. Waits for the keys to be
   released first, so a key still held from the game doesn't count. */
void wait_key (void) {
	while (any_key ());
	while (!any_key ());
	k_fire_old = 1;              /* don't let that key fire in the game */
}

/* letter keys: is the key of half row `port`, bit `bit`, pressed? */
unsigned char lk_bit;
unsigned int lk_port;
unsigned char letter_key (void) {
	if (inport (lk_port) & lk_bit) return 0;
	return 1;
}
#define KEY(port, bit)  (lk_port = (port), lk_bit = (bit), letter_key ())
#define KEY_E   KEY (0xFBFE, 4)
#define KEY_T   KEY (0xFBFE, 16)
#define KEY_J   KEY (0xBFFE, 8)
#define KEY_A   KEY (0xFDFE, 1)
#define KEY_S   KEY (0xFDFE, 2)
#define KEY_N   KEY (0x7FFE, 8)

/* ------------------------------------------------------------------ */
/* CELLS. In tile mode a cell of the screen is a tile number and an     */
/* attribute in splib2's display list; sp_PrintAtInv changes one and    */
/* marks it for redrawing at the next sp_UpdateNow.                     */
/* ------------------------------------------------------------------ */

/* read cell (cr, cc) into c_tile, c_attr */
void cell_get (void) {
	gw = cr; gw = ((gw << 5) + cc) << 2;
	gp = dl + gw;
	c_attr = gp [0]; c_tile = gp [1];
}

/* write c_tile, c_attr at (cr, cc). Rows 0-23 and columns 0-31 only:
   splib2 doesn't check, and outside that it writes over other memory. */
void cell_put (void) {
	if (cr > 23 || cc > 31) return;
	sp_PrintAtInv (cr, cc, c_attr, c_tile);
}

/* the whole screen: blank tiles with attribute c_attr */
void cls (void) {
	sp_ClearRect (&clip_full, c_attr, T_SPACE, sp_CR_TILES);
	sp_Invalidate (&clip_full, &clip_full);
}

/* attribute c_attr on the cells of rect, keeping their tiles (the
   original's PRINT OVER 1 with spaces: the colour changes, not the
   picture) */
void paint_attr (void) {
	for (gi = 0; gi < rect.height; ++ gi) {
		gw = rect.row_coord + gi; gw = ((gw << 5) + rect.col_coord) << 2;
		gp = dl + gw;
		for (gj = 0; gj < rect.width; ++ gj) { *gp = c_attr; gp += 4; }
	}
	sp_Invalidate (&rect, &clip_full);
}

/* INK gv for every blank cell (tile 32): sprites take the colours of the
   cells they are on, so this is the colour of the player where there is
   nothing drawn (e.g. green, as the original printed him with INK 4) */
void recolour_blank (void) {
	gp = dl;
	for (gw = 0; gw < 768; ++ gw) {
		if (gp [1] == T_SPACE) gp [0] = (gp [0] & 0xF8) | gv;
		gp += 4;
	}
	sp_Invalidate (&clip_full, &clip_full);
}

/* show screen ss (SCR_* from gfx/screens): depack it into SCRBUF (768
   tiles, then 768 attributes) and copy it into the display list. That's
   much faster than 768 sp_PrintAtInv; then the whole screen is marked
   for redrawing. */
unsigned char ss;
void show_screen (void) {
	sp_ZX0 ((void *) scr_addr [ss], scrbuf);
	gp = dl; tw_p = scrbuf;
	for (gw = 0; gw < 768; ++ gw) {
		gp [0] = tw_p [768]; gp [1] = *tw_p ++;
		gp += 4;
	}
	sp_Invalidate (&clip_full, &clip_full);
}

/* ------------------------------------------------------------------ */
/* TEXT. Like PRINT: a cursor (tx_row, tx_col) that moves on, wraps at  */
/* column 32, and 13 (a line break in texts.txt) moves to the next row. */
/* ------------------------------------------------------------------ */

unsigned char tx_row, tx_col, tx_attr, tx_ch, tx_wrapped;
unsigned char *tx_s;
unsigned int  tx_num;
unsigned char tx_dig [6];

void newline (void) { tx_col = 0; ++ tx_row; }

/* character tx_ch at the cursor, which moves on */
void put_ch (void) {
	/* a line break right after a full line: the line already broke */
	if (tx_ch == 13) { if (!tx_wrapped) newline (); tx_wrapped = 0; return; }
	tx_wrapped = 0;
	cr = tx_row; cc = tx_col; c_tile = tx_ch; c_attr = tx_attr; cell_put ();
	if (++ tx_col == 32) { newline (); tx_wrapped = 1; }
}

/* text tx_s at the cursor, at once, wrapping at any character (as
   PRINT does). Call sp_UpdateNow to see it. */
void print_raw (void) {
	while (tx_ch = *tx_s) { put_ch (); ++ tx_s; }
}

/* print_raw at (row r, column c) with attribute a */
#define PRINT_AT(r, c, a, t)  { tx_row = (r); tx_col = (c); tx_attr = (a); tx_s = (t); print_raw (); }

/* the original's typewriter (GO SUB 9999): a character at a time, a
   click each, a long beep on each full stop. Unlike the original, it
   wraps whole words (with type_wrap = 1) instead of cutting them.
   type_fast = 1 prints at once, with the same word wrapping.          */
unsigned char type_fast, tw_len;
void type_text (void) {
	while (tx_ch = *tx_s) {
		if (tx_ch == ' ') {
			/* does the next word fit on this line? */
			tw_p = tx_s; tw_len = 0;
			while (1) { ++ tw_p; gv = *tw_p; if (gv <= ' ') break; ++ tw_len; }
			gv = tx_col + tw_len;
			if (gv > 31) { newline (); ++ tx_s; continue; }
			if (tx_col == 0 && tx_wrapped) { ++ tx_s; continue; }
		}
		put_ch ();
		++ tx_s;
		if (type_fast) continue;
		sp_UpdateNow ();
		if (tx_ch == '.') { bp_n = P(10); bp_ms = 500; }
		else { bp_n = P(-10); bp_ms = 10; }
		beep ();
	}
	sp_UpdateNow ();
}

#define TYPE_AT(r, c, a, t)  { tx_row = (r); tx_col = (c); tx_attr = (a); tx_s = (t); type_text (); }

/* number tx_num at the cursor */
void print_num (void) {
	gi = 0;
	do { tx_dig [gi ++] = '0' + tx_num % 10; tx_num /= 10; } while (tx_num);
	while (gi) { tx_ch = tx_dig [-- gi]; put_ch (); }
}

/* row r blank with attribute a */
void clear_row (void) {
	for (cc = 0; cc < 32; ++ cc) { c_tile = T_SPACE; cell_put (); }
}
#define CLEAR_ROW(r, a)  { cr = (r); c_attr = (a); clear_row (); }

/* the bottom two rows (the original's "PRINT #0", its lower screen):
   cleared, then text tx_s in attribute tx_attr                        */
#define LOWER  (PAPER(0) | INK(7))
unsigned char msg_tf;
void msg (void) {
	CLEAR_ROW (22, LOWER); CLEAR_ROW (23, LOWER);
	tx_row = 22; tx_col = 0;
	msg_tf = type_fast; type_fast = 1; type_text (); type_fast = msg_tf;
}
#define MSG(a, t)  { tx_attr = (a); tx_s = (t); msg (); }

/* the lower rows' colour: random ink, as the original's INK RND*6 */
void msg_flicker (void) {
	rect.row_coord = 22; rect.col_coord = 0; rect.height = 2; rect.width = 32;
	c_attr = PAPER(0) | INK(1 + rnd () % 6);
	paint_attr ();
}

/* the Ghost-tipo logo (UDG A B over C D) at (cr, cc), red on paper p */
unsigned char lg_paper;
void logo (void) {
	c_attr = lg_paper | INK(2);
	c_tile = T_LOGO_A; cell_put (); ++ cc; ++ c_tile; cell_put ();
	++ cr; -- cc; ++ c_tile; cell_put (); ++ cc; ++ c_tile; cell_put ();
	-- cr; -- cc;
}

/* "Title          AB" / "               CD": the header of each part,
   with the logo at column lg_col */
unsigned char lg_col;
void header (void) {
	tx_row = 0; tx_col = 0; print_raw ();
	cr = 0; cc = lg_col; logo ();
}
#define HEADER(t, col, paper)  { tx_s = (t); tx_attr = (paper); lg_paper = (paper); lg_col = (col); header (); }

/* ------------------------------------------------------------------ */
/* SPRITES (frames from gfx/sprites.png, 16x16)                         */
/* ------------------------------------------------------------------ */

void spr_put (void) {
	gp = sprite_frames + frame_offsets [s_f];
	sp_MoveSprAbs (spr [s_n], &clip_full, gp - spr_cur [s_n], s_y >> 3, s_x >> 3, s_x & 7, s_y & 7);
	spr_cur [s_n] = gp;
}

/* out of sight: outside the clip rectangle (column 34) */
void spr_hide (void) {
	sp_MoveSprAbs (spr [s_n], &clip_full, 0, 0, 34, 0, 0);
}

void hide_all (void) {
	s_n = 0; spr_hide (); s_n = 1; spr_hide ();
}

/* sprite s_n on plane s_f (lower planes are drawn on top). A 16 px
   sprite needs 3 columns of characters to be shifted 0-7 px; the third
   one only receives what is shifted out of the second, so it points at
   SPNullSprPtr and frames don't need to store a blank third column.  */
void spr_create (void) {
	spr [s_n] = sp_CreateSpr (sp_MASK_SPRITE, 3, sprite_frames, s_f);
	sp_AddColSpr (spr [s_n], sprite_frames + 48);
	sp_AddColSpr (spr [s_n], sprite_frames + 48);
	gp = (unsigned char *) spr [s_n];
	gw = (gp [6] << 8) | gp [7];            /* first char struct (big endian) */
	for (gi = 0; gi < 9; gi ++) {
		gp = (unsigned char *) gw;
		if (gi >= 6) { gp [7] = null_ptr & 255; gp [8] = null_ptr >> 8; }
		gw = (gp [0] << 8) | gp [1];         /* next char struct              */
	}
	spr_cur [s_n] = sprite_frames;
}

/* ------------------------------------------------------------------ */
/* WHAT A CELL IS, for moving around. Each part sets `kind`.           */
/* ------------------------------------------------------------------ */

#define C_FREE     0
#define C_SOLID    1
#define C_DEADLY   2
#define C_LADDER   3

#define K_STREET   0           /* roads (blank, white paper) are free      */
#define K_ROOM     1           /* T walls are solid, ^ kills               */
#define K_PLAT     2           /* Gregory: S T solid, F ladder, ^ kills    */
unsigned char kind;
unsigned char max_row;         /* lowest row you can be on                 */

/* class of cell (cr, cc); outside the play area is solid */
unsigned char cell_class (void) {
	if (cr > max_row || cc > 31) return C_SOLID;
	cell_get ();
	if (kind == K_STREET) {
		if (c_tile == T_SPACE && (c_attr & 0x38) == PAPER(7)) return C_FREE;
		return C_SOLID;
	}
	if (c_tile == T_SPIKE) return C_DEADLY;
	if (c_tile == T_T) return C_SOLID;
	if (kind == K_PLAT) {
		if (c_tile == T_S) return C_SOLID;
		if (c_tile == T_F) return C_LADDER;
	}
	return C_FREE;
}

/* ------------------------------------------------------------------ */
/* TOP VIEW MOVEMENT (street, shop, the jefe's chamber, the ayudante's  */
/* rooms, the sea). The original moved a character per key press; here  */
/* you glide from cell to cell, WALK_SPEED px per frame, and choose a   */
/* direction each time you're exactly on a cell. That keeps you lined   */
/* up with 1-character gaps without having to aim pixel-perfectly.     */
/* ------------------------------------------------------------------ */

unsigned char pl_x, pl_y;          /* pixels (top left)                     */
unsigned char pl_dir;              /* 0 still, 1 left, 2 right, 3 up, 4 down */
unsigned char pl_big;              /* 0: 8x8 (the little man), 1: 16x16     */
unsigned char pl_speed;
unsigned char pl_face;             /* F_RIGHT or F_LEFT                     */
unsigned char pl_frame;

/* can we go from here one cell in direction gk? (cells on the edge
   ahead: 1 or 2 of them; deadly cells don't stop you) */
unsigned char td_free (void) {
	if (gk == 1) { if (pl_x == 0) return 0; cc = (pl_x >> 3) - 1; cr = pl_y >> 3; }
	else if (gk == 2) { cc = (pl_x >> 3) + 1 + pl_big; cr = pl_y >> 3; }
	else if (gk == 3) { if (pl_y == 0) return 0; cr = (pl_y >> 3) - 1; cc = pl_x >> 3; }
	else { cr = (pl_y >> 3) + 1 + pl_big; cc = pl_x >> 3; }
	if (cell_class () == C_SOLID) return 0;
	if (pl_big) {
		if (gk < 3) ++ cr; else ++ cc;
		if (cell_class () == C_SOLID) return 0;
	}
	return 1;
}

void td_move (void) {
	if ((pl_x & 7) == 0 && (pl_y & 7) == 0) {
		pl_dir = 0;
		if (k_up) { gk = 3; if (td_free ()) pl_dir = 3; }
		if (!pl_dir && k_down) { gk = 4; if (td_free ()) pl_dir = 4; }
		if (!pl_dir && k_left) { gk = 1; pl_face = F_LEFT; if (td_free ()) pl_dir = 1; }
		if (!pl_dir && k_right) { gk = 2; pl_face = F_RIGHT; if (td_free ()) pl_dir = 2; }
	}
	if (pl_dir == 1) pl_x -= pl_speed;
	else if (pl_dir == 2) pl_x += pl_speed;
	else if (pl_dir == 3) pl_y -= pl_speed;
	else if (pl_dir == 4) pl_y += pl_speed;
	/* walking frame: the step every other half cell */
	pl_frame = pl_face;
	if (pl_dir && ((pl_x + pl_y) & 4)) ++ pl_frame;
}

/* just arrived on a cell? (moving and lined up) */
unsigned char td_arrived (void) {
	if (pl_dir && (pl_x & 7) == 0 && (pl_y & 7) == 0) return 1;
	return 0;
}

/* the cell under the middle of the little man */
void pl_cell (void) {
	cr = (pl_y + 4) >> 3; cc = (pl_x + 4) >> 3;
}

void draw_player (void) {
	s_n = 0; s_f = pl_frame; s_x = pl_x; s_y = pl_y; spr_put ();
}

/* one frame: draw, wait, read the keys */
void frame (void) {
	sp_UpdateNow ();
	wait_frame ();
	read_keys ();
}

/* ------------------------------------------------------------------ */
/* MUSIC: the tunes of lines 3020-3090 and 3580-3600, BEEP .3 each.    */
/* 100 = PAUSE 5, 101 = end.                                           */
/* ------------------------------------------------------------------ */

unsigned char tune_intro [] = {
	0, 0, 100, 5, 4, 2, 100, 4, 0, 100, 5, 5, 5, 5, 5, 5, 100 };
unsigned char tune_end [] = {
	0, 2, 4, 5, 7, 5, 4, 3, 100, 7, 5, 4, 2, 5, 4, 2, 0, 101 };
unsigned char *tune_p;
void play_tune (void) {
	while (*tune_p != 101) {
		if (*tune_p == 100) { pause_n = 5; pause50 (); }
		else { bp_n = P(0) + *tune_p; bp_ms = 300; beep (); }
		++ tune_p;
	}
}

/* ------------------------------------------------------------------ */
/* DEATH: the little man falls over, a sad sound, a message, a key.     */
/* ------------------------------------------------------------------ */

void player_dies (void) {
	pl_frame = F_DEAD; draw_player (); sp_UpdateNow ();
	for (gi = 30; gi > 0; gi -= 3) { bp_n = P(gi); bp_ms = 30; beep (); }
	MSG (PAPER(0) | INK(7), tx_s);
	sp_UpdateNow ();
	wait_key ();
}

/* ================================================================== */
/* THE INTRO (lines 10-140)                                            */
/* ================================================================== */

#define INTRO_ATTR  (PAPER(0) | INK(7))

void intro (void) {
	border = BLACK; set_border ();
	c_attr = INTRO_ATTR; cls ();
	type_fast = 0;
	TYPE_AT (0, 0, INTRO_ATTR, t_welcome);
	TYPE_AT (4, 0, INTRO_ATTR, t_copyright);
	TYPE_AT (6, 0, INTRO_ATTR, t_by);
	TYPE_AT (7, 0, INTRO_ATTR, t_drive);
	TYPE_AT (10, 0, INTRO_ATTR, t_fire);
	bp_n = P(10); bp_ms = 200; beep ();
	pause_n = 100; pause50 ();
	TYPE_AT (13, 0, INTRO_ATTR, t_aqui);
	bp_n = P(10); bp_ms = 200; beep ();
	pause_n = 200; pause50 ();
	pause_n = 50; pause50 ();
	TYPE_AT (18, 0, INTRO_ATTR, t_graficos);

	/* the Ghost-tipo */
	cr = 19; cc = 0; lg_paper = PAPER(0); logo ();
	sp_UpdateNow ();
	pause_n = 20; pause50 ();
	TYPE_AT (21, 0, INTRO_ATTR, t_es_tipo);

	/* you: O over P, green. BUG: the original set OVER 1 here (for the
	   accent of "tú") and typed "Ese eres tu" over "Es el Ghost-tipo",
	   which came out as garbage. Here the caption is cleared first.   */
	pause_n = 20; pause50 ();
	cr = 19; cc = 0; c_attr = PAPER(0) | INK(4);
	c_tile = T_O; cell_put (); ++ cr; ++ c_tile; cell_put ();
	cc = 1; c_tile = T_SPACE; cell_put (); -- cr; cell_put ();
	CLEAR_ROW (21, INTRO_ATTR);
	pause_n = 50; pause50 ();
	TYPE_AT (21, 0, INTRO_ATTR, t_eres_tu);

	/* the Ghost "a secas": R, magenta */
	pause_n = 20; pause50 ();
	cr = 19; cc = 0; c_tile = T_SPACE; cell_put ();
	cr = 20; c_tile = T_R; c_attr = PAPER(0) | INK(3); cell_put ();
	CLEAR_ROW (21, INTRO_ATTR);
	pause_n = 50; pause50 ();
	TYPE_AT (21, 0, INTRO_ATTR, t_es_secas);
	pause_n = 50; pause50 ();
	MSG (LOWER, t_pulsa_algo);
	sp_UpdateNow ();
	wait_key ();
}

/* ================================================================== */
/* THE STREET (170-390): drive the Ghost-tipo to your house, the      */
/* flashing one (avenue 1, street 2), and press FIRE.                 */
/* ================================================================== */

void street (void) {
	ss = SCR_STREET; show_screen ();
	gv = 2; recolour_blank ();          /* the car is red, as printed with INK 2 */

	/* your house: PAPER RND*6 (line 270). BUG: the intro says to go to the
	   flashing buildings, but nothing flashed: now your house does. */
	gv = rnd () % 7;
	for (cr = 7; cr < 12; ++ cr)
		for (cc = 0; cc < 4; ++ cc) {
			cell_get ();
			if ((c_attr & 0x38) == PAPER(5)) {
				gk = INK(7); if (gv >= 4) gk = INK(0);
				if (c_tile >= T_LOGO_A && c_tile < T_LOGO_A + 4) gk = INK(2);
				c_attr = FLASH | PAPER(gv) | gk; cell_put ();
			}
		}
	MSG (LOWER, t_street_home);
	PRINT_AT (23, 0, LOWER, t_street_joy);

	kind = K_STREET; max_row = 21;
	pl_x = 240; pl_y = 56; pl_big = 1; pl_speed = WALK_SPEED; pl_dir = 0; pl_face = F_RIGHT;
	gj = 0;                              /* 1 while at the house           */
	read_keys ();                /* not the keys of before */
	while (1) {
		td_move ();
		s_n = 0; s_f = F_CAR; s_x = pl_x; s_y = pl_y; spr_put ();
		frame ();
		/* at the house: avenue 1 (x <= 5 chars), on the street of row 7
		   or row 14 (line 380). BUG: the original went in without FIRE,
		   though the intro says "Cuando llegues pulsa FIRE".          */
		if (pl_x <= 40 && (pl_y == 56 || pl_y == 112)) {
			if (!gj) { gj = 1; CLEAR_ROW (23, LOWER); PRINT_AT (23, 0, LOWER, t_street_arrived); }
			if (k_fire_new) break;
		} else if (gj) {
			gj = 0; CLEAR_ROW (23, LOWER); PRINT_AT (23, 0, LOWER, t_street_joy);
		}
	}
	hide_all ();
}

/* ================================================================== */
/* GHOST-CASA, the shop (1000-1120). The cursor (the logo) goes up and */
/* down with 9 / 8; FIRE on a price buys; E leaves.                    */
/* ================================================================== */

void show_money (void) {
	CLEAR_ROW (22, LOWER);
	PRINT_AT (22, 0, LOWER, t_shop_money);
	tx_num = money; print_num ();
}

void shop (void) {
	ss = SCR_SHOP; show_screen ();
	gv = 2; recolour_blank ();          /* the cursor is red                    */
	CLEAR_ROW (23, LOWER);
	money = 10000;
	show_money ();
	/* the cursor: column 2, rows 8 to 20 (BUG: the original allowed row 21,
	   and printing its lower half on row 22 stopped the program) */
	pl_x = 16; pl_y = 64; pl_dir = 0;
	read_keys ();                /* not the keys of before */
	while (1) {
		if ((pl_y & 7) == 0) {
			pl_dir = 0;
			if (k_down && pl_y < 160) pl_dir = 4;
			if (k_up && pl_y > 64) pl_dir = 3;
		}
		if (pl_dir == 4) pl_y += 2;
		if (pl_dir == 3) pl_y -= 2;

		/* BUG: the original bought once per loop while 0 was held, so a
		   press could buy several. Here a press buys once. */
		if (k_fire_new && (pl_y == 64 || pl_y == 80)) {
			if (pl_y == 64) gw = 900; else gw = 100;     /* caza-busters, caja */
			bp_n = P(10); bp_ms = 200; beep ();
			if (money < gw) {
				/* in the red: start again from the very beginning (RUN) */
				CLEAR_ROW (22, PAPER(7) | INK(2));
				PRINT_AT (22, 0, PAPER(7) | INK(2), t_shop_red);
				sp_UpdateNow ();
				for (gk = 61; gk; -- gk) { bp_n = P(gk - 1); bp_ms = 10; beep (); }
				CLEAR_ROW (22, LOWER);
				PRINT_AT (22, 0, LOWER, t_shop_key);
				sp_UpdateNow ();
				wait_key ();
				restart_game = 1;
				break;
			}
			money -= gw;
			show_money ();
		}
		/* " Pulse La letra E para salir", in a random ink each loop (the
		   original's INK RND*7 could be yellow on yellow: not here) */
		PRINT_AT (21, 0, PAPER(6) | INK(rnd () % 6), t_shop_exit);
		if (KEY_E) break;

		s_n = 0; s_f = F_CAR; s_x = pl_x; s_y = pl_y; spr_put ();
		frame ();
	}
	hide_all ();
}

/* ================================================================== */
/* THE SONG (3000-3100) and the PRUEBA (3200-3290)                     */
/* ================================================================== */

void song (void) {
	ss = SCR_LOGO; show_screen ();
	sp_UpdateNow ();
	tune_p = tune_intro; play_tune ();
	tune_p = tune_end; play_tune ();
}

/* returns 1 if you hit the ghost */
#define PRUEBA_ATTR  (PAPER(7) | INK(0))
#define TRACY_X      224                 /* column 28                     */
unsigned char gh_x, gh_y, gh_f;

unsigned char prueba (void) {
	c_attr = PRUEBA_ATTR; cls ();
	HEADER (t_prueba_title, 18, PRUEBA_ATTR);
	type_fast = 1;
	TYPE_AT (2, 0, PRUEBA_ATTR, t_prueba);
	type_fast = 0;
	wait_key ();
	/* BUG: the original cleared rows 3-10, so the first line of the text
	   (row 2) stayed on screen. */
	for (cr = 2; cr <= 10; ++ cr) { c_attr = PRUEBA_ATTR; clear_row (); }
	gv = 3; recolour_blank ();            /* the ghost is magenta         */
	/* Tracy, red: M over N */
	cr = 16; cc = 28; c_attr = PAPER(7) | INK(2); c_tile = T_M; cell_put ();
	cr = 17; c_tile = T_M + 1; cell_put ();

	/* the ghost walks along row 10. BUG: the original printed " R" at
	   column n, so the ghost was at n+1 while the hit test used n: the
	   shot hit when the ghost had already passed Tracy, and missed when
	   it was right above him. It also printed n on row 21 (a debugging
	   leftover). Here the shot hits if the ghost is over column 28.   */
	gh_x = 0; gh_y = 80;
	read_keys ();                /* not the keys of before */
	while (1) {
		s_n = 1; s_f = F_GHOST + ((gh_x >> 3) & 1); s_x = gh_x; s_y = gh_y; spr_put ();
		frame ();
		if ((gh_x & 7) == 0) { bp_n = P(0); bp_ms = 20; beep (); }
		if (k_fire_new) break;
		gh_x += GHOST_SPEED;
		if (gh_x > 248) gh_x = 0;
	}
	/* the shot: a beam up column 28 */
	for (cr = 15; cr >= 11; -- cr) {
		cc = 28; c_tile = T_F; c_attr = PAPER(7) | INK(2); cell_put ();
		sp_UpdateNow ();
		bp_n = P(30); bp_ms = 2; beep (); bp_n = P(20); beep ();
	}
	if (gh_x + 6 >= TRACY_X && gh_x <= TRACY_X + 6) {
		/* hit: the ghost becomes letters and is gone */
		s_n = 1; spr_hide ();
		for (gk = 32; gk <= 100; ++ gk) {
			cr = 10; cc = 28; c_tile = gk; c_attr = PRUEBA_ATTR; cell_put ();
			sp_UpdateNow ();
			bp_n = P(gk >> 1); bp_ms = 10; beep ();
		}
		wait_key ();
		return 1;
	}
	/* missed: the ghost comes down and walks to you */
	while (gh_y < 128) { gh_y += 2; s_n = 1; s_y = gh_y; spr_put (); frame (); }
	for (gh_x = 0; gh_x < TRACY_X; gh_x += 2) {
		s_n = 1; s_f = F_GHOST + ((gh_x >> 3) & 1); s_x = gh_x; s_y = gh_y; spr_put ();
		frame ();
	}
	PRINT_AT (19, 0, PAPER(4) | INK(7), t_mueres);
	sp_UpdateNow ();
	wait_key ();
	hide_all ();
	return 0;
}

/* ================================================================== */
/* GHOST-TRAMPA (3410-3610): night falls on the city                   */
/* ================================================================== */

/* the GHOST EMBUSTES block letters (line 3550) */
void block_title (void) {
	PRINT_AT (16, 0, tx_attr, t_block_title);
}

void trampa (void) {
	/* BUG: the original printed this over whatever was on the screen
	   (the Prueba). The screen gfx/screens/04-city.png is clean.     */
	ss = SCR_CITY; show_screen ();
	PRINT_AT (10, 0, PAPER(7) | INK(0), t_noche);
	sp_UpdateNow ();
	pause_n = 100; pause50 ();

	/* night: rows 2-21 dark blue (PRINT OVER 1; PAPER 0; INK 1) */
	rect.row_coord = 2; rect.col_coord = 0; rect.height = 20; rect.width = 32;
	c_attr = PAPER(0) | INK(1); paint_attr ();
	/* BUG: the original's text wrapped onto row 10 and left the end of
	   "Se hace denoche..." showing after it */
	CLEAR_ROW (9, PAPER(0) | INK(7)); CLEAR_ROW (10, PAPER(0) | INK(7));
	type_fast = 1;
	TYPE_AT (9, 0, PAPER(0) | INK(7), t_corridas);
	type_fast = 0;
	sp_UpdateNow ();
	pause_n = 50; pause50 ();

	/* a ghost rises at column 0. BUG: the original then printed a space
	   in white paper at (7, 0), leaving a white square. */
	gv = 3; recolour_blank ();
	for (gh_y = 160; gh_y >= 56; gh_y -= 2) {
		s_n = 1; s_f = F_GHOST + ((gh_y >> 3) & 1); s_x = 0; s_y = gh_y; spr_put ();
		frame ();
		if ((gh_y & 7) == 0) { bp_n = P(gh_y >> 3); bp_ms = 20; beep (); }
	}
	hide_all ();
	CLEAR_ROW (9, PAPER(0) | INK(7)); CLEAR_ROW (10, PAPER(0) | INK(7));
	PRINT_AT (9, 0, PAPER(0) | INK(7), t_gente);
	sp_UpdateNow ();
	pause_n = 50; pause50 ();

	/* the people are scared: the lights go on, yellow windows */
	for (cc = 1; cc <= 30; ++ cc) {
		if (cc == 17) continue;
		c_tile = T_K; c_attr = PAPER(0) | INK(6);
		cr = 4; cell_put (); cr = 5; cell_put (); cr = 6; cell_put ();
		sp_UpdateNow ();
		bp_n = P(30); bp_ms = 100; beep ();
	}
	bp_n = P(30); bp_ms = 1000; beep ();
	CLEAR_ROW (9, PAPER(0) | INK(7));
	PRINT_AT (9, 0, PAPER(0) | INK(7), t_socorro);
	PRINT_AT (10, 5, PAPER(0) | INK(7), t_quien);
	sp_UpdateNow ();
	pause_n = 100; pause50 ();

	/* GHOST EMBUSTES in block letters, invisible (ink = paper = black),
	   shown a column at a time in random colours */
	tx_attr = PAPER(0) | INK(0); block_title ();
	sp_UpdateNow ();
	bp_n = P(0); bp_ms = 1000; beep ();
	rect.row_coord = 16; rect.height = 3; rect.width = 1;
	for (gk = 0; gk < 32; ++ gk) {
		rect.col_coord = gk;
		c_attr = PAPER(0) | INK(rnd () % 7); paint_attr ();
		sp_UpdateNow ();
		bp_n = P(gk); bp_ms = 10; beep ();
	}
	/* then it flashes white. BUG (cosmetic): the original ended with the
	   letters black again, invisible; here they stay white. */
	pause_n = 50; pause50 ();
	rect.col_coord = 0; rect.width = 32;
	for (gk = 0; gk < 6; ++ gk) {
		c_attr = PAPER(0) | INK(7); paint_attr (); sp_UpdateNow ();
		bp_n = P(gk); bp_ms = 100; beep ();
		pause_n = 20; pause50 ();
		if (gk == 5) break;
		c_attr = PAPER(0) | INK(0); paint_attr (); sp_UpdateNow ();
	}
	tune_p = tune_end; play_tune ();
	PRINT_AT (21, 0, PAPER(0) | INK(7), t_pulsa_tecla);
	sp_UpdateNow ();
	wait_key ();
}

/* ================================================================== */
/* GREGORY (3620-4170): three platform screens.                        */
/*                                                                     */
/* BUG: the original had no gravity: you could fly anywhere and go     */
/* through walls (it tested the walls with SCREEN$, which never        */
/* recognises UDGs). Its own code shows what was meant: a jump (key 0, */
/* line 9200), a moving platform to ride over the spikes, poles to     */
/* climb (the F characters), and falling kills ("Caes. Mueres."). That */
/* is what this does:                                                  */
/*   - S and T are solid, F are ladders (up/down), ^ kills;            */
/*   - FIRE jumps forward, 3 characters up and 6 across, like 9200;    */
/*   - falling more than FALL_LIMIT px kills.                          */
/* ================================================================== */

#define PLAT_Y      88                 /* the platform: row 11              */
#define PLAT_MIN    128                /* columns 16 ...                    */
#define PLAT_MAX    224                /* ... to 28                         */

unsigned char pf_state;                /* 0 ground, 1 jumping, 2 falling, 3 climbing */
unsigned char pf_jump;                 /* frame of the jump                 */
unsigned char pf_from;                 /* y where we left the ground        */
unsigned char pf_air_dir;              /* forward while jumping: 0, 1 left, 2 right */
unsigned char plat_on, plat_x, plat_dir;   /* the platform (screen 1)       */
unsigned char pr, pc;                  /* scratch for the tests             */

/* is pixel (pc, pr) in a solid cell? */
unsigned char pf_solid_px (void) {
	cr = pr >> 3; cc = pc >> 3;
	if (cell_class () == C_SOLID) return 1;
	return 0;
}

/* standing on something? (y must be a multiple of 8) */
unsigned char pf_ground (void) {
	if (pl_y & 7) return 0;
	if (plat_on && pl_y + 8 == PLAT_Y && pl_x + 7 >= plat_x && pl_x <= plat_x + 7) return 2;
	cr = (pl_y >> 3) + 1;
	for (gi = 0; gi < 2; ++ gi) {
		if (gi) cc = (pl_x + 7) >> 3; else cc = pl_x >> 3;
		gk = cell_class ();
		if (gk == C_SOLID) return 1;
		if (gk == C_LADDER) {
			/* the top of a ladder is a floor */
			-- cr; if (cell_class () != C_LADDER) return 1; ++ cr;
		}
	}
	return 0;
}

/* can the player's box move to x = pc (rows of pl_y)? */
unsigned char pf_free_x (void) {
	pr = pl_y; if (pf_solid_px ()) return 0;
	pr = pl_y + 7; if (pf_solid_px ()) return 0;
	return 1;
}

/* the ladder column under the player's middle, if any (sets cc) */
unsigned char pf_on_ladder (void) {
	cc = (pl_x + 4) >> 3;
	cr = pl_y >> 3; if (cell_class () == C_LADDER) return 1;
	cr = (pl_y + 7) >> 3; if (cell_class () == C_LADDER) return 1;
	return 0;
}

/* one frame of the player in Gregory. Returns 1 if he died. */
unsigned char pf_move (void) {
	/* the platform carries whoever stands on it */
	if (pf_state == 0 && pf_ground () == 2) {
		if (plat_dir) { pc = pl_x + 8; if (pf_free_x ()) ++ pl_x; }
		else if (pl_x) { pc = pl_x - 1; if (pf_free_x ()) -- pl_x; }
	}

	if (pf_state == 3) {
		/* climbing: cell to cell, like the top view */
		if ((pl_y & 7) == 0) {
			pl_dir = 0;
			cc = pl_x >> 3;
			if (k_up) { cr = pl_y >> 3; if (cell_class () == C_LADDER) pl_dir = 3; }
			if (k_down) { cr = (pl_y >> 3) + 1; if (cell_class () == C_LADDER) pl_dir = 4; }
			/* not climbing on: standing on something (the floor, the top
			   of the ladder), or walking away, or no ladder left: off */
			if (!pl_dir && (k_left || k_right || pf_ground () || !pf_on_ladder ())) pf_state = 0;
		}
		if (pl_dir == 3) pl_y -= 2;
		if (pl_dir == 4) pl_y += 2;
		pl_frame = F_CLIMB + ((pl_y >> 2) & 1);
		if (pf_state == 3) return 0;
	}

	if (pf_state == 0) {
		if (!pf_ground ()) { pf_state = 2; pf_from = pl_y; pf_air_dir = 0; }
		else {
			/* on a ladder and up/down: climb (lined up with it) */
			if ((k_up || k_down) && pf_on_ladder ()) {
				pl_x = cc << 3; pf_state = 3; pl_dir = 0; return 0;
			}
			if (k_down) {
				cc = (pl_x + 4) >> 3; cr = (pl_y >> 3) + 1;
				if (cell_class () == C_LADDER) { pl_x = cc << 3; pf_state = 3; pl_dir = 0; return 0; }
			}
			if (k_fire_new) {
				pf_state = 1; pf_jump = 0; pf_from = pl_y;
				if (pl_face == F_LEFT) pf_air_dir = 1; else pf_air_dir = 2;
				bp_n = P(20); bp_ms = 10; beep ();
			}
		}
	}

	/* walking (on the ground or falling off a ledge) or jumping forward */
	gk = 0;
	if (pf_state == 1 || pf_air_dir) gk = pf_air_dir;
	else if (k_left) gk = 1;
	else if (k_right) gk = 2;
	if (gk == 1) {
		pl_face = F_LEFT;
		if (pl_x > 1) { pc = pl_x - 2; if (pf_free_x ()) pl_x -= 2; }
	} else if (gk == 2) {
		pl_face = F_RIGHT;
		if (pl_x < 246) { pc = pl_x + 9; if (pf_free_x ()) pl_x += 2; }
	}
	pl_frame = pl_face;
	if (gk && (pl_x & 4)) ++ pl_frame;

	if (pf_state == 1) {
		/* going up: stop at a ceiling */
		gv = jump_tab [pf_jump];
		pr = pl_y - gv;
		pc = pl_x; if (pf_solid_px ()) gv = 0;
		pc = pl_x + 7; if (pf_solid_px ()) gv = 0;
		/* (a jump under a low ceiling still goes forward: that's how you
		   get off the top of the pole in screen 2) */
		pl_y -= gv;
		if (++ pf_jump == JUMP_LEN) pf_state = 2;
	} else if (pf_state == 2) {
		/* falling, 2 px a frame, until there's ground */
		if (pf_ground ()) {
			pf_state = 0; pf_air_dir = 0;
			if (pl_y > pf_from + FALL_LIMIT) { tx_s = t_caes; return 1; }
		} else {
			pl_y += 2;
			if (pl_y >= 176) { tx_s = t_caes; return 1; }
		}
	}

	/* spikes: the cell of the middle or of the feet */
	pc = (pl_x + 4) >> 3;
	cc = pc; cr = (pl_y + 4) >> 3; if (cell_class () == C_DEADLY) { tx_s = t_estas_muerto; return 1; }
	cc = pc; cr = (pl_y + 7) >> 3; if (cell_class () == C_DEADLY) { tx_s = t_estas_muerto; return 1; }
	return 0;
}

/* play Gregory screen gv (1-3). Returns 1 when it's done, 0 if you died. */
unsigned char greg_screen;
unsigned char gregory_screen (void) {
	ss = SCR_GREG1 + greg_screen - 1; show_screen ();
	gv = 4; recolour_blank ();          /* the little man is green (INK 4) */
	CLEAR_ROW (22, LOWER); CLEAR_ROW (23, LOWER);
	kind = K_PLAT; max_row = 23;
	plat_on = 0; pf_state = 0; pf_air_dir = 0; pl_face = F_RIGHT; pl_dir = 0;
	if (greg_screen == 1) {
		pl_x = 0; pl_y = 80;
		plat_on = 1; plat_x = PLAT_MIN; plat_dir = 1;
		/* the platform was red (INK 2): the row it moves along */
		cr = 11; for (cc = 16; cc <= 28; ++ cc) { c_tile = T_SPACE; c_attr = PAPER(0) | INK(2); cell_put (); }
	} else if (greg_screen == 2) {
		/* BUG: the original's title "Gregory, Pantalla 2" was printed and
		   then covered by the screen at once; now it's at the bottom. */
		PRINT_AT (22, 0, LOWER, t_gregory2_title);
		pl_x = 0; pl_y = 56;
	} else {
		pl_x = 8; pl_y = 120;
	}
	read_keys ();                /* not the keys of before */
	while (1) {
		if (plat_on) {
			if (plat_dir) { if (++ plat_x == PLAT_MAX) plat_dir = 0; }
			else if (-- plat_x == PLAT_MIN) plat_dir = 1;
			if ((plat_x & 7) == 0) { bp_n = P(plat_x >> 3); bp_ms = 10; beep (); }
			s_n = 1; s_f = F_PLATFORM; s_x = plat_x; s_y = PLAT_Y; spr_put ();
		}
		if (pf_move ()) { player_dies (); hide_all (); return 0; }
		draw_player ();
		frame ();
		/* the way out of each screen */
		if (greg_screen == 1 && pf_state == 0 && pl_x >= 232) break;
		if (greg_screen == 2 && pl_y <= 16 && pl_x >= 240) break;
		if (greg_screen == 3 && (pl_y <= 24 || (pl_y <= 40 && pl_x >= 240))) break;
	}
	hide_all ();
	return 1;
}

void gregory (void) {
	do {
		c_attr = PAPER(7) | INK(0); cls ();
		HEADER (t_gregory_title, 21, PAPER(7) | INK(0));
		type_fast = 1;
		TYPE_AT (2, 0, PAPER(7) | INK(0), t_gregory);
		type_fast = 0;
		wait_key ();
		/* BUG: dying sent the original back to the very start (RUN, or
		   GO TO 0 because "sigue" was never set for this part), or, in
		   screen 3, into screen 2's code with screen 3 on view. Here you
		   repeat the screen where you died. */
		for (greg_screen = 1; greg_screen <= 3; ++ greg_screen)
			while (!gregory_screen ());
		/* the original asks this even when you've just won */
		c_attr = PAPER(0) | INK(7); cls ();
		PRINT_AT (10, 0, PAPER(0) | INK(7), t_repetir);
		sp_UpdateNow ();
		while (1) {
			if (KEY_S) { gv = 1; break; }
			if (KEY_N) { gv = 0; break; }
		}
	} while (gv);
}

/* ================================================================== */
/* THE JEFE (4180-4240): 5 rooms of spikes, two gaps in each row      */
/* ================================================================== */

void status_line (void) {
	CLEAR_ROW (22, LOWER); CLEAR_ROW (23, LOWER);
	PRINT_AT (22, 0, LOWER, t_status);
	cr = 22; cc = 18; lg_paper = PAPER(0); logo ();
	PRINT_AT (23, 20, LOWER, t_money_sign);
	tx_num = money; print_num ();
}

void jefe (void) {
	c_attr = PAPER(7) | INK(0); cls ();
	TYPE_AT (0, 0, PAPER(7) | INK(0), t_jefe);
	wait_key ();
	kind = K_ROOM; max_row = 21; pl_big = 0; pl_speed = WALK_SPEED;
	stage = 0;
	while (stage < SPIKE_ROOMS) {
		c_attr = PAPER(7) | INK(4); cls ();    /* INK 4: the little man is green */
		for (cr = 0; cr <= 20; cr += 2) {
			for (cc = 0; cc < 32; ++ cc) { c_tile = T_SPIKE; c_attr = PAPER(2) | INK(2); cell_put (); }
			c_tile = T_SPACE; c_attr = PAPER(7) | INK(4);
			cc = rnd () & 31; cell_put ();
			cc = rnd () & 31; cell_put ();
		}
		status_line ();
		pl_x = 0; pl_y = 168; pl_dir = 0; pl_face = F_RIGHT;
		read_keys ();                /* not the keys of before */
		while (1) {
			td_move ();
			draw_player ();
			frame ();
			if (td_arrived ()) {
				bp_n = P(pl_x >> 3); bp_ms = 10; beep ();
				bp_n = P(pl_y >> 3); beep ();
			}
			pl_cell ();
			if (cell_class () == C_DEADLY) {
				/* "Estas muerto." and the five rooms start again (GO TO sigue) */
				tx_s = t_muerto_punto; player_dies ();
				stage = 0; break;
			}
			if (pl_y == 0) { ++ stage; break; }
		}
		hide_all ();
	}
}

/* ================================================================== */
/* THE AYUDANTE (4250-5130). Dying anywhere here starts again from the */
/* first room (GO TO sigue, sigue = 4271).                             */
/* ================================================================== */

/* walls of T all round, with a way in at the bottom (columns 14-17) */
void room_walls (void) {
	for (gi = 0; gi < 32; ++ gi) {
		c_tile = T_T;
		cr = 0; cc = gi; cell_put ();
		cr = 21; cell_put ();
		if (gi < 22) { cr = gi; cc = 0; cell_put (); cc = 31; cell_put (); }
	}
	c_tile = T_SPACE; c_attr = (c_attr & 0x38) | INK(4); cr = 21;
	for (cc = 14; cc < 18; ++ cc) cell_put ();
}

/* the little man at the way in */
void room_start (void) {
	pl_x = 120; pl_y = 168; pl_dir = 0; pl_face = F_RIGHT; pl_big = 0;
	pl_speed = WALK_SPEED; kind = K_ROOM; max_row = 21;
}

/* the mine detector (6000-6120). BUG: the original printed "nada cerca"
   whenever ANY of the four cells around you was empty, which is nearly
   always, even with a mine next to you. Some of its tests also printed
   at random places, and at the edges SCREEN$ (w, -1) stopped the
   program. Now: "Una mina cerca" if a mine is next to you, up, down,
   left or right; "nada cerca" if not. */
#define ROOM1_ATTR  (PAPER(5) | INK(0))
void detector (void) {
	pl_cell ();
	gv = 0;
	-- cr; cell_get (); if (c_tile == T_SPIKE) gv = 1;
	cr += 2; cell_get (); if (c_tile == T_SPIKE) gv = 1;
	-- cr; -- cc; cell_get (); if (c_tile == T_SPIKE) gv = 1;
	cc += 2; cell_get (); if (c_tile == T_SPIKE) gv = 1;
	/* the original printed this over the top wall: the wall is put back
	   first, as "nada cerca" is shorter than "Una mina cerca" */
	for (cc = 0; cc < 14; ++ cc) { cr = 0; c_tile = T_T; c_attr = FLASH | ROOM1_ATTR; cell_put (); }
	if (gv) { PRINT_AT (0, 0, ROOM1_ATTR, t_mina_cerca); }
	else { PRINT_AT (0, 0, ROOM1_ATTR, t_nada_cerca); }
}

/* the fence of room 1: H at row 4 in ink pqs (2 red: live, 1 blue: off),
   or S at row 7 of room 2 (6 yellow: live, 5 cyan: off) */
unsigned char pqs, fence_row, fence_tile, fence_paper;
void draw_fence (void) {
	cr = fence_row; c_tile = fence_tile; c_attr = fence_paper | pqs;
	for (cc = 1; cc <= 30; ++ cc) cell_put ();
}

/* is the little man on the fence row? */
unsigned char on_fence (void) {
	if (pl_y + 7 >= (fence_row << 3) && pl_y <= (fence_row << 3) + 7) return 1;
	return 0;
}

/* returns 1 if you got through, 0 if you died */
unsigned char room1 (void) {
	c_attr = PAPER(5) | INK(4); cls ();
	c_attr = FLASH | ROOM1_ATTR; room_walls ();
	MSG (LOWER, t_minas);
	sp_UpdateNow ();
	for (stage = 0; stage < MINES; ++ stage) {
		bp_ms = 10; bp_n = P(-20); beep ();
		cc = 1 + rnd () % 29; cr = 1 + rnd () % 19;
		bp_n = P(-10); beep (); bp_n = P(0); beep ();
		c_tile = T_SPIKE; c_attr = PAPER(5) | INK(5); cell_put ();
		sp_UpdateNow ();
		bp_n = P(10); beep (); bp_n = P(20); beep ();
	}
	room_start ();
	MSG (LOWER, t_listo);
	stage = 0;                       /* 0 mines, 1 + rocks, 2 + the fence  */
	gh_y = 0;                     /* the rock: 0 = a new one            */
	pqs = 1; fence_row = 4; fence_tile = T_H; fence_paper = PAPER(5);
	tick = 0;
	read_keys ();                /* not the keys of before */
	while (1) {
		td_move ();
		draw_player ();
		if (stage == 1) {
			/* the rocks (4400-4440): one at a time, down a random column */
			if (gh_y == 0) { gh_x = (1 + rnd () % 29) << 3; gh_y = 8; }
			gh_y += ROCK_SPEED;
			if (gh_y >= 168) gh_y = 0;
			else { s_n = 1; s_f = F_ROCK; s_x = gh_x; s_y = gh_y; spr_put (); }
			if (gh_y && gh_x == pl_x && gh_y + 6 >= pl_y && gh_y <= pl_y + 6) {
				tx_s = t_por_favor; player_dies (); hide_all (); return 0;
			}
		}
		frame ();
		if (++ tick == TICK) {
			tick = 0;
			msg_flicker ();
			detector ();
			if (stage == 2) {
				/* the fence changes at random (4460-4470) */
				gv = rnd () % 5;
				if (gv == 1 || gv == 3) pqs = 2;
				if (gv == 2 || gv == 4) pqs = 1;
				draw_fence ();
			}
		}
		pl_cell ();
		if (cell_class () == C_DEADLY) { tx_s = t_muerto_punto; player_dies (); hide_all (); return 0; }
		if (stage == 0 && pl_y <= 144) { stage = 1; MSG (LOWER, t_rocas); }
		if (stage == 1 && pl_y <= 48) {
			stage = 2; s_n = 1; spr_hide ();
			MSG (LOWER, t_valla);
			draw_fence ();
		}
		if (stage == 2 && on_fence ()) {
			if (pqs == 2) { tx_s = t_por_favor; player_dies (); hide_all (); return 0; }
			if (pl_y == 32) break;
		}
	}
	MSG (LOWER, t_pasado1);
	sp_UpdateNow ();
	wait_key ();
	hide_all ();
	return 1;
}

unsigned char room2 (void) {
	c_attr = PAPER(2) | INK(4); cls ();
	c_attr = PAPER(2) | INK(0); room_walls ();
	room_start ();
	MSG (LOWER, t_listo2);
	stage = 0; tick = 0;
	pqs = 6; fence_row = 7; fence_tile = T_S; fence_paper = PAPER(2);
	read_keys ();                /* not the keys of before */
	while (1) {
		td_move ();
		draw_player ();
		frame ();
		if (++ tick == TICK) {
			tick = 0;
			msg_flicker ();
			if (stage == 2) {
				/* BUG: the original tested "w=1 OR e=3" (4680), a typo
				   for e=1, so the fence was live half as often as the
				   first one. */
				gv = rnd () % 5;
				if (gv == 1 || gv == 3) pqs = 6;
				if (gv == 2 || gv == 4) pqs = 5;
				draw_fence ();
			}
		}
		pl_cell ();
		if (cell_class () == C_DEADLY) { tx_s = t_muerto_punto; player_dies (); return 0; }
		if (stage == 0 && pl_y <= 144) {
			/* rows of spikes, one gap each (4600-4610). The walls (T)
			   don't kill, whatever the message says: only the spikes. */
			stage = 1;
			for (cr = 10; cr <= 16; cr += 2) {
				for (cc = 1; cc <= 30; ++ cc) { c_tile = T_SPIKE; c_attr = PAPER(1) | INK(1); cell_put (); }
				cc = 1 + rnd () % 29; c_tile = T_SPACE; c_attr = PAPER(2) | INK(4); cell_put ();
			}
			MSG (LOWER, t_paredes);
		}
		if (stage == 1 && pl_y <= 72) { stage = 2; MSG (LOWER, t_alambrada); draw_fence (); }
		if (stage == 2 && on_fence ()) {
			if (pqs == 6) { tx_s = t_electro; player_dies (); return 0; }
			if (pl_y == 56) break;
		}
	}
	cr = 5; cc = 1 + rnd () % 29; c_tile = T_H; c_attr = PAPER(2) | INK(0); cell_put ();
	MSG (LOWER, t_pasado2);
	sp_UpdateNow ();
	wait_key ();
	hide_all ();
	return 1;
}

/* THE COME-COME (4760-4900): eat all the text before the clock runs out.
   BUG: the original counted the loops (LAP = 22*32), not what you ate,
   and the clock started at 22*32+120: you won by standing still. Here you
   win when no character is left. */
#define COME_ATTR  (PAPER(7) | INK(0))
unsigned int come_left, come_time;
void comecome (void) {
	c_attr = PAPER(7) | INK(4); cls ();
	pl_y = 48; pl_x &= 0xF8;            /* where room 2 left you: row 6   */
	kind = K_ROOM; max_row = 20; pl_dir = 0; pl_speed = WALK_SPEED;
	while (1) {
		tx_row = 0; tx_col = 0; tx_attr = COME_ATTR;
		for (gj = 0; gj < 3; ++ gj) { tx_s = t_come; print_raw (); }
		PRINT_AT (10, 9, PAPER(0) | INK(7), t_pulsa_mayus);
		sp_UpdateNow ();
		wait_key ();
		/* what is left to eat */
		come_left = 0;
		gp = dl;
		for (gw = 0; gw < 21 * 32; ++ gw) {
			/* a character, or a space of PULSA UNA TECLA (inverse) */
			if (gp [1] != T_SPACE || (gp [0] & 0x38) != PAPER(7)) ++ come_left;
			gp += 4;
		}
		come_time = COME_TIME;
		tick = 0;
		read_keys ();                /* not the keys of before */
		while (come_left && come_time) {
			td_move ();
			draw_player ();
			frame ();
			pl_cell ();
			cell_get ();
			if (c_tile != T_SPACE || (c_attr & 0x38) != PAPER(7)) {
				c_tile = T_SPACE; c_attr = PAPER(7) | INK(4); cell_put ();
				-- come_left;
			}
			if (++ tick == TICK) {
				tick = 0; -- come_time;
				CLEAR_ROW (21, COME_ATTR);
				PRINT_AT (21, 0, COME_ATTR, t_time);
				tx_num = come_time; print_num ();
			}
		}
		if (come_left == 0) break;
		PRINT_AT (10, 9, PAPER(0) | INK(7), t_tiempo);
		sp_UpdateNow ();
		wait_key ();
	}
	hide_all ();
	c_attr = COME_ATTR; cls ();
	PRINT_AT (10, 9, PAPER(0) | INK(7), t_conseguiste);
	sp_UpdateNow ();
	bp_n = P(0); bp_ms = 1000; beep ();
	PRINT_AT (21, 0, COME_ATTR, t_pulsa_mayus);
	sp_UpdateNow ();
	wait_key ();
}

/* THE SEA (4910-5130): its ghost, then a field of spikes */
#define SEA_ATTR   (PAPER(1) | INK(0))
unsigned char sea (void) {
	c_attr = PAPER(1) | INK(4); cls ();
	c_attr = SEA_ATTR; room_walls ();
	/* waves (the original drew them with PLOT and DRAW) */
	for (stage = 0; stage < 21; ++ stage) {
		cr = 1 + rnd () % 19; cc = 1 + rnd () % 28;
		c_attr = PAPER(1) | INK(5);
		for (c_tile = T_WAVE; c_tile < T_WAVE + 3; ++ c_tile) { cell_put (); ++ cc; }
	}
	room_start ();
	MSG (LOWER, t_mar);
	stage = 0; tick = 0;
	read_keys ();                /* not the keys of before */
	while (1) {
		td_move ();
		draw_player ();
		if (stage == 1) {
			/* the ghost along row 15, leaving a trail "___" */
			gh_x += GHOST_SPEED;
			if (gh_x > 208) gh_x = 8;
			if ((gh_x & 7) == 0) { cr = 15; cc = gh_x >> 3; c_tile = T_LINE; c_attr = PAPER(1) | INK(5); cell_put (); }
			s_n = 1; s_f = F_GHOST + ((gh_x >> 3) & 1); s_x = gh_x; s_y = 120; spr_put ();
		}
		frame ();
		if (++ tick == TICK) { tick = 0; msg_flicker (); }
		pl_cell ();
		if (cell_class () == C_DEADLY) { tx_s = t_muerto_punto; player_dies (); hide_all (); return 0; }
		if (stage == 0 && pl_y <= 144) {
			stage = 1; gh_x = 8;
			cr = 15; c_tile = T_E; c_attr = PAPER(1) | INK(5);
			for (cc = 1; cc <= 30; ++ cc) cell_put ();
			MSG (LOWER, t_reflejos);
		}
		if (stage == 1 && k_fire_new) {
			/* the original: hit if the ghost is at your column or the one
			   to its right (5020) */
			if (gh_x + 8 >= pl_x && gh_x <= pl_x + 8) {
				MSG (LOWER, t_bien);
				cr = 15; cc = gh_x >> 3; c_tile = T_T; c_attr = SEA_ATTR; cell_put ();
				s_n = 1; spr_hide ();
				sp_UpdateNow ();
				bp_n = P(0); bp_ms = 1000; beep ();
				break;
			}
			/* missed: it runs along your row and gets you */
			MSG (LOWER, t_ohoh);
			for (gh_x = 8; gh_x < 232; gh_x += 4) {
				s_n = 1; s_f = F_GHOST + ((gh_x >> 3) & 1); s_x = gh_x; s_y = pl_y; spr_put ();
				frame ();
			}
			tx_s = t_muerto; player_dies (); hide_all (); return 0;
		}
	}
	/* the field of spikes (5060-5130), in inverse video. BUG: the original
	   could put them on the walls, and on you. */
	c_attr = SEA_ATTR; room_walls ();
	for (stage = 0; stage < SEA_SPIKES; ++ stage) {
		cr = 1 + rnd () % 20; cc = 1 + rnd () % 30;
		gv = (pl_y >> 3) + 1;
		if (cr + 1 >= (pl_y >> 3) && cr <= gv && cc + 1 >= (pl_x >> 3) && cc <= (pl_x >> 3) + 1) continue;
		c_tile = T_SPIKE; c_attr = PAPER(5) | INK(1); cell_put ();
	}
	MSG (LOWER, t_tocas);
	read_keys ();                /* not the keys of before */
	while (1) {
		td_move ();
		draw_player ();
		frame ();
		if (++ tick == TICK) { tick = 0; msg_flicker (); }
		pl_cell ();
		if (cell_class () == C_DEADLY) { tx_s = t_muerto_punto; player_dies (); hide_all (); return 0; }
		if (pl_y == 8) break;
	}
	hide_all ();
	return 1;
}

void ayudante (void) {
	c_attr = PAPER(7) | INK(0); cls ();
	TYPE_AT (0, 0, PAPER(7) | INK(0), t_ayudante);
	newline (); newline (); newline (); newline ();
	tx_s = t_pulsa_tecla2; type_text ();
	wait_key ();
	while (1) {
		if (!room1 ()) continue;
		if (!room2 ()) continue;
		comecome ();
		if (sea ()) break;
	}
}

/* ================================================================== */
/* THE END (5140-5330)                                                  */
/* ================================================================== */

#define END_ATTR  (PAPER(1) | INK(7))
unsigned char choice;              /* 0 Tracy, 1 Jefe, 2 Ayudante */
void the_end (void) {
	c_attr = END_ATTR; cls ();
	TYPE_AT (0, 0, END_ATTR, t_por_fin);
	while (1) {
		if (KEY_T) { choice = 0; break; }
		if (KEY_J) { choice = 1; break; }
		if (KEY_A) { choice = 2; break; }
	}
	newline ();
	tx_s = t_elegido; type_text ();
	if (choice == 0) tx_s = t_tracy;
	else if (choice == 1) tx_s = t_jefe_name;
	else tx_s = t_ayudante_name;
	type_text ();
	tx_s = t_pulsa_tecla_end; type_text ();
	/* the original kept the choice (m$, n$, cs) for the next part, which
	   was never loaded: here it is shown */
	cr = tx_row + 2; cc = 15;
	if (choice == 0) { c_attr = END_ATTR & 0xF8 | INK(2); c_tile = T_M; }
	else if (choice == 1) { c_attr = END_ATTR & 0xF8 | INK(4); c_tile = T_SPACE; }
	else { c_attr = END_ATTR & 0xF8 | INK(3); c_tile = T_M; }
	cell_put ();
	++ cr;
	if (choice == 0) c_tile = T_M + 1;
	else if (choice == 1) c_tile = T_UDG + 20;          /* U */
	else c_tile = T_O + 1;                          /* P */
	cell_put ();
	sp_UpdateNow ();
	wait_key ();

	border = WHITE; set_border ();
	c_attr = PAPER(7) | INK(0); cls ();
	cr = 0; cc = 0; lg_paper = PAPER(7); logo ();
	cc = 30; logo ();
	PRINT_AT (0, 6, PAPER(7) | INK(0), t_ghostbusters);
	type_fast = 1;
	TYPE_AT (5, 0, PAPER(7) | INK(0), t_fin);
	type_fast = 0;
	wait_key ();
	border = BLACK; set_border ();
}

/* ================================================================== */

void game (void) {
	restart_game = 0;
	intro ();
	street ();
	shop ();
	if (restart_game) return;
	do song (); while (!prueba ());
	trampa ();
	gregory ();
	jefe ();
	ayudante ();
	the_end ();
}

main () {
	#asm
		di
	#endasm
	sp_AddMemory (0, NUMBLOCKS, 14, AD_FREE);   /* before sp_RegisterHook! */
	sp_InitIM2 (0xf1f1);
	sp_CreateGenericISR (0xf1f1);
	sp_RegisterHook (255, isr);
	#asm
		ei
		ld  a, r
		ld  (_rnd_seed), a
	#endasm

	dl = (unsigned char *) dl_addr;
	scrbuf = (unsigned char *) SCRBUF;

	for (gi = 0; gi < NTILECODES; ++ gi) sp_TileArray (tile_code [gi], tile_gfx + (gi << 3));

	sp_Initialize (INK(7), T_SPACE);
	border = BLACK; set_border ();

	s_n = 0; s_f = 0; spr_create ();       /* the player: plane 0, in front */
	s_n = 1; s_f = 1; spr_create ();
	hide_all ();

	while (1) game ();
}
