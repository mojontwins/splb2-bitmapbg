/*
 * Classic Japanese Monster Castle '93 (CJMC93) - C port for z88dk
 * (classic, sccz80) + splib2-bitmapbg.
 *
 * Original: LOKO SOFT 1993, Sinclair BASIC compiled with ERE's compiler
 * (tape 1 of 2). The program was decompiled back into BASIC
 * (orig/cjmc93.bas.txt, see tools/listing.py) and rewritten here. GAME.md
 * describes the game section by section; the section names (1.4, 3.1,
 * GUARDIAN...) are used in the comments below too. BUGS.md lists what was
 * wrong in the original and what this port does about it.
 *
 * What changes from the original:
 *   - The original moved everything a character (8 px) at a time by
 *     PRINTing UDGs. Here the player, the enemies and the moving platforms
 *     are masked splib2 sprites that move pixel by pixel, at 25 fps.
 *   - The player jumps along an arc (gravity and acceleration, from the
 *     tables jump_up and fall_speed), reaching the original's 6 characters.
 *   - The backdrop (castle, cave, bridge) is copied whole into splib2's
 *     background buffer (the shadow screen, BITMAPBG): the original did the
 *     same with RANDOMIZE USR 61913. Anything painted on it, the play area
 *     included, stays under the sprites.
 *   - Room contents come from gfx/rooms.txt, painted over the backdrop as
 *     8x8 tiles. Some rooms also show a picture in the game area
 *     (gfx/pictures), unpacked straight into the shadow by pic_unpack.
 *
 * Everything that draws (tiles, texts, rooms, backdrops, pictures, lines,
 * sprites) is in printer.h, in assembly that calls splib2's routines
 * directly.
 *
 * Three functions do the work the original repeated in every room:
 *   room_draw ()    draws any room: backdrop + picture + layout + HUD
 *   actors_move ()  moves every enemy and platform, whatever the room
 *   player_move ()  the player: walking, gravity, jumping, platforms
 *
 * Coding rules for this port: no local variables and no function
 * parameters (only the library calls take them): globals instead.
 *
 * Memory map
 *   24200 - ...     this binary (code + splib2 + data + display list)
 *   ...   - $D3E6   stack (grows down)
 *   $D3E6 - $D7FF   splib2 block memory (NUMBLOCKS * 15)
 *   $D800 - $EFFF   SHADOW: the background bitmap, display-file layout
 *   $F000 - $F100   IM2 vector table
 *   $F1F1 - $F1FF   generic ISR
 *   $F200 - $FFFF   splib2 horizontal rotation tables
 */

#pragma output STACKPTR=54246

#include <spritepack.h>

#define SHADOW      SP_BGSHADOW     /* = BGSHADOW*256 in SPconfig.def      */
#define NUMBLOCKS   70              /* 7 sprites: 65 blocks, + hook, spare */
#define AD_FREE     (SHADOW - NUMBLOCKS * 15)       /* = 54246 = STACKPTR  */

#include "assets.h"

/* ====================================================================== */
/* TUNING. Pixels, and frames of 1/25 s.                                   */
/* ====================================================================== */

#define LIVES           3

#define PLAYER_SPEED    2       /* walking, pixels per frame                */
#define FALL_MAX        6       /* terminal speed (last entry of fall_speed) */

/* The jump. jump_up[] is how many pixels the player rises on each frame of
   a jump: fast at first, slower and slower to the top (the deceleration of
   gravity). Its total, JUMP_HEIGHT, is the height of the jump: 48 px, the
   original's 6 characters (sub 8031 rose one character six times). Change
   the numbers freely; keep JUMP_FRAMES = the number of entries and
   JUMP_HEIGHT = their sum (the room layouts need 48 to be playable). */
#define JUMP_FRAMES     14
#define JUMP_HEIGHT     48
unsigned char jump_up [JUMP_FRAMES] = { 6, 5, 5, 5, 4, 4, 4, 3, 3, 3, 2, 2, 1, 1 };

/* Falling: pixels per frame, frame after frame, from the moment there is
   nothing under the feet (after the top of a jump, or off a ledge). The
   last entry repeats (terminal speed). Same curve as the jump in reverse,
   so a jump is a symmetric arc. */
#define FALL_FRAMES     11
unsigned char fall_speed [FALL_FRAMES] = { 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, FALL_MAX };

#define KNIGHT_SPEED    2       /* 1.4                                      */
#define SHIFTER_SPEED   2       /* 1.5                                      */
#define SHIFT_GLITCH    4       /* frames the "changing" figure shows       */
#define PLATFORM_SPEED  1       /* 2.1, 2.2                                 */
#define BALL_SPEED      2       /* 3.1, 3.2 (each axis)                     */
#define BALLS_KILL      1       /* 0: harmless, as in the original (a bug)  */
#define GUARD_SPEED     2       /* 3.3                                      */
#define GUARDIAN_SPEED  2       /* 4.1                                      */
#define RAY_FRAMES      4       /* how long the guardian ray shows          */
#define SHOT_FRAMES     2       /* how long the player shot shows           */
#define SHOT_WAIT       8       /* frames between shots                     */
#define FLASH_FRAMES    8       /* 1.6: the flashing box (16/50 s)          */

