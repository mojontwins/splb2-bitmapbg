/*
 * ELEUTERIO - big hamma platforming action!
 *
 * C + inline asm port for z88dk (classic, sccz80) + splib2 of the Mojon
 * Twins' ZX Basic prototype for the Radascompo 2010 (orig/eleuterio.bas,
 * which used fourSpriter 2.1: 2x2 character sprites moving 8 px at a time).
 *
 * This version: splib2 masked sprites moving pixel by pixel at 25 fps over
 * an ordinary **tile** background.
 *
 * ------------------------------------------------------------------------
 * WHY THIS EXAMPLE EXISTS
 * ------------------------------------------------------------------------
 * The other examples in this repository (Lhikoj, CJMC3) use BITMAPBG: the
 * background of every cell comes from a 6144-byte "shadow screen" at
 * $D800-$EFFF. Eleuterio's screen is made of 8x8 tiles only, so it doesn't
 * need that. It is linked against **splib2t.lib**, the same library built
 * with BITMAPBG = 0 (`../../build.sh tiles`), which is plain Sprite Pack 2.2
 * tile mode:
 *
 *   - a cell's background is a tile number in the display list, and
 *     sp_TileArray() says which 8 bytes each tile number draws;
 *   - sp_PrintAtInv(row, col, attr, tile) changes a cell (and marks it
 *     for redrawing); that's all the "drawing" this game does;
 *   - nothing reads or writes $D800-$EFFF, so those 6K are free.
 *
 * This file uses the freed space (see MEMORY MAP below) for splib2's block
 * memory, the stack and the game's work buffers, and leaves about 5K
 * documented as free for whatever comes next (more levels, music, a
 * title screen...). README.md explains the other ways of getting that RAM
 * back when you DO use BITMAPBG.
 *
 * ------------------------------------------------------------------------
 * HOW TO KEEP DEVELOPING IT
 * ------------------------------------------------------------------------
 *   levels.txt         the levels (stairs, platform damage, enemies, tiles)
 *   gfx/sprites.png    every sprite frame (white ink, red mask, black clear)
 *   gfx/tiles.png      the 96 background tiles, in Spectrum colours
 *   this file          the game. Tunable numbers are the #defines below.
 * Run build.sh (or make.bat): tools/conv.py turns the three data files into
 * src/assets.h, then zcc builds eleuterio.tap.
 *
 * ------------------------------------------------------------------------
 * CODING CONVENTIONS (same as the other examples)
 * ------------------------------------------------------------------------
 *   - No local variables: sccz80 (z88dk 1.10 era) generates slow and
 *     sometimes wrong code for them. Functions work on globals instead.
 *   - Few parameters, for the same reason. Values are passed in globals
 *     (e.g. cx, cy, ct for "put tile ct at column cx, row cy").
 *   - The table "WHO CLOBBERS WHAT" below says which globals each function
 *     overwrites, so you know what survives a call.
 *   - Positions of sprites are in PIXELS. The original game worked in
 *     characters; px = 8 * char. The level grid (stairs and holes) is
 *     2 characters (16 px) wide and 4 characters (32 px) high.
 *
 * ------------------------------------------------------------------------
 * MEMORY MAP (48K, splib2t.lib = tile mode)
 * ------------------------------------------------------------------------
 *   $5E88 - ~$B4xx  this binary (org 24200): code, splib2, assets, the
 *                   display list (3K) and splib2's tile table (512 bytes).
 *                   It may grow up to $D7FF.
 *   $D800 - $DA75   splib2 block memory: NUMBLOCKS (42) blocks x 15 bytes
 *                   for 4 sprites (10 blocks each) + the ISR hook.
 *   $DA76 - $DAD6   game buffers: big tiles (32) + level grid (65)
 *   $DAD7 - $EDFF   FREE (about 4.9K): see FREE_RAM / FREE_RAM_END
 *   $EE00 - $EFFF   stack (512 bytes, grows down from $F000)
 *   $F000 - $F100   IM2 vector table             } fixed by splib2's
 *   $F1F1 - $F1FF   generic ISR                  } SPconfig.def
 *   $F200 - $FFFF   splib2 horizontal rotation tables
 *
 * With the BITMAPBG library, $D800-$EFFF would be the shadow screen and
 * the stack/blocks would have to go below $D800 (see Lhikoj or CJMC3).
 */

/* Stack top. The stack grows down from here, into $EFFF, $EFFE... */
#pragma output STACKPTR=61440

#include <spritepack.h>

#define NUMBLOCKS     42                /* 4 sprites x (1 + 9 chars) + hook */
#define AD_FREE       0xD800            /* splib2 block memory goes here    */
#define BUFFERS       (AD_FREE + NUMBLOCKS * 15)        /* = $DA76          */
#define FREE_RAM      (BUFFERS + 32 + 65)               /* = $DAD7          */
#define FREE_RAM_END  0xEE00            /* bottom of a 512-byte stack       */

#include "assets.h"

/* ------------------------------------------------------------------ */
/* TUNING                                                              */
/* ------------------------------------------------------------------ */

/* Movement, in pixels per frame (25 fps). Every move ends on the level
   grid (multiples of 16 px across, 32 px down), so these must divide 16. */
#define PLAYER_SPEED       2       /* walking and climbing                 */
#define PLAYER_FALL_SPEED  4       /* through a hole, down to the next floor */
#define ENEMY_FAST         2       /* enemies with speed=1 in levels.txt   */
#define ENEMY_SLOW         1       /* enemies with speed=0                 */
#define ENEMY_FALL_SPEED   2       /* hammered through the hole            */

/* The original ran its logic once per main loop; timed things (hammer
   blows, fixing, explosions) counted those loops. Here they count "ticks"
   of TICK_FRAMES frames. An enemy climbs into/out of a hole 8 px per tick,
   so TICK_FRAMES must divide 8. */