/* the play area, in pixels (sprite top-left corner) */
#define AREA_LEFT       32      /* column 4                                 */
#define AREA_RIGHT      208     /* column 26 (the player is 2 wide)         */
#define AREA_TOP        24      /* row 3: the balls' ceiling                */
#define PLAYER_TOP      0       /* the player can go up into the top bands  */
#define AREA_BOTTOM     144     /* rows 18-19: standing on row 20           */
#define BRIDGE_LEFT     48      /* 4.1: right of the guardian               */
#define BRIDGE_RIGHT    240
#define BRIDGE_TOP      0

/* collision byte of a cell (in the display list, see put_tile) */
#define C_SOLID         1
#define C_DEADLY        2

/* ====================================================================== */
/* splib2 housekeeping                                                     */
/* ====================================================================== */

void *my_malloc (uint bytes) { return sp_BlockAlloc (0); }
void *u_malloc = my_malloc;
void *u_free = sp_FreeBlock;

unsigned char isrc;                     /* interrupts since the last frame */

/* ISR hook: splib2 needs two spare bytes before it */
extern void isr (void);
#asm
	defw 0
._isr
	ld  hl, _isrc
	inc (hl)
	ret
#endasm

/* ====================================================================== */
/* Globals instead of locals and parameters.                               */
/*                                                                         */
/* Who uses what (a function may use the globals of the functions it      */
/* calls, so callers must not keep values there across the call):          */
/*   drawing (printer.h) the table at the top of printer.h                 */
/*   cell_hit ........... bx by bw bh, reads the map                       */
/*   beep ............... bp_n bp_ms bp_count bp_half bp_spk, gp           */
/*   actors_move ........ an + cell_free, rnd                              */
/*   player_move ........ mk + cell_free                                   */
/*   game level ......... gi gj                                            */
/* ====================================================================== */

unsigned char gi, gj, gk;
unsigned int  gw;
unsigned char *gp;

unsigned char t_row, t_col, t_attr, t_tile, t_num;
unsigned char *tp;
unsigned char *lp;
unsigned char lc, lk;
unsigned char *sp_s;
unsigned char s_n, s_f, s_x, s_y;
unsigned char bx, by, bw, bh;
unsigned char mk;
unsigned char wf_n;

unsigned char *dlist;                   /* splib2 display list (4 bytes/cell) */
unsigned char attr_buf [768];           /* backdrop attributes, depacked     */

/* ====================================================================== */
/* Low level                                                               */
/* ====================================================================== */

unsigned char __FASTCALL__ inport (unsigned int port) {
	#asm
		ld  b, h
		ld  c, l
		in  a, (c)
		ld  l, a
		ld  h, 0
	#endasm
}

/* 25 fps: two interrupts since the last call */
void wait_frame (void) {
	while (isrc < 2);
	isrc = 0;
}

/* wf_n interrupts (1/50 s each) */
void wait_frames (void) {
	isrc = 0;
	while (isrc < wf_n);
	isrc = 0;
}

/* 16 bit xorshift */
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

/* RND 1..10, as the original's INT (RND * 10) + 1 */
unsigned char rnd10 (void) {
	return rnd () % 10 + 1;
}

/* keys: O P Q as in the original; 0 and SPACE fire (4.1) */
unsigned char k_left, k_right, k_up, k_fire;

void read_keys (void) {
	gk = inport (0xDFFE);
	k_right = !(gk & 1);
	k_left = !(gk & 2);
	if (k_right && k_left) { k_right = k_left = 0; }
	k_up = !(inport (0xFBFE) & 1);
	k_fire = !(inport (0x7FFE) & 1) || !(inport (0xEFFE) & 1);
}

unsigned char any_key (void) {
	return ((inport (0x00FE) & 0x1F) != 0x1F);
}

/* PAUSE 0: wait for a key (after the keys are released) */
void wait_key (void) {
	while (any_key ());
	while (!any_key ());
}

/* BEEP: bp_ms milliseconds of note bp_n. bp_n is the BASIC's pitch minus
   BEEP_MIN: P(n) for BEEP x, n. (From Ghost Embustes 2.) */
#define P(n)  ((n) - BEEP_MIN)
unsigned char bp_n;
unsigned int  bp_ms, bp_count, bp_half;
unsigned char bp_spk;
void beep (void) {
	gp = beep_tab + (bp_n << 2);
	bp_count = gp [0] | (gp [1] << 8);
	/* half periods = Hz * ms / 500 */
	bp_half = ((unsigned long) (gp [2] | (gp [3] << 8)) * bp_ms) / 500;
	if (bp_half == 0) return;
	bp_spk = 0;                         /* black border */
	#asm
		di
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
		ei
	#endasm
}

/* ====================================================================== */
/* Collision: does the box (bx, by, bw x bh pixels) touch anything?        */
/* Returns the OR of the collision bytes of the cells it covers            */
/* (0 = free, C_SOLID, C_DEADLY).                                          */
/* ====================================================================== */

unsigned char cell_hit (void) {
	#asm
		; B = rows covered = ((by + bh - 1) >> 3) - (by >> 3) + 1
		ld  a, (_by)
		ld  c, a
		ld  a, (_bh)
		dec a
		add a, c
		rrca
		rrca
		rrca
		and 31
		ld  b, a
		ld  a, c
		rrca
		rrca
		rrca
		and 31
		ld  c, a                ; C = first row
		ld  a, b
		sub c
		inc a
		ld  b, a
		; HL = dlist + row * 128 + 1
		ld  l, c
		ld  h, 0
		add hl, hl
		add hl, hl
		add hl, hl
		add hl, hl
		add hl, hl
		add hl, hl
		add hl, hl
		ld  de, (_dlist)
		add hl, de
		inc hl
		; E = columns covered, HL += first col * 4
		ld  a, (_bx)
		ld  c, a
		ld  a, (_bw)
		dec a
		add a, c
		rrca
		rrca
		rrca
		and 31
		ld  e, a
		ld  a, c
		rrca
		rrca
		rrca
		and 31
		ld  c, a
		ld  a, e
		sub c
		inc a
		ld  e, a                ; columns
		ld  a, c
		add a, a
		add a, a
		ld  c, a
		ld  a, b
		ld  b, 0
		add hl, bc
		ld  b, a
		ld  d, 0                ; D = result
	.ch_row
		push hl
		push de
		ld  c, e
	.ch_col
		ld  a, (hl)
		or  d
		ld  d, a
		inc hl
		inc hl
		inc hl
		inc hl
		dec c
		jr  nz, ch_col
		ld  a, d
		pop de
		ld  d, a
		pop hl
		push de
		ld  de, 128
		add hl, de
		pop de
		djnz ch_row
		ld  l, d
		ld  h, 0
	#endasm
}

/* ====================================================================== */
/* Game state                                                              */
/* ====================================================================== */

/* The rooms, in the order they are played (GAME.md has a section for
   each). R_1_S, the box room, is a detour from 1.6. */
#define R_1_1     0
#define R_1_2     1
#define R_1_3     2
#define R_1_4     3
#define R_1_5     4
#define R_1_6     5
#define R_1_S     6
#define R_1_7     7
#define R_1_8     8
#define R_CARD2   9
#define R_2_1     10
#define R_2_2     11
#define R_CARD3   12
#define R_3_1     13
#define R_3_2     14
#define R_3_3     15
#define R_CARD4   16
#define R_4_1     17
#define R_END     18

/* how a room is left */
#define X_LEFT    0     /* reach the left edge (column 4)                   */
#define X_MOUTH   1     /* 1.7: walk into the mouth (column 14)             */
#define X_TOPLEFT 2     /* 2.x: top left, standing on row 5 (y = 4)         */
#define X_NONE    3     /* cards, the guardian, the end: by code            */

/* per room: layout, start (column, row of the feet, as the original's x
   and y), exit, next room */
unsigned char room_layout [] = { L_1_1, L_1_2, L_1_3, L_1_4, L_1_5, L_1_6, L_1_S, L_1_7, L_1_8,
	L_CARD2, L_2_1, L_2_2, L_CARD3, L_3_1, L_3_2, L_3_3, L_CARD4, L_4_1, L_END };
unsigned char room_x [] =  { 26, 26, 26, 26, 26, 24, 26, 26, 26,  4, 26, 26,  0, 26, 26, 25,  0, 26,  0 };
unsigned char room_y [] =  { 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19,  0, 19, 19, 19,  0, 13,  0 };
unsigned char room_exit [] = { X_LEFT, X_LEFT, X_LEFT, X_LEFT, X_LEFT, X_LEFT, X_LEFT, X_MOUTH, X_LEFT,
	X_NONE, X_TOPLEFT, X_TOPLEFT, X_NONE, X_LEFT, X_LEFT, X_LEFT, X_NONE, X_NONE, X_NONE };
unsigned char room_next [] = { R_1_2, R_1_3, R_1_4, R_1_5, R_1_6, R_1_7, R_1_7, R_1_8, R_CARD2,
	R_2_1, R_2_2, R_CARD3, R_3_1, R_3_2, R_3_3, R_CARD4, R_4_1, R_END, R_1_1 };

unsigned char room;                     /* R_*                            */
unsigned char new_room;                 /* set to change rooms            */
unsigned char lives;
unsigned char game_over;                /* 1: lost, 2: won                */
unsigned char boxes_gone;               /* 1.S: a box has been chosen     */
unsigned char area_left, area_right, area_top;
unsigned char frame;                    /* frame counter                  */

/* the player */
unsigned char px, py;                   /* pixels, top-left of 16x16      */
unsigned char pfacing;                  /* 0 right, 1 left                */
unsigned char pjump;                    /* 1: rising                      */
unsigned char pjc;                      /* frame of the jump (jump_up)    */
unsigned char pfc;                      /* frame of the fall (fall_speed) */
unsigned char pground;                  /* 1: standing on something       */
unsigned char pbump;                    /* 1: hit his head this frame     */
unsigned char pdead;                    /* 1: touched something deadly    */