#define TICK_FRAMES        4
#define FIX_SPEED          (8 / TICK_FRAMES)

#define HAMMER_STRENGTH    2       /* taken off a platform / enemy per blow */
#define START_LIVES        3       /* lives at the start of a game          */
#define TOUCH_DIST         12      /* an enemy closer than this (px, in both */
                                   /* axes) to the player kills him         */
#define POP_TICKS          4       /* length of an enemy's death "pop"      */
#define DEATH_FRAMES       30      /* the player's death animation          */
#define LEVEL_CLEAR_DELAY  75      /* frames between the last death and the */
                                   /* next level                            */

/* Play area, in pixels (the original's character limits * 8) */
#define MIN_X              24      /* column 3: grid column 0               */
#define MAX_X              216     /* column 27: grid column 12             */
#define TOP_Y              40      /* row 5: floor 0                        */
#define START_X            24      /* player start: column 3...             */
#define START_Y            168     /* ...row 21, the bottom floor           */

/* Level grid values (curScreenBuf in the original) */
#define G_HOLE             0       /* 1..12: platform piece, 12 = intact    */
#define G_FIXING           64      /* an enemy is fixing this hole          */
#define G_STAIRS           128     /* stairs down from this floor           */

#define BLANK_TILE         NTILES  /* an extra all-zero tile                */

/* HUD text: tiles TEXT_TILE.. are the ROM font's characters 32..95
   (space, digits, capitals), so HUD_TEXT ("A") = TEXT_TILE + 'A' - 32 */
#define TEXT_TILE          128
#define HUD_ATTR           0x46    /* BRIGHT yellow on black                */

/* ------------------------------------------------------------------ */
/* splib2 housekeeping (as in every splib2 program)                   */
/* ------------------------------------------------------------------ */

/* splib2 allocates its sprite structures through these */
void *my_malloc (uint bytes) { return sp_BlockAlloc (0); }
void *u_malloc = my_malloc;
void *u_free = sp_FreeBlock;

struct sp_Rect clip_full = {0, 0, 24, 32};

struct sp_SS *spr [4];             /* 0-2 enemies, 3 the player             */
unsigned char *spr_cur [4];        /* frame each sprite shows now           */

unsigned char isrc;                /* interrupts since the last frame       */

/* ISR hook: counts interrupts. splib2 needs 2 spare bytes before it. */
extern void isr (void);
#asm
	defw 0
._isr
	ld  hl, _isrc
	inc (hl)
	ret
#endasm

unsigned char blank_gfx [8];       /* graphic of BLANK_TILE (all zero)      */

/* ------------------------------------------------------------------ */
/* GLOBALS                                                             */
/*                                                                     */
/* WHO CLOBBERS WHAT                                                   */
/*   put_char, put_half, put_half_rev, put_block .... (read cx cy ct)  */
/*   put_block_shadow ...... (also reads s0 s1 s2 s3)                  */
/*   put_big ............... bx_ by_ bc + put_char (reads bti)         */
/*   draw_level ............ gx gy tpo stairs tix tbg bti + put_*      */
/*   spr_put / spr_create .. gp gw gi (reads s_n s_f s_x s_y)          */
/*   beep .................. nothing (fastcall, registers only)        */
/*   player_hit_floor ...... gv gi + put_*                              */
/*   move_player ........... pbx pby pcx pcy + the above                */
/*   enemy_random_state .... gj                                        */
/*   move_enemies .......... ei ebx eby eb + enemy_random_state, put_* */
/*   check_touch ........... ei                                        */
/*   hud_draw .............. cx cy ct sp_s gv                          */
/*   player_dies ........... gi + hud/beep/start_level                 */
/*   game / main ........... gi                                        */
/* ------------------------------------------------------------------ */

unsigned char gi, gj, gv;          /* general scratch                       */
unsigned int  gw;
unsigned char *gp;

unsigned char cx, cy, ct;          /* put_*: column, row, tile              */
unsigned char s0, s1, s2, s3;      /* put_block_shadow: per-char offsets    */

unsigned char s_n, s_f, s_x, s_y;  /* spr_put: sprite, frame, x, y (pixels) */

unsigned char *lv;                 /* its 43 bytes (see levels.txt / conv)  */

/* Work buffers in the RAM freed by not using BITMAPBG. Plain pointers to
   fixed addresses (note: sccz80 miscompiles ((char *) CONSTANT)[i], so
   always go through a pointer variable like these).                     */
unsigned char *bigtiles;           /* [2][16] the two 4x4 "big tiles"       */
unsigned char *grid;               /* [5][13] the level grid, see GRID()    */

/* the level grid: 5 floors (0 top .. 4 bottom) x 13 columns. Even columns
   are stairs columns, odd columns the breakable pieces between them.    */
#define GRID(y, x)  grid [(y) * 13 + (x)]

/* ------------------------------------------------------------------ */
/* LOW LEVEL                                                           */
/* ------------------------------------------------------------------ */

/* IN from a port. FASTCALL: the parameter arrives in HL (and can only be
   used from asm; in sccz80 a fastcall parameter has no C name).       */
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

/* BEEP .005, n of the original: a 5 ms tone, n semitones from middle C
   (n = BEEP_MIN..63). Call as beep (n - BEEP_MIN), i.e. with B(n) below.
   The table (count, cycles) is computed by tools/conv.py.              */
#define B(n)  ((n) - BEEP_MIN)
void __FASTCALL__ beep (unsigned char idx) {
	#asm
		ld  h, 0
		ld  d, h
		ld  e, l
		add hl, hl
		add hl, de              ; hl = idx * 3
		ld  de, _beep_tab
		add hl, de
		ld  e, (hl)
		inc hl
		ld  d, (hl)             ; de = delay count for half a period
		inc hl
		ld  a, (hl)
		add a, a
		ld  l, a                ; l = half periods to play
		ld  h, 0                ; h = speaker bit (border stays black)
	.beep_half
		ld  a, h
		xor 16
		ld  h, a
		out ($FE), a
		ld  b, d
		ld  c, e
	.beep_wait
		dec bc
		ld  a, b
		or  c
		jr  nz, beep_wait
		dec l
		jr  nz, beep_half
	#endasm
}