/* ====================================================================== */
/* Actors: enemies and platforms. Up to 3, sprites 1-3.                    */
/* ====================================================================== */

#define NACT 3
#define A_NONE      0
#define A_KNIGHT    1   /* 1.4: walks along row 9, turns at the ends        */
#define A_SHIFTER   2   /* 1.5: the same along the floor, changing shape    */
#define A_PLATFORM  3   /* 2.x: a moving platform, carries the player       */
#define A_BALL      4   /* 3.1, 3.2: bounces off walls                      */
#define A_GUARD     5   /* 3.3: walks the floor at random                   */
#define A_GUARDIAN  6   /* 4.1: goes up and down at random, fires           */
#define A_PROP      7   /* 1.S: the box and the prize (moved by code)       */

unsigned char a_type [NACT];
unsigned char a_x [NACT], a_y [NACT];   /* pixels, top-left               */
unsigned char a_left [NACT];            /* 1: moving left (or up)         */
unsigned char a_up [NACT];              /* balls: 1 moving up             */
unsigned char a_min [NACT], a_max [NACT];
unsigned char a_frame [NACT];           /* SPR_*                          */
unsigned char a_t [NACT];               /* timer (glitch, ray...)         */
unsigned char a_form [NACT];            /* shifter: 0 knight, 1 player-like */
unsigned char a_moved [NACT];           /* platforms: pixels moved (+/-)  */

/* the part of each actor that touches (offsets from a_x, a_y; size), and
   the part of the player that can be touched */
unsigned char at_ox [] = { 0, 2, 2, 0, 0, 2, 2, 0 };
unsigned char at_oy [] = { 0, 2, 0, 0, 0, 2, 2, 0 };
unsigned char at_w []  = { 0, 12, 12, 16, 8, 12, 12, 16 };
unsigned char at_h []  = { 0, 20, 16, 8, 8, 14, 14, 16 };
#define P_OX 3
#define P_OY 2
#define P_W  10
#define P_H  14

unsigned char an;                       /* actor being handled            */

/* ray (4.1): the guardian's, and the player's shot */
unsigned char ray_t, ray_y;             /* frames left, pixel line        */
unsigned char shot_t, shot_y, shot_wait;
unsigned char lx0, lx1, ly;             /* line_xor: columns, pixel line  */

/* ====================================================================== */
/* ROOM DRAWING, TEXTS, SPRITES: printer.h                                 */
/* ====================================================================== */

#include "printer.h"

/* ====================================================================== */
/* ENEMY / PLATFORM MOVEMENT                                               */
/* ====================================================================== */

/* puts actor an in place for room (called by room_start) */
void actor_set (void) {
	a_left [an] = 0; a_up [an] = 0; a_t [an] = 0; a_form [an] = 0; a_moved [an] = 0;
}

/* the actors each room starts with (the original's start values) */
void actors_start (void) {
	for (an = 0; an < NACT; an ++) { a_type [an] = A_NONE; actor_set (); }
	an = 0;
	if (room == R_1_4) {
		/* line 255: FOR e=24 TO 7 STEP -1, then back. Rows 7-9 */
		a_type [0] = A_KNIGHT; a_frame [0] = SPR_KNIGHT;
		a_x [0] = 24 * 8; a_y [0] = 7 * 8; a_left [0] = 1;
		a_min [0] = 7 * 8; a_max [0] = 24 * 8;
	} else if (room == R_1_5) {
		/* line 400: FOR e=5 TO 20 and back, drawn at columns e+1, e+2 */
		a_type [0] = A_SHIFTER; a_frame [0] = SPR_DOPPEL_R;
		a_x [0] = 6 * 8; a_y [0] = 18 * 8; a_form [0] = 1;
		a_min [0] = 6 * 8; a_max [0] = 21 * 8;
	} else if (room == R_2_1 || room == R_2_2) {
		/* line 952: rows 15, 10 and 5, starting at columns 6, 23, 16
		   (drawn one to the right), all going right, 5..25 */
		for (an = 0; an < 3; an ++) {
			a_type [an] = A_PLATFORM; a_frame [an] = SPR_PLATFORM;
			a_min [an] = 6 * 8; a_max [an] = 26 * 8;
		}
		a_x [0] = 7 * 8;  a_y [0] = 15 * 8;
		a_x [1] = 24 * 8; a_y [1] = 10 * 8;
		a_x [2] = 17 * 8; a_y [2] = 5 * 8;
	} else if (room == R_3_1 || room == R_3_2) {
		/* line 1130: both at row 10, column 10; one going left, one right,
		   both down */
		for (an = 0; an < 2; an ++) {
			a_type [an] = A_BALL; a_frame [an] = SPR_BALL;
			a_x [an] = 10 * 8; a_y [an] = 10 * 8;
		}
		a_left [0] = 1;
	} else if (room == R_3_3) {
		/* line 1340: columns 10 and 15 (drawn one to the right), rows 18-19 */
		for (an = 0; an < 2; an ++) {
			a_type [an] = A_GUARD; a_frame [an] = SPR_GUARD;
			a_y [an] = 18 * 8; a_min [an] = 6 * 8; a_max [an] = 26 * 8;
		}
		a_x [0] = 11 * 8; a_x [1] = 16 * 8;
	} else if (room == R_4_1) {
		/* line 2140: by = 5 (body row; the head is row 4), column 4 */
		a_type [0] = A_GUARDIAN; a_frame [0] = SPR_GUARDIAN;
		a_x [0] = 4 * 8; a_y [0] = 4 * 8;
		a_min [0] = 4 * 8; a_max [0] = 16 * 8;
	}
}

/* walk actor an horizontally mk pixels between a_min and a_max, turning
   at the ends (knight, shifter, platform, guard). a_moved = pixels moved
   (in the direction a_left says) */
void actor_walk (void) {
	a_moved [an] = 0;
	while (mk --) {
		if (a_left [an]) {
			if (a_x [an] <= a_min [an]) { if (!a_moved [an]) a_left [an] = 0; return; }
			-- a_x [an]; ++ a_moved [an];
		} else {
			if (a_x [an] >= a_max [an]) { if (!a_moved [an]) a_left [an] = 1; return; }
			++ a_x [an]; ++ a_moved [an];
		}
	}
}

/* ball: one pixel along x, one along y, bouncing off solid cells and the
   edges of the play area, as lines 1222-1233 (which looked at ATTR) */
void ball_step (void) {
	bw = 8; bh = 8;
	if (a_left [an]) {
		bx = a_x [an] - 1; by = a_y [an];
		if (a_x [an] <= AREA_LEFT || cell_hit ()) a_left [an] = 0; else -- a_x [an];
	} else {
		bx = a_x [an] + 1; by = a_y [an];
		if (a_x [an] >= AREA_RIGHT + 8 || cell_hit ()) a_left [an] = 1; else ++ a_x [an];
	}
	if (a_up [an]) {
		bx = a_x [an]; by = a_y [an] - 1;
		if (a_y [an] <= AREA_TOP || cell_hit ()) a_up [an] = 0; else -- a_y [an];
	} else {
		bx = a_x [an]; by = a_y [an] + 1;
		if (a_y [an] >= AREA_BOTTOM + 8 || cell_hit ()) a_up [an] = 1; else ++ a_y [an];
	}
}

/* line 1580 of the original is dead code; the guardian fires in 3030 */
void guardian_fire (void);

/* moves every actor of the room, one frame */
void actors_move (void) {
	for (an = 0; an < NACT; an ++) {
		gk = a_type [an];
		if (gk == A_KNIGHT) {
			mk = KNIGHT_SPEED; actor_walk ();

		} else if (gk == A_SHIFTER) {
			/* lines 412-500: at every character it may change shape:
			   f <= 5 the knight (3 rows), f > 5 the player look-alike (2
			   rows), with the "changing" figure (sub 8060) in between */
			if ((a_x [an] & 7) == 0 && a_t [an] == 0) {
				gj = 0;
				if (rnd10 () > 5) gj = 1;
				if (gj != a_form [an]) { a_form [an] = gj; a_t [an] = SHIFT_GLITCH; }
			}
			mk = SHIFTER_SPEED; actor_walk ();
			if (a_t [an]) { -- a_t [an]; a_frame [an] = SPR_SHAPESHIFT; a_y [an] = 17 * 8; }
			else if (a_form [an] == 0) { a_frame [an] = SPR_KNIGHT; a_y [an] = 17 * 8; }
			else { a_frame [an] = SPR_DOPPEL_R + a_left [an]; a_y [an] = 18 * 8; }

		} else if (gk == A_PLATFORM) {
			/* lines 980-1060. Is the player standing on it? Then he goes
			   with it (player_move moves him by a_moved) */
			mk = PLATFORM_SPEED; actor_walk ();

		} else if (gk == A_BALL) {
			for (mk = 0; mk < BALL_SPEED; mk ++) ball_step ();

		} else if (gk == A_GUARD) {
			/* lines 1380-1420: at every character, left or right at random */
			if ((a_x [an] & 7) == 0) {
				a_left [an] = 0;
				if (rnd10 () <= 5) a_left [an] = 1;
			}
			mk = GUARD_SPEED; actor_walk ();

		} else if (gk == A_GUARDIAN) {
			/* lines 3000-3030: at every character, RND 1..10:
			   1-3 up, 4-6 down, 8 fire, others stay */
			if ((a_y [an] & 7) == 0) {
				gj = rnd10 ();
				if (gj <= 3) a_t [an] = 1;          /* up    */
				else if (gj <= 6) a_t [an] = 2;     /* down  */
				else a_t [an] = 0;
				if (gj == 8 && ray_t == 0) guardian_fire ();
			}
			for (mk = 0; mk < GUARDIAN_SPEED; mk ++) {
				if (a_t [an] == 1 && a_y [an] > a_min [an]) -- a_y [an];
				if (a_t [an] == 2 && a_y [an] < a_max [an]) ++ a_y [an];
			}
		}
	}
}