/* ------------------------------------------------------------------ */
/* KEYBOARD: O P Q A SPACE, like the original. As in the original, a    */
/* key only counts if it is the only one pressed in its half-row (so O  */
/* and P together do nothing).                                          */
/* ------------------------------------------------------------------ */

unsigned char k_left, k_right, k_up, k_down, k_fire, k_restart;

void read_keys (void) {
	gv = inport (0xDFFE) & 0x1F;          /* Y U I O P              */
	k_left = (gv == 0x1D);                /* only O (bit 1)         */
	k_right = (gv == 0x1E);               /* only P (bit 0)         */
	k_up = ((inport (0xFBFE) & 0x1F) == 0x1E);      /* only Q   */
	k_down = ((inport (0xFDFE) & 0x1F) == 0x1E);    /* only A   */
	k_fire = ((inport (0x7FFE) & 0x1F) == 0x1E);    /* only SPACE */
	k_restart = !(inport (0xBFFE) & 0x10);          /* H: restart level */
}

/* ------------------------------------------------------------------ */
/* TILES. In tile mode, "drawing" is telling splib2 which tile goes in  */
/* each cell: sp_PrintAtInv stores tile + attribute in the display list */
/* and marks the cell dirty; the next sp_UpdateNow redraws it (with any */
/* sprite on top). The attribute comes from the tile (gfx/tiles.png).   */
/* ------------------------------------------------------------------ */

/* tile ct at column cx, row cy */
void put_char (void) {
	sp_PrintAtInv (cy, cx, tile_attrs [ct], ct);
}

/* tiles ct, ct+1 side by side (the original's tilePutHalfBlock) */
void put_half (void) {
	put_char (); ++ cx; ++ ct; put_char (); -- cx; -- ct;
}

/* tiles ct+1, ct side by side (tilePutHalfBlockReverse) */
void put_half_rev (void) {
	++ ct; put_char (); ++ cx; -- ct; put_char (); -- cx;
}

/* 2x2 block: ct, ct+1 over ct+2, ct+3 (tilePutBlock) */
void put_block (void) {
	put_half (); ++ cy; ct += 2; put_half (); -- cy; ct -= 2;
}

/* 2x2 block where each tile can be swapped for its shaded version: the
   shaded tiles are 4 places further in the tile set, so s0..s3 are 0
   (plain) or 4 (shaded) for the top-left, top-right, bottom-left and
   bottom-right tile (tilePutBlockWithShadow).                         */
void put_block_shadow (void) {
	ct += s0; put_char (); ct -= s0;
	++ cx; ct += 1 + s1; put_char (); ct -= 1 + s1; -- cx;
	++ cy; ct += 2 + s2; put_char (); ct -= 2 + s2;
	++ cx; ct += 3 + s3; put_char (); ct -= 3 + s3; -- cx; -- cy;
}

/* 4x4 "big tile" bti (0 = floor piece, 1 = floor piece with stairs) at
   (cx, cy), from the 16 tile numbers in bigtiles (tilePutBigBlock)     */
unsigned char bti, bx_, by_, bc;
void put_big (void) {
	bc = bti << 4;
	for (by_ = 0; by_ < 4; ++ by_) {
		for (bx_ = 0; bx_ < 4; ++ bx_) {
			ct = bigtiles [bc ++];
			put_char ();
			++ cx;
		}
		cx -= 4; ++ cy;
	}
	cy -= 4;
}

/* ------------------------------------------------------------------ */
/* HUD: row 1, in the ROM font (see TEXT_TILE). "LIVES n" on the left,  */
/* "LEVEL n" on the right. Redrawn by start_level.                      */
/* ------------------------------------------------------------------ */

unsigned char *sp_s;               /* string being printed                  */

/* zero-terminated string sp_s at (cx, cy) */
void print_str (void) {
	while (*sp_s) {
		sp_PrintAtInv (cy, cx, HUD_ATTR, TEXT_TILE - 32 + *sp_s ++);
		++ cx;
	}
}

/* number gv (0..99) at (cx, cy) */
void print_num (void) {
	if (gv >= 10) { sp_PrintAtInv (cy, cx, HUD_ATTR, TEXT_TILE - 32 + '0' + gv / 10); ++ cx; }
	sp_PrintAtInv (cy, cx, HUD_ATTR, TEXT_TILE - 32 + '0' + gv % 10); ++ cx;
}

unsigned char lives;               /* declared here for the HUD             */
unsigned char level;               /* current level number                  */

void hud_draw (void) {
	cy = 1;
	cx = 2;  sp_s = "LIVES "; print_str (); gv = lives; print_num ();
	cx = 22; sp_s = "LEVEL "; print_str (); gv = level + 1; print_num ();
}

/* ------------------------------------------------------------------ */
/* DRAWING A LEVEL (drawHammerinLevel). The screen is:                 */
/*   - a frame (columns 0-1 and 30-31),                               */
/*   - a background strip at the top (rows 3-6),                      */
/*   - 4 floors, each 7 big tiles of 4x4 (rows 7-10, 11-14, 15-18,    */
/*     19-22), with or without stairs, and with the breakable pieces  */
/*     drawn according to their damage,                               */
/*   - a row of spikes (row 23).                                      */
/* It also fills the level grid. The big tiles are rebuilt on the fly  */
/* for every piece so that shadows join up (the original's comments   */
/* call this a real PITA; it's ported as it was).                     */
/* ------------------------------------------------------------------ */

unsigned char gx, gy, tpo, stairs, tix, tbg;