/* ====================================================================== */
/* PLAYER MOVEMENT                                                         */
/* ====================================================================== */

/* is the player's box at (bx, by) free? 16x16 */
unsigned char player_free (void) {
	bw = 16; bh = 16;
	return !cell_hit ();
}

/* is there ground under the player's feet: a solid cell, the bottom of
   the area or a platform? (pdead if it is water) */
void player_ground (void) {
	pground = 0;
	if (py >= AREA_BOTTOM) { pground = 1; return; }
	bx = px; by = py + 16; bw = 16; bh = 1;
	gk = cell_hit ();
	if (gk) {
		pground = 1;
		if (gk & C_DEADLY) pdead = 1;    /* line 800: on the water */
		return;
	}
	for (an = 0; an < NACT; an ++) {
		if (a_type [an] == A_PLATFORM && py + 16 == a_y [an]
			&& px + 16 > a_x [an] && px < a_x [an] + 16) {
			pground = an + 2;           /* 2 + the platform */
			return;
		}
	}
}

void player_move (void) {
	pbump = 0;

	/* carried by a platform (lines 1010-1043, which moved the player onto
	   the platform's column) */
	player_ground ();
	if (pground >= 2) {
		an = pground - 2;
		for (mk = a_moved [an]; mk; mk --) {
			if (a_left [an]) {
				bx = px - 1; by = py;
				if (px > area_left && player_free ()) -- px;
			} else {
				bx = px + 1; by = py;
				if (px < area_right && player_free ()) ++ px;
			}
		}
	}

	/* jump: only from the ground (line 8020) */
	if (k_up && !pjump && pground) {
		pjump = 1; pjc = 0;
	}

	if (pjump) {
		/* going up, jump_up [pjc] pixels this frame */
		for (mk = jump_up [pjc]; mk; mk --) {
			bx = px; by = py - 1;
			if (py <= area_top || !player_free ()) {
				pjump = 0; pbump = 1;     /* head bump: start falling */
				break;
			}
			-- py;
		}
		if (++ pjc == JUMP_FRAMES) pjump = 0;
		pfc = 0;
	} else {
		/* gravity: fall_speed [pfc] pixels, a pixel at a time */
		if (pground) pfc = 0;
		else {
			for (mk = fall_speed [pfc]; mk; mk --) {
				player_ground ();
				if (pground) break;
				++ py;
			}
			if (pfc < FALL_FRAMES - 1) ++ pfc;
		}
	}

	/* walking (lines 8009-8010), also in the air */
	if (k_left) {
		pfacing = 1;
		for (mk = 0; mk < PLAYER_SPEED; mk ++) {
			bx = px - 1; by = py;
			if (px <= area_left || !player_free ()) break;
			-- px;
		}
	}
	if (k_right) {
		pfacing = 0;
		for (mk = 0; mk < PLAYER_SPEED; mk ++) {
			bx = px + 1; by = py;
			if (px >= area_right || !player_free ()) break;
			++ px;
		}
	}

}

/* player at the room's start */
void player_start (void) {
	px = room_x [room] << 3; py = (room_y [room] - 1) << 3;
	pjump = pjc = pfc = pdead = 0; pfacing = 1;
}

/* touching an actor? (the original compared x and y, see BUGS.md) */
void player_touch (void) {
	for (an = 0; an < NACT; an ++) {
		gk = a_type [an];
		if (gk == A_NONE || gk == A_PLATFORM || gk == A_PROP) continue;
		if (gk == A_BALL && !BALLS_KILL) continue;
		gi = a_x [an] + at_ox [gk]; gj = a_y [an] + at_oy [gk];
		if (gk == A_SHIFTER) gj = 18 * 8 + 2;   /* only rows 18-19 (line 410) */
		if (gi + at_w [gk] > px + P_OX && px + P_OX + P_W > gi
			&& gj + at_h [gk] > py + P_OY && py + P_OY + P_H > gj) pdead = 1;
	}
}

/* ====================================================================== */
/* GUARDIAN                                                                */
/* ====================================================================== */

/* line 3030: PLOT 48, 175 - by*8 + 4: DRAW 200, 0 (the head's row) */
void guardian_fire (void) {
	ray_y = a_y [0] + 4; ray_t = RAY_FRAMES;
	ly = ray_y; lx0 = 6; lx1 = 31; line_xor ();
	bp_n = P(40); bp_ms = 10; beep ();
}

/* ====================================================================== */
/* Rooms: entering, special rules, exits                                   */
/* ====================================================================== */

/* the first frame of a room: draw it, place everyone */
void room_start (void) {
	room_draw ();
	actors_start ();
	player_start ();
	ray_t = shot_t = shot_wait = 0;
	area_left = AREA_LEFT; area_right = AREA_RIGHT; area_top = PLAYER_TOP;
	if (room == R_1_1) {
		t_row = 10; t_col = 5; t_attr = 7; sp_s = "ENTRA EN EL CASTILLO"; print_str ();
	}
	if (room == R_4_1) { area_left = BRIDGE_LEFT; area_right = BRIDGE_RIGHT; area_top = BRIDGE_TOP; }
}