void draw_level (void) {
	lv = levels + level * LEVEL_SIZE;

	/* clear everything to the blank tile */
	sp_ClearRect (&clip_full, 7, BLANK_TILE, sp_CR_TILES);
	sp_Invalidate (&clip_full, &clip_full);

	/* frame: 2x2 blocks down both sides, half a block at the bottom */
	tix = (lv [42] << 2) + 68;
	for (gy = 3; gy <= 21; gy += 2) {
		ct = tix; cy = gy;
		cx = 0; put_block ();
		cx = 30; put_block ();
	}
	ct = tix; cy = 23;
	cx = 0; put_half ();
	cx = 30; put_half ();

	/* the background above the top floor, with a shadow on its top and
	   left edges */
	tbg = (lv [41] << 3) + 36;
	ct = tbg;
	cx = 2; cy = 3; s0 = 4; s1 = 4; s2 = 4; s3 = 0; put_block_shadow ();
	s0 = 4; s1 = 4; s2 = 0; s3 = 0;
	for (cx = 4; cx <= 28; cx += 2) put_block_shadow ();
	cx = 2; cy = 5; s0 = 4; s1 = 0; s2 = 4; s3 = 0; put_block_shadow ();
	for (cx = 4; cx <= 28; cx += 2) put_block ();

	/* the two big tiles: 0 = no stairs, 1 = stairs. Tiles 92 and 93 are
	   the stairs. Entries 0, 3, 8 and 12 are set per piece below.      */
	tix = lv [40] * 6;
	bigtiles [1] = tix;           bigtiles [2] = tix + 1;
	bigtiles [4] = tbg + 6;       bigtiles [5] = tbg + 7;
	bigtiles [6] = tbg + 6;       bigtiles [7] = tbg + 7;
	bigtiles [9] = tbg + 1;       bigtiles [10] = tbg;
	bigtiles [11] = tbg + 1;      bigtiles [13] = tbg + 3;
	bigtiles [14] = tbg + 2;      bigtiles [15] = tbg + 3;

	bigtiles [16 + 1] = 92;       bigtiles [16 + 2] = 93;
	bigtiles [16 + 4] = tbg + 6;  bigtiles [16 + 5] = 92;
	bigtiles [16 + 6] = 93;       bigtiles [16 + 7] = tbg + 7;
	bigtiles [16 + 9] = 92;       bigtiles [16 + 10] = 93;
	bigtiles [16 + 11] = tbg + 5; bigtiles [16 + 13] = 92;
	bigtiles [16 + 14] = 93;      bigtiles [16 + 15] = tbg + 7;

	for (gy = 0; gy < 4; ++ gy) {
		stairs = lv [7 * gy];                  /* bit 0 = column 0 */
		for (gx = 0; gx < 7; ++ gx) {
			bti = stairs & 1;
			if (bti) GRID (gy, gx + gx) = G_STAIRS;
			else GRID (gy, gx + gx) = 12;      /* solid, can't be broken */

			/* left edge: the end of the piece to the left */
			if (gx) {
				tpo = 3 - (lv [7 * gy + gx] >> 2);   /* 0 intact .. 3 hole */
				if (tpo < 3) {
					bigtiles [(bti << 4) + 0] = tix + 1 + 2 * tpo;
					bigtiles [(bti << 4) + 4] = tbg + 6;
					bigtiles [5] = tbg + 7;          /* sic: big tile 0 */
				} else {
					bigtiles [(bti << 4) + 0] = tbg;
					bigtiles [(bti << 4) + 4] = tbg + 2;
					bigtiles [5] = tbg + 3;          /* sic */
				}
			} else bigtiles [(bti << 4) + 0] = tix + 1;

			/* right edge: the start of the piece to the right, which
			   also goes in the grid                                    */
			if (gx < 6) {
				tpo = 3 - (lv [7 * gy + gx + 1] >> 2);
				GRID (gy, gx + gx + 1) = lv [7 * gy + gx + 1];
				if (tpo < 3) bigtiles [(bti << 4) + 3] = tix + 2 * tpo;
				else bigtiles [(bti << 4) + 3] = tbg + 1;
			} else bigtiles [(bti << 4) + 3] = tix;

			/* shadow of the frame on the leftmost column */
			if (gx == 0) {
				bigtiles [(bti << 4) + 8] = tbg + 4;
				bigtiles [(bti << 4) + 12] = tbg + 6;
			} else {
				bigtiles [(bti << 4) + 8] = tbg;
				bigtiles [(bti << 4) + 12] = tbg + 2;
			}

			cx = 2 + (gx << 2); cy = 7 + (gy << 2);
			put_big ();
			stairs >>= 1;
		}
	}

	/* spikes at the bottom */
	ct = 94; cy = 23;
	for (cx = 2; cx <= 28; cx += 2) put_half ();
}

/* ------------------------------------------------------------------ */
/* SPRITES. Frames come from gfx/sprites.png (frame = row*8 + column).  */
/* splib2 changes a sprite's graphic by a displacement from the one it  */
/* shows now, so spr_cur remembers the current frame of each sprite.   */
/* ------------------------------------------------------------------ */

/* sprite s_n shows frame s_f at pixel (s_x, s_y) */
void spr_put (void) {
	gp = sprite_frames + frame_offsets [s_f];
	sp_MoveSprAbs (spr [s_n], &clip_full, gp - spr_cur [s_n], s_y >> 3, s_x >> 3, s_x & 7, s_y & 7);
	spr_cur [s_n] = gp;
}

/* sprite s_n out of sight: moved outside the clip rectangle (column 34).
   The next spr_put brings it back.                                    */
void spr_hide (void) {
	sp_MoveSprAbs (spr [s_n], &clip_full, 0, 0, 34, 0, 0);
}

/* address of splib2's empty graphic (for the third column, below) */
extern unsigned int null_ptr;
#asm
	LIB SPNullSprPtr
._null_ptr
	defw SPNullSprPtr
#endasm

/* create sprite s_n on plane s_f (lower planes are drawn on top). A 16 px
   sprite needs 3 columns of characters to be shifted 0-7 px; the third
   one only receives what is shifted out of the second, so it points at
   SPNullSprPtr and frames don't need to store a blank third column.   */
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
/* THE PLAYER (moveHammerinLevel). A state machine:                   */
/*   0 idle (reads the keys)  1 walking left   2 walking right         */
/*   3 climbing up            4 climbing down  5 falling               */
/*   6 hammering                                                       */
/* Walking, climbing and falling go to a destination (pl_dest) on the  */
/* grid and then return to idle, so the player is always on the grid   */
/* when he decides something.                                          */
/* ------------------------------------------------------------------ */

unsigned char pl_x, pl_y;          /* pixels                                */
unsigned char pl_state, pl_dest;
unsigned char pl_facing;           /* 0 left, 2 right (added to the frame)  */
unsigned char pl_frame;
unsigned char pl_blow, pl_timer;   /* hammering: animation step, frames     */
unsigned char pbx, pby;            /* grid column (0-12) and floor (0-4)    */
unsigned char pcx, pcy;            /* character column / row                */

/* sprite frames (gfx/sprites.png, row 0) */
#define F_LEFT       0             /* +1: the step frame; +2: facing right  */
#define F_RIGHT      2
#define F_CLIMB      4             /* +1: the other foot                    */
#define F_HAMMER     6             /* +1: hammer down                       */

/* Which of two frames to show, from the position (the original alternated
   every character): STEP is 1 on even character columns, CLIMB is 1 on
   odd character rows.
   NOTE: never add a comparison to a number in sccz80, e.g.
   F_RIGHT + ((x & 8) == 0): a true comparison isn't 1 there (that bug
   showed garbage frames). Use bit arithmetic like this, or an if.     */
#define STEP(p)      ((((p) >> 3) & 1) ^ 1)
#define CLIMB(p)     (((p) >> 3) & 1)

void init_player (void) {
	pl_x = START_X; pl_y = START_Y;
	pl_state = 0; pl_facing = F_LEFT; pl_frame = F_LEFT;
}

/* forward: the hammer can hit an enemy fixing a hole */
void hit_enemy (void);

/* The end of a hammer blow (the original's plStateData = 3). Damages the
   platform piece under the player and redraws it, or hits the enemy
   fixing the hole there.                                              */
void player_hit_floor (void) {
	beep (B (0));
	if (pby >= 4) return;                  /* bottom floor: spikes        */
	gv = GRID (pby, pbx);

	if (gv < G_FIXING) {
		/* any floor cell except stairs can be broken: the pieces and
		   the columns without stairs (as in the original)              */
		if (gv > HAMMER_STRENGTH) gv -= HAMMER_STRENGTH; else gv = 0;
		GRID (pby, pbx) = gv;

		if (gv > 3) {
			/* still standing: cracked piece, 3 levels of damage */
			ct = lv [40] * 6 + 2 * (3 - (gv >> 2));
			cx = pcx; cy = pcy + 2; put_half ();
		} else {
			/* broken: draw the hole (background with its shadows) */
			GRID (pby, pbx) = G_HOLE;
			tix = (lv [41] << 3) + 36;
			cx = pcx; cy = pcy + 2;
			if (pbx > 0 && pby > 0 && GRID (pby - 1, pbx - 1) == G_STAIRS) {
				ct = tix + 5; put_char ();
				++ cx; ct = tix; put_char (); -- cx;
			} else { ct = tix; put_half_rev (); }
			cx = pcx + 1; cy = pcy + 3;
			if (pbx < 12 && GRID (pby, pbx + 1) == G_STAIRS) {
				ct = tix + 2; put_char ();
				++ cx; ct = 92; put_char ();
			} else { ct = tix + 2; put_half (); }
		}
		beep (B (-10));
	} else if (gv == G_FIXING) {
		hit_enemy ();
		beep (B (10));
	} else beep (B (50));
}

void move_player (void) {
	/* where we are in the grid (exact only when idle, which is when
	   it's used) and in characters (for drawing)                      */
	pbx = (pl_x - MIN_X) >> 4;
	pby = (pl_y - TOP_Y) >> 5;
	pcx = pl_x >> 3; pcy = pl_y >> 3;

	/* standing on a hole? fall to the next floor */
	if (pl_state == 0 && pby < 4 && GRID (pby, pbx) == G_HOLE) {
		pl_state = 5; pl_dest = pl_y + 32;
	}

	if (pl_state == 0) {
		if (k_left) {
			pl_facing = F_LEFT;
			if (pl_x > MIN_X) { pl_state = 1; pl_dest = pl_x - 16; }
		} else if (k_right) {
			pl_facing = F_RIGHT;
			if (pl_x < MAX_X) { pl_state = 2; pl_dest = pl_x + 16; }
		}
		if (k_up) {
			/* stairs go down from a floor, so to go up we need stairs
			   on the floor above, at this column                        */
			if (pby > 0 && GRID (pby - 1, pbx) == G_STAIRS) { pl_state = 3; pl_dest = pl_y - 32; }
		} else if (k_down) {
			if (pby < 4 && GRID (pby, pbx) == G_STAIRS) { pl_state = 4; pl_dest = pl_y + 32; }
		}
		if (k_fire) { pl_state = 6; pl_blow = 0; pl_timer = 0; }
		if (pl_state == 0) pl_frame = pl_facing;
	}

	if (pl_state == 1) {
		pl_x -= PLAYER_SPEED;
		pl_frame = F_LEFT + STEP (pl_x);   /* step frame on even chars */
		if (pl_x == pl_dest) pl_state = 0;
	} else if (pl_state == 2) {
		pl_x += PLAYER_SPEED;
		pl_frame = F_RIGHT + STEP (pl_x);
		if (pl_x == pl_dest) pl_state = 0;
	} else if (pl_state == 3 || pl_state == 4) {
		if (pl_state == 3) pl_y -= PLAYER_SPEED; else pl_y += PLAYER_SPEED;
		if ((pl_y & 7) == 0) {
			/* a click per character climbed, like the original */
			if (pl_y & 8) {
				pl_frame = F_CLIMB + 1;
				if (pl_state == 3) beep (B (2)); else beep (B (10));
			}
			else { pl_frame = F_CLIMB; beep (B (0)); }
		}
		if (pl_y == pl_dest) pl_state = 0;
	} else if (pl_state == 5) {
		pl_y += PLAYER_FALL_SPEED;
		pl_frame = pl_facing + 1;
		if ((pl_y & 7) == 0) beep (B (30) - (pl_y >> 3));   /* falling whistle */
		if (pl_y == pl_dest) pl_state = 0;
	} else if (pl_state == 6) {
		/* three ticks: hammer up, down, up; then the blow lands */
		pl_frame = F_HAMMER + (pl_blow & 1);    /* up, down, up */
		if (++ pl_timer == TICK_FRAMES) {
			pl_timer = 0;
			if (++ pl_blow == 3) {
				player_hit_floor ();
				pl_state = 0;
			}
		}
	}
}