/* lost a life: back to the room's start (lines 310 and others) */
void player_dies (void) {
	bp_n = P(-10); bp_ms = 300; beep ();
	-- lives;
	if (lives == 0) { game_over = 1; return; }
	hud_draw ();
	player_start ();
	pfacing = 1;
}

/* 1.S: the boxes (lines 2040-2120). The box over the player's head when he
   bumps it is the one chosen. The left one (column 6) gives a life. */
void box_room (void) {
	if (boxes_gone || !pbump) return;
	gi = (px + 8) >> 3;                 /* the player's middle column */
	if (gi >= 6 && gi <= 7) gj = 0;
	else if (gi >= 14 && gi <= 15) gj = 1;
	else if (gi >= 22 && gi <= 23) gj = 2;
	else return;
	if (py != 14 * 8) return;           /* right under it (rows 14-15) */

	/* the boxes and the text go; the chosen box is a sprite now */
	boxes_gone = 1;
	room_draw ();
	an = 0; a_type [0] = A_PROP; a_frame [0] = SPR_BOX;
	a_x [0] = (6 + (gj << 3)) << 3; a_y [0] = 12 * 8;
	/* the box jumps up a character */
	for (gk = 0; gk < 8; gk ++) {
		-- a_y [0]; draw_sprites (); sp_UpdateNow (); wait_frame ();
	}
	bp_n = P(10); bp_ms = 10; beep ();
	if (gj == 0) {
		/* the prize: a little man (an extra life) climbs out of the box,
		   6 characters up with rising notes, and back down */
		bp_n = P(40); bp_ms = 50; beep ();
		an = 1; a_type [1] = A_PROP; a_frame [1] = SPR_PLAYER_R;
		a_x [1] = a_x [0]; a_y [1] = a_y [0];
		for (gk = 0; gk < 48; gk ++) {
			-- a_y [1];
			if ((gk & 7) == 7) { bp_n = P(31 - (gk >> 3)); bp_ms = 10; beep (); }
			draw_sprites (); sp_UpdateNow (); wait_frame ();
		}
		for (gk = 0; gk < 48; gk ++) {
			++ a_y [1]; draw_sprites (); sp_UpdateNow (); wait_frame ();
		}
		a_type [1] = A_NONE;
		++ lives; hud_draw ();
	} else {
		bp_n = P(0); bp_ms = 50; beep ();
	}
	a_type [0] = A_NONE;
	isrc = 0;
}

/* 4.1: rays, shots, winning */
void guardian_room (void) {
	/* the guardian ray: shown RAY_FRAMES, kills if it crosses the player
	   (line 3100: by = y or by = y - 1) */
	if (ray_t) {
		if (ray_y >= py + P_OY && ray_y < py + P_OY + P_H) {
			ly = ray_y; lx0 = 6; lx1 = 31; line_xor ();
			ray_t = 0;
			spr_hide_all (); sp_UpdateNow ();
			sp_s = "EL RAYO TE ALCANZA. LA CAGAS. Oh"; msg_row10 ();
			bp_n = P(40); bp_ms = 1000; beep ();
			wait_key ();
			pdead = 1;
			return;
		}
		if (-- ray_t == 0) { ly = ray_y; lx0 = 6; lx1 = 31; line_xor (); }
	}

	/* the player shot: 0 or SPACE (line 3080): a line across the whole
	   screen at his feet' row; kills the guardian if it crosses it */
	if (shot_wait) -- shot_wait;
	if (shot_t) {
		if (-- shot_t == 0) { ly = shot_y; lx0 = 0; lx1 = 32; line_xor (); }
	} else if (k_fire && !shot_wait) {
		shot_y = py + 8; shot_t = SHOT_FRAMES; shot_wait = SHOT_WAIT;
		ly = shot_y; lx0 = 0; lx1 = 32; line_xor ();
		bp_n = P(30); bp_ms = 5; beep ();
		if (shot_y >= a_y [0] + 2 && shot_y < a_y [0] + 16) {
			/* line 4010 */
			if (ray_t) { ly = ray_y; lx0 = 6; lx1 = 31; line_xor (); ray_t = 0; }
			sp_UpdateNow ();
			a_type [0] = A_NONE; draw_sprites (); sp_UpdateNow ();
			sp_s = "HAS CONSEGUIDO SALIR DEL CASTI- LLO"; msg_row10 ();
			wait_key ();
			game_over = 2;
		}
	}
}