/* ------------------------------------------------------------------ */
/* ENEMIES (initEnemiesHammerinLevel, moveEnemiesHammerinLevel)         */
/*                                                                     */
/* States: 0 idle (decides)  1 left  2 right  3 up  4 down             */
/*         5 falling (hammered through a hole)                         */
/*         6 fixing a hole   7 "pop" (dying)   8 dead (hidden)         */
/*                                                                     */
/* They walk to the next grid point and decide again. When idle on a   */
/* hole they climb into it and fix it (the grid shows G_FIXING). If the */
/* player hammers them down to 0 life meanwhile, they fall through to  */
/* the floor below. Landing on solid floor (or the spikes) kills them  */
/* with a pop. Landing on another open hole, they survive with half    */
/* their life and start fixing that one.                               */
/* Touching the player (states 0-4) costs him a life: see check_touch. */
/* ------------------------------------------------------------------ */

unsigned char en_x [3], en_y [3];          /* pixels                        */
unsigned char en_state [3], en_dest [3];
unsigned char en_step [3];                 /* ticks into fixing / exploding */
unsigned char en_timer [3];                /* frames into the current tick  */
unsigned char en_life [3], en_last_life [3];
unsigned char en_ai [3];                   /* 0-3, see levels.txt           */
unsigned char en_speed [3];                /* px per frame                  */
unsigned char en_fix [3];                  /* ticks to fix a hole           */
unsigned char en_chasing [3];              /* the original's enCoscao       */
unsigned char en_old_state [3];            /* direction it was walking      */
unsigned char en_facing [3];               /* 0 left, 2 right               */
unsigned char en_frame [3];

unsigned char ei, ebx, eby, eb;            /* enemy being processed         */
unsigned char dead_count, clear_timer;

/* frames: enemy AI type n uses row n+1 of sprites.png */
#define F_FIX        6             /* fixing (instead of hammering)         */
#define F_BOOM       56            /* the "pop"; +1: its second frame       */
#define F_TOMB       58            /* (unused now: free for your ideas)     */

void init_enemies (void) {
	for (ei = 0; ei < 3; ++ ei) {
		en_x [ei] = MIN_X + ei * 96;           /* columns 3, 15 and 27      */
		en_y [ei] = TOP_Y;                     /* top floor                 */
		en_state [ei] = 0;
		en_facing [ei] = 0;
		en_chasing [ei] = 0;
		en_old_state [ei] = 0;
		en_timer [ei] = 0;
		gp = lv + 28 + (ei << 2);
		en_ai [ei] = gp [0];
		en_fix [ei] = gp [1];
		en_life [ei] = en_last_life [ei] = gp [2];
		en_speed [ei] = gp [3] ? ENEMY_FAST : ENEMY_SLOW;
		en_frame [ei] = (gp [0] + 1) << 3;
	}
	dead_count = 0; clear_timer = 0;
}

/* pick a random direction that is possible from here (the original's
   enemySelectRandomEnState). Loops until one works.                   */
void enemy_random_state (void) {
	while (en_state [ei] == 0) {
		gj = rnd () % 6;
		if (gj == 0 && en_x [ei] > MIN_X) {
			en_state [ei] = 1; en_dest [ei] = en_x [ei] - 16; en_facing [ei] = 0;
		} else if (gj == 1 && en_x [ei] < MAX_X) {
			en_state [ei] = 2; en_dest [ei] = en_x [ei] + 16; en_facing [ei] = 2;
		} else if (gj == 2 || gj == 3) {
			if (eby > 0 && GRID (eby - 1, ebx) == G_STAIRS) {
				en_state [ei] = 3; en_dest [ei] = en_y [ei] - 32;
			}
		} else if (gj > 3) {
			if (eby < 3 && GRID (eby, ebx) == G_STAIRS) {
				en_state [ei] = 4; en_dest [ei] = en_y [ei] + 32;
			}
		}
	}
}

/* the player's hammer landed on a hole being fixed: find who's in it */
void hit_enemy (void) {
	for (ei = 0; ei < 3; ++ ei)
		if (en_state [ei] == 6 && en_x [ei] == pl_x && en_y [ei] == pl_y + 16) break;
	if (ei == 3) return;
	if (en_life [ei] > HAMMER_STRENGTH) {
		en_life [ei] -= HAMMER_STRENGTH;
		if (en_step [ei]) -- en_step [ei];     /* it loses a tick of work   */
	} else {
		en_life [ei] = 0;                      /* falls next frame          */
		GRID (pby, pbx) = G_HOLE;              /* the hole stays open       */
	}
}