/* has the player left the room? */
void room_exits (void) {
	gk = room_exit [room];
	if (gk == X_LEFT) { if (px <= AREA_LEFT) new_room = room_next [room]; }
	else if (gk == X_MOUTH) { if (px <= 14 * 8) new_room = room_next [room]; }
	else if (gk == X_TOPLEFT) {
		/* line 1070: IF y = 4 AND x = 5 */
		if (px <= 5 * 8 && py <= 3 * 8) new_room = room_next [room];
	}
	/* 1.6: the dot at row 8, column 5 leads to the box room (line 640:
	   IF y = 9 AND x = 5 THEN GO TO 1650) */
	if (room == R_1_6 && px + 16 > 5 * 8 && px <= 5 * 8) {
		if (py + 16 > 8 * 8) {
			if (py <= 8 * 8) new_room = R_1_S;
		}
	}
}

/* ====================================================================== */
/* Cards: the phase titles                                                 */
/* ====================================================================== */

/* FASE 2: the player rides up and down column 4 five times, or until a
   key is pressed (lines 850-920) */
void card_lift (void) {
	px = 32; py = 18 * 8; pfacing = 0;
	for (gi = 0; gi < 5; gi ++) {
		for (gj = 0; gj < 112; gj ++) {
			if (gj < 56) -- py; else ++ py;
			s_n = 0; s_f = SPR_PLAYER_R; s_x = px; s_y = py; spr_put ();
			sp_UpdateNow ();
			wait_frame ();
			if (any_key ()) return;
		}
	}
}

void card (void) {
	spr_hide_all ();
	room_draw ();
	sp_UpdateNow ();
	if (room == R_CARD2) card_lift ();
	else wait_key ();
	spr_hide_all ();
}

/* 4.1 before the fight: the guardian's message, PAUSE 0 (lines 1490-1520) */
void guardian_intro (void) {
	spr_hide_all ();
	sp_UpdateNow ();
	typing = 1; type_msg ();
	wait_key ();
}

/* the end (line 5010): walking out over the grass, ADIOS */
void ending (void) {
	room = R_END;
	spr_hide_all ();
	room_draw ();
	px = 29 * 8; py = 18 * 8;
	for (gi = 0; gi < 76; gi ++) {
		px -= 2;
		s_n = 0; s_f = SPR_PLAYER_L; s_x = px; s_y = py; spr_put ();
		sp_UpdateNow ();
		wait_frame ();
	}
	t_row = 10; t_col = 13; t_attr = 7; sp_s = "ADIOS"; print_str ();
	sp_UpdateNow ();
	wait_key ();
}

/* ====================================================================== */
/* The game                                                                */
/* ====================================================================== */

/* TITLE (lines 10-55): room 1.1 with the instructions, PAUSE 0, three
   beeps, "ENTRA EN EL CASTILLO" */
void title (void) {
	room = R_1_1; lives = LIVES;
	spr_hide_all ();
	room_draw ();
	sp_UpdateNow ();
	wait_key ();
	bp_n = P(0); bp_ms = 100; beep ();
	bp_ms = 200; beep ();
	bp_n = P(10); bp_ms = 1000; beep ();
	t_row = 10; t_col = 5; t_attr = 7; sp_s = "ENTRA EN EL CASTILLO"; print_str ();
}

/* one room, until it is left or the game ends */
void play_room (void) {
	room_start ();
	if (room == R_4_1) guardian_intro ();
	new_room = room;
	isrc = 0;
	while (new_room == room && !game_over) {
		read_keys ();
		actors_move ();
		player_move ();
		player_touch ();
		if (room == R_1_S) box_room ();
		if (room == R_1_6) flash_box ();
		if (room == R_4_1) guardian_room ();
		if (pdead) {
			player_dies ();
			if (room == R_4_1 && !game_over) {
				/* the message on row 10 goes: redraw the bridge */
				room_draw (); typing = 0; type_msg ();
				actors_start (); ray_t = shot_t = 0;
			}
		}
		if (game_over) break;
		room_exits ();
		draw_sprites ();
		sp_UpdateNow ();
		++ frame;
		wait_frame ();
	}
}

void game (void) {
	title ();
	game_over = 0; boxes_gone = 0;
	while (1) {
		if (room == R_CARD2 || room == R_CARD3 || room == R_CARD4) {
			card ();
			room = room_next [room];
			continue;
		}
		play_room ();
		if (game_over == 1) {
			/* line 310: CLS: RUN */
			spr_hide_all (); shadow_clear (); sp_UpdateNow ();
			wf_n = 50; wait_frames ();
			return;
		}
		if (game_over == 2) { ending (); return; }
		room = new_room;
	}
}

main () {
	#asm
		di
	#endasm
	sp_AddMemory (0, NUMBLOCKS, 14, AD_FREE);   /* before sp_RegisterHook */
	sp_InitIM2 (0xf1f1);
	sp_CreateGenericISR (0xf1f1);
	sp_RegisterHook (255, isr);
	#asm
		ei
	#endasm

	sp_Initialize (7, 0);
	dlist = sp_CompDListAddr (0, 0);
	sp_Border (BLACK);
	*((unsigned char *) 23624) = 0;           /* BORDCR: black border */

	for (s_n = 0; s_n < NSPR; s_n ++) spr_create ();

	while (1) {
		rnd_seed += isrc + frame;
		game ();
	}
}