/* AI: does enemy ei notice the player? (decides en_chasing) */
void enemy_notice (void) {
	gv = en_ai [ei];
	if (gv == 0) en_chasing [ei] = 0;                       /* blind     */
	else if (gv == 1) {                                      /* same floor */
		if (en_y [ei] == pl_y) en_chasing [ei] = 1; else en_chasing [ei] = 0;
	}
	else if (gv == 2)                                        /* +-1 floor  */
		{
		if (en_y [ei] + 32 >= pl_y && en_y [ei] <= pl_y + 32) en_chasing [ei] = 1;
		else en_chasing [ei] = 0;
	}
	else en_chasing [ei] = 1;                                /* hound      */
}

/* AI: choose what to do next. Only at stairs, the edges, the first time,
   or always for a chasing hound; otherwise it keeps walking.         */
void enemy_decide (void) {
	enemy_notice ();

	gv = 0;                                      /* decide now?        */
	if (en_old_state [ei] == 0) gv = 1;          /* first time         */
	if (GRID (eby, ebx) == G_STAIRS) gv = 1;     /* stairs down here   */
	if (eby > 0 && GRID (eby - 1, ebx) == G_STAIRS) gv = 1;   /* or up */
	if (en_ai [ei] == 3 && en_chasing [ei]) gv = 1;           /* hound */
	if (en_x [ei] == MIN_X || en_x [ei] == MAX_X) gv = 1;     /* edges */

	if (gv) {
		if (en_chasing [ei] == 0) enemy_random_state ();
		else {
			/* greedy chase: stairs first if the player is on another
			   floor, else towards him, else random                    */
			if (eby > 0 && GRID (eby - 1, ebx) == G_STAIRS && pl_y < en_y [ei]) {
				en_state [ei] = 3; en_dest [ei] = en_y [ei] - 32;
			}
			if (eby < 3 && GRID (eby, ebx) == G_STAIRS && pl_y > en_y [ei]) {
				en_state [ei] = 4; en_dest [ei] = en_y [ei] + 32;
			}
			if (en_state [ei] == 0) {
				if (pl_x > en_x [ei]) { en_state [ei] = 2; en_dest [ei] = en_x [ei] + 16; }
				else if (pl_x < en_x [ei]) { en_state [ei] = 1; en_dest [ei] = en_x [ei] - 16; }
			}
			if (en_state [ei] == 0) enemy_random_state ();
		}
		en_old_state [ei] = en_state [ei];
	} else {
		/* keep walking the same way */
		if (en_old_state [ei] == 1) { en_state [ei] = 1; en_dest [ei] = en_x [ei] - 16; }
		else if (en_old_state [ei] == 2) { en_state [ei] = 2; en_dest [ei] = en_x [ei] + 16; }
	}
}

/* the enemy finished fixing the hole at (ebx, eby): redraw the piece */
void enemy_restore_piece (void) {
	GRID (eby, ebx) = 12;
	cx = en_x [ei] >> 3; cy = (en_y [ei] >> 3) + 2;
	ct = lv [40] * 6; put_half ();
	ct = (lv [41] << 3) + 36 + 4;          /* shaded background below it */
	++ cy; put_half_rev ();
	if (ebx < 6 && GRID (eby, ebx + 1) != G_STAIRS) {    /* sic: < 6 */
		cx += 2; put_half_rev ();
	}
}

/* one tick passed for enemy ei? (for the timed states 6 and 7) */
unsigned char enemy_tick (void) {
	if (++ en_timer [ei] < TICK_FRAMES) return 0;
	en_timer [ei] = 0;
	return 1;
}

void move_enemies (void) {
	dead_count = 0;
	for (ei = 0; ei < 3; ++ ei) {
		ebx = (en_x [ei] - MIN_X) >> 4;
		eby = (en_y [ei] - TOP_Y) >> 5;
		eb = (en_ai [ei] + 1) << 3;           /* first frame of its row     */

		/* on the spikes (bottom floor): pop */
		if (eby > 3 && en_state [ei] < 7) {
			en_state [ei] = 7; en_step [ei] = 0; en_timer [ei] = 0;
		}

		/* hammered to 0 while fixing: fall. Its life is halved now in
		   case it lands on another hole and survives (see state 5)     */
		if (en_life [ei] == 0) {
			en_state [ei] = 5; en_dest [ei] = en_y [ei] + 16;
			en_life [ei] = en_last_life [ei] >> 1;
			if (en_life [ei] == 0) en_life [ei] = 1;
			en_last_life [ei] = en_life [ei];
		}

		/* idle on a hole: start fixing it */
		if (en_state [ei] == 0 && eby < 4 && GRID (eby, ebx) == G_HOLE) {
			en_state [ei] = 6; en_step [ei] = 0; en_timer [ei] = 0;
			GRID (eby, ebx) = G_FIXING;
		}

		if (en_state [ei] == 0) enemy_decide ();

		/* walking and climbing */
		gv = en_state [ei];
		if (gv == 1) {
			en_x [ei] -= en_speed [ei];
			en_frame [ei] = eb + STEP (en_x [ei]);
			if (en_x [ei] == en_dest [ei]) en_state [ei] = 0;
		} else if (gv == 2) {
			en_x [ei] += en_speed [ei];
			en_frame [ei] = eb + F_RIGHT + STEP (en_x [ei]);
			if (en_x [ei] == en_dest [ei]) en_state [ei] = 0;
		} else if (gv == 3 || gv == 4) {
			if (gv == 3) en_y [ei] -= en_speed [ei]; else en_y [ei] += en_speed [ei];
			en_frame [ei] = eb + F_CLIMB + CLIMB (en_y [ei]);
			if (en_y [ei] == en_dest [ei]) en_state [ei] = 0;
		} else if (gv == 5) {
			en_y [ei] += ENEMY_FALL_SPEED;
			en_frame [ei] = eb + en_facing [ei];
			if (en_y [ei] == en_dest [ei]) {
				/* landed on the floor below. On an open hole: it gets
				   straight to fixing it (no idle frame in between, or the
				   player falling right behind could bump into it).
				   Anything else: pop.                                    */
				eby = (en_y [ei] - TOP_Y) >> 5;
				if (eby < 4 && GRID (eby, ebx) == G_HOLE) {
					en_state [ei] = 6; en_step [ei] = 0; en_timer [ei] = 0;
					GRID (eby, ebx) = G_FIXING;
				} else {
					en_state [ei] = 7; en_step [ei] = 0; en_timer [ei] = 0;
					beep (B (24));
				}
			}
		} else if (gv == 6) {
			/* fixing: 2 ticks climbing down into the hole, en_fix ticks
			   of work, 2 ticks climbing out, then the piece is back     */
			gj = en_step [ei];
			if (gj < 2) {
				en_y [ei] += FIX_SPEED;
				en_frame [ei] = eb + F_CLIMB + CLIMB (en_y [ei]);
			} else if (gj < en_fix [ei] + 2) {
				en_frame [ei] = eb + F_FIX + (gj & 1);
			} else if (gj < en_fix [ei] + 4) {
				en_y [ei] -= FIX_SPEED;
				en_frame [ei] = eb + F_CLIMB + CLIMB (en_y [ei]);
			}
			if (enemy_tick ()) {
				if (gj == en_fix [ei] + 3) {
					/* out of the hole: it's fixed */
					eby = (en_y [ei] - TOP_Y) >> 5;
					enemy_restore_piece ();
					en_state [ei] = 0;
				}
				++ en_step [ei];
			}
		} else if (gv == 7) {
			/* pop: POP_TICKS ticks alternating the two frames, then gone */
			en_frame [ei] = F_BOOM + (en_step [ei] & 1);
			if (enemy_tick () && ++ en_step [ei] == POP_TICKS) {
				en_state [ei] = 8; en_chasing [ei] = 0;
			}
		}

		s_n = ei;
		if (en_state [ei] == 8) { ++ dead_count; spr_hide (); }
		else { s_f = en_frame [ei]; s_x = en_x [ei]; s_y = en_y [ei]; spr_put (); }
	}
}

/* ------------------------------------------------------------------ */
/* THE GAME                                                            */
/* ------------------------------------------------------------------ */

void start_level (void) {
	draw_level ();
	hud_draw ();
	init_enemies ();
	init_player ();
	isrc = 0;
}

/* does an enemy touch the player? Only walking/climbing/idle enemies
   (states 0-4) hurt: falling, fixing and popping ones don't (the one
   fixing the hole under your feet is what you're hammering).          */
unsigned char check_touch (void) {
	for (ei = 0; ei < 3; ++ ei) {
		if (en_state [ei] > 4) continue;
		if (en_x [ei] + TOUCH_DIST > pl_x && pl_x + TOUCH_DIST > en_x [ei] &&
		    en_y [ei] + TOUCH_DIST > pl_y && pl_y + TOUCH_DIST > en_y [ei]) return 1;
	}
	return 0;
}

/* the player was caught: pop animation, lose a life, restart the level
   (or the whole game, from level 1, when there are no lives left)     */
void player_dies (void) {
	for (gi = 0; gi < DEATH_FRAMES; ++ gi) {
		s_n = 3; s_f = F_BOOM + ((gi >> 2) & 1); s_x = pl_x; s_y = pl_y; spr_put ();
		sp_UpdateNow ();
		if ((gi & 3) == 0) beep (B (20) - (gi >> 1));    /* falling tones */
		wait_frame ();
	}
	if (-- lives == 0) {
		for (gi = 24; gi; gi -= 3) beep (B (gi));         /* game over */
		lives = START_LIVES;
		level = 0;
	}
	start_level ();
}

void game (void) {
	level = 0;
	lives = START_LIVES;
	start_level ();
	while (1) {
		read_keys ();
		if (k_restart) { start_level (); continue; }

		move_player ();
		move_enemies ();
		s_n = 3; s_f = pl_frame; s_x = pl_x; s_y = pl_y; spr_put ();

		sp_UpdateNow ();
		wait_frame ();

		if (check_touch ()) { player_dies (); continue; }

		/* all three dead: next level (after a little pause and a tune) */
		if (dead_count == 3 && ++ clear_timer == LEVEL_CLEAR_DELAY) {
			for (gi = 0; gi < 24; gi += 4) beep (B (gi));
			if (++ level == LEVEL_COUNT) level = 0;
			start_level ();
		}
	}
}

main () {
	#asm
		di
	#endasm
	/* splib2's memory for sprites: in the RAM the shadow screen would use */
	sp_AddMemory (0, NUMBLOCKS, 14, AD_FREE);   /* before sp_RegisterHook! */
	sp_InitIM2 (0xf1f1);
	sp_CreateGenericISR (0xf1f1);
	sp_RegisterHook (255, isr);
	#asm
		ei
		ld  a, r
		ld  (_rnd_seed), a              ; a little randomness
	#endasm

	/* the game's buffers, also in the freed RAM */
	bigtiles = (unsigned char *) BUFFERS;
	grid = (unsigned char *) (BUFFERS + 32);

	/* tile set: tile n draws tiles + 8n (gfx/tiles.png) */
	for (gi = 0; gi < NTILES; ++ gi) sp_TileArray (gi, tiles + (gi << 3));
	sp_TileArray (BLANK_TILE, blank_gfx);
	/* HUD text: the ROM font (characters 32-95 from $3D00) */
	for (gi = 0; gi < 64; ++ gi) sp_TileArray (TEXT_TILE + gi, (void *) (0x3D00 + (gi << 3)));

	sp_Initialize (7, BLANK_TILE);
	sp_Border (BLACK);

	/* planes: the player (3) on plane 0, in front of the enemies */
	s_n = 0; s_f = 1; spr_create ();
	s_n = 1; s_f = 2; spr_create ();
	s_n = 2; s_f = 3; spr_create ();
	s_n = 3; s_f = 0; spr_create ();

	game ();
}
