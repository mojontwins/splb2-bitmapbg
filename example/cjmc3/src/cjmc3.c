/*
 * Kurashiku Nihongo Monsuta Shiro 3 (CJMC3) - C + inline asm port for
 * z88dk (classic, sccz80) + splib2-bitmapbg.
 *
 * Original: LOKOsoft 1991/1992, remade by The Mojon Twins in 2009 as a
 * HiSoft BASIC + fourSpriter showcase. This port keeps the game, its
 * screens and its rules, and changes the engine:
 *
 *   - splib2 (Sprite Pack 2.2) with the BITMAPBG patch: masked sprites that
 *     move pixel by pixel over a bitmapped background, at 25 fps.
 *   - The woods are three ZX0-packed screen thirds per screen. They are
 *     depacked straight into the shadow screen (sp_ZX0 into sp_BgThird(n)),
 *     and every static thing (platforms, trees, houses, HUD, the boniato
 *     patches, fires, eyes) is painted straight into the shadow as an 8x8
 *     tile with sp_BgPutTile. Nothing is drawn on the real screen and
 *     captured back.
 *   - The background attribute is cyan (INK 5) instead of dark blue, so
 *     the sprites, which take the attribute of the cell they are on, show.
 *
 * Coding rules for this port: no local variables (sccz80 is happier with
 * globals), as few parameters as possible (globals instead), only the
 * library calls take parameter lists.
 *
 * Memory map
 *   23296 - 23551   (free: printer buffer)
 *   24200 - ~$D3xx  this binary (code + splib2 + data + display list)
 *   ...   - $D5C0   stack
 *   $D5C0 - $D7FF   splib2 block memory (NUMBLOCKS * 15)
 *   $D800 - $EFFF   SHADOW: background bitmap, display-file layout
 *   $F000 - $F100   IM2 vector table
 *   $F1F1 - $F1FF   generic ISR
 *   $F200 - $FFFF   splib2 horizontal rotation tables
 */

#pragma output STACKPTR=54784

#include <spritepack.h>

#define SHADOW      SP_BGSHADOW     /* = BGSHADOW*256 in SPconfig.def */
#define NUMBLOCKS   31
#define AD_FREE     (SHADOW - NUMBLOCKS * 15)

#include "assets.h"

/* ------------------------------------------------------------------ */
/* Tuning (pixels per frame at 25 fps)                                 */
/* ------------------------------------------------------------------ */

#define PLAYER_HORZ_SPEED   2
#define PLAYER_JUMP_SPEED   2       /* going up                          */
#define PLAYER_FALL_SPEED   2       /* gravity                           */
#define JUMP_HEIGHT         40      /* 5 characters, like the original   */
#define ENEMY_SPEED         2
#define MAX_X               240     /* 256 - 16                          */
#define MAX_Y               176     /* 192 - 16: the original allows the */
                                    /* player down to the HUD rows       */
#define TICK_FRAMES         4       /* one original "loop" = 8 px at 2   */
                                    /* px/frame: AI, eyes and fires run  */
                                    /* once per tick                     */
#define GROW_FRAMES         384     /* original: 768 interrupts          */
#define LIVES               20
#define WATER               5       /* water carried after filling up    */
#define MAX_CARRIED         3
#define WIN_SCORE           30

#define BG_ATTR             5       /* cyan woods (was INK 1)            */
#define COLL_SOLID          1       /* in the display list's tile byte   */
#define COLL_WATER          2

/* sound effects (the original's beeper routines, relocatable) */
#define SFX_PLANT           0
#define SFX_HURT            1
#define SFX_WATERING        2
#define SFX_HARVEST         3
#define SFX_JUMP            4
#define SFX_FILL            5

/* ------------------------------------------------------------------ */
/* splib2 housekeeping                                                 */
/* ------------------------------------------------------------------ */

void *my_malloc (uint bytes) { return sp_BlockAlloc (0); }
void *u_malloc = my_malloc;
void *u_free = sp_FreeBlock;

struct sp_Rect clip_full = {0, 0, 24, 32};

struct sp_SS *spr [3];
unsigned char *spr_cur [3];
unsigned char spr_on [3];

unsigned char isrc;

/* ISR hook: splib2 needs two spare bytes before it */
extern void isr (void);
#asm
	defw 0
._isr
	ld  hl, _isrc
	inc (hl)
	ret
#endasm

/* ------------------------------------------------------------------ */
/* Globals instead of locals and parameters.                           */
/*                                                                     */
/* Who clobbers what (a function may use the globals of the functions */
/* it calls; callers must not keep values there across the call):      */
/*   put_raw ......... (reads t_row t_col t_attr t_tile)               */
/*   put_tile ........ tp + put_raw (also reads t_attr: remap, coll)   */
/*   draw_layout ..... lp lk la lb lc, t_row t_col t_attr t_tile       */
/*   print_str ....... sp_s, t_col t_tile                              */
/*   print_num ....... t_num t_tile t_col                              */
/*   hud_draw ........ everything above + gi                           */
/*   bon_draw_slot ... bp (reads), t_*                                 */
/*   bon_draw_all .... bp gj + bon_draw_slot                           */
/*   spr_put ......... s_n s_f s_x s_y, gp                             */
/*   box_free ........ (reads bx by)                                   */
/*   player_move ..... mk + box_free, sfx                              */
/*   enemy_move ...... mk + box_free                                   */
/*   anim_tick ....... t_* + rnd                                       */
/*   action .......... bp gj + bon_draw_slot, hud_draw, sfx            */
/*   game/title/ending top level: gi gj gw                             */
/* ------------------------------------------------------------------ */

unsigned char gi, gj;
unsigned int  gw;
unsigned char *gp;

unsigned int  cp_dst, cp_len;

unsigned char t_row, t_col, t_attr, t_tile, t_minrow, t_num;
unsigned char *tp;
unsigned char *lp;
unsigned char lk, la, lb, lc;
unsigned char *sp_s;
unsigned char s_n, s_f, s_x, s_y;
unsigned char bx, by;
unsigned char mk;
unsigned char *bp;

unsigned char *dlist;

/* ------------------------------------------------------------------ */
/* Low level                                                           */
/* ------------------------------------------------------------------ */

void fill_mem (unsigned char v) {
	/* cp_len bytes at cp_dst = v */
	#asm
		ld  hl, 2
		add hl, sp
		ld  a, (hl)
		ld  hl, (_cp_dst)
		ld  (hl), a
		ld  d, h
		ld  e, l
		inc de
		ld  bc, (_cp_len)
		dec bc
		ldir
	#endasm
}

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
	/* 25 fps: two interrupts since the last call */
	while (isrc < 2);
	isrc = 0;
}

void wait_frames (unsigned char n) {
	isrc = 0;
	while (isrc < n);
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

/* keys: O P Q SPACE, as in the original */
unsigned char k_left, k_right, k_up, k_fire;

void read_keys (void) {
	gj = inport (0xDFFE);
	k_right = !(gj & 1);
	k_left = !(gj & 2);
	if (k_right && k_left) { k_right = k_left = 0; }
	k_up = !(inport (0xFBFE) & 1);
	k_fire = !(inport (0x7FFE) & 1);
}

unsigned char any_key (void) {
	return ((inport (0x00FE) & 0x1F) != 0x1F);
}

void wait_key (void) {
	while (any_key ());
	while (!any_key ());
}

void __FASTCALL__ sfx (unsigned char n) {
	/* the original's 50-byte beeper routines, back to back in sfx_data */
	#asm
		ld  a, l
		ld  hl, _sfx_data
		ld  de, 50
		or  a
	.sfx_l
		jr  z, sfx_go
		add hl, de
		dec a
		jr  sfx_l
	.sfx_go
		jp  (hl)
	#endasm
}

void play_step (void) {
	#asm
		call _sfx_step
	#endasm
}

/* ------------------------------------------------------------------ */
/* Painting the background (shadow screen + display list)              */
/* ------------------------------------------------------------------ */

/* tile t_tile at (t_row, t_col) with t_attr. No collision change. */
void put_raw (void) {
	sp_BgPutTile (t_row, t_col, t_attr, tiles + (t_tile << 3));
}

/* layout tile: also sets the collision byte from the ORIGINAL attribute
   (BRIGHT = solid, plain INK 5 = water) and turns INK 1 into cyan.    */
void put_tile (void) {
	tp = dlist + (((t_row << 5) + t_col) << 2) + 1;
	if (t_attr & 64) *tp = COLL_SOLID;
	else if (t_attr == 5) *tp = COLL_WATER;
	else *tp = 0;
	if (t_attr == 1) { t_attr = BG_ATTR; put_raw (); t_attr = 1; }
	else put_raw ();
}

void put_adv (void) {
	if (t_row >= t_minrow) put_tile ();
	if (++ t_col == 32) { t_col = 0; ++ t_row; }
}

/* layout stream at lp (see tools/conv.py):
   F0 r c cursor | F1 a attribute | F2 n t: n x t | F3 n t1 t2: n x pair
   FF end | anything else: a tile                                       */
void draw_layout (void) {
	while ((lc = *lp ++) != 0xFF) {
		if (lc == 0xF0) { t_row = *lp ++; t_col = *lp ++; }
		else if (lc == 0xF1) t_attr = *lp ++;
		else if (lc == 0xF2) {
			lk = *lp ++; t_tile = *lp ++;
			while (lk --) put_adv ();
		} else if (lc == 0xF3) {
			lk = *lp ++; la = *lp ++; lb = *lp ++;
			while (lk --) { t_tile = la; put_adv (); t_tile = lb; put_adv (); }
		} else { t_tile = lc; put_adv (); }
	}
}

/* zero-terminated string at sp_s, text font, from (t_row, t_col) */
void print_str (void) {
	while (*sp_s) {
		t_tile = TILE_FONT - 32 + *sp_s ++;
		put_raw ();
		++ t_col;
	}
}

/* t_num (0..99) at (t_row, t_col), font base in t_tile's place: lc     */
unsigned char num_font;
void print_num (void) {
	if (t_num >= 10) {
		t_tile = num_font + 16 + t_num / 10;     /* '0' is code 48 = 32 + 16 */
		put_raw (); ++ t_col;
	}
	t_tile = num_font + 16 + t_num % 10;
	put_raw (); ++ t_col;
}

/* shadow bitmap: clear */
void shadow_clear (void) {
	cp_dst = SHADOW; cp_len = 6144; fill_mem (0);
}

/* ------------------------------------------------------------------ */
/* Sprites                                                             */
/* ------------------------------------------------------------------ */

void spr_put (void) {
	gp = sprite_frames + s_f * SPR_FRAME_SIZE;
	sp_MoveSprAbs (spr [s_n], &clip_full, gp - spr_cur [s_n], s_y >> 3, s_x >> 3, s_x & 7, s_y & 7);
	spr_cur [s_n] = gp;
	spr_on [s_n] = 1;
}

void spr_hide (void) {
	if (spr_on [s_n]) {
		sp_MoveSprAbs (spr [s_n], &clip_full, 0, 0, 34, 0, 0);
		spr_on [s_n] = 0;
	}
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

/* sprite s_n on plane s_f */
void spr_create (void) {
	spr [s_n] = sp_CreateSpr (sp_MASK_SPRITE, 3, sprite_frames, s_f);
	sp_AddColSpr (spr [s_n], sprite_frames + 48);
	sp_AddColSpr (spr [s_n], sprite_frames + 48);
	/* third column: only receives pixels rotated out of the second one.
	   Point it at SPNullSprPtr (never animated): frames need no blank
	   third column. */
	gp = (unsigned char *) spr [s_n];
	gw = (gp [6] << 8) | gp [7];
	for (gi = 0; gi < 9; gi ++) {
		gp = (unsigned char *) gw;
		if (gi >= 6) { gp [7] = null_ptr & 255; gp [8] = null_ptr >> 8; }
		gw = (gp [0] << 8) | gp [1];
	}
	spr_cur [s_n] = sprite_frames;
	spr_on [s_n] = 0;
}

/* ------------------------------------------------------------------ */
/* Collision: is the 16x16 box at (bx, by) free of solid cells?        */
/* ------------------------------------------------------------------ */

unsigned char box_free (void) {
	#asm
		ld  a, (_by)
		ld  c, a
		and 7
		ld  b, 2
		jr  z, bf_r2
		inc b                   ; b = rows covered (2 or 3)
	.bf_r2
		ld  a, c
		and $F8                 ; row * 8
		ld  l, a
		ld  h, 0
		add hl, hl
		add hl, hl
		add hl, hl
		add hl, hl              ; row * 128 = row * 32 cells * 4 bytes
		ld  de, (_dlist)
		add hl, de
		inc hl                  ; collision byte
		ld  a, (_bx)
		ld  c, a
		and $F8
		rrca                    ; col * 4
		ld  e, a
		ld  d, 0
		add hl, de
		ld  a, c
		and 7
		ld  c, 2
		jr  z, bf_c2
		inc c                   ; c = columns covered
	.bf_c2
	.bf_row
		push hl
		push bc
	.bf_col
		ld  a, (hl)
		cp  1                   ; COLL_SOLID
		jr  z, bf_hit
		inc hl
		inc hl
		inc hl
		inc hl
		dec c
		jr  nz, bf_col
		pop bc
		pop hl
		ld  de, 128
		add hl, de
		djnz bf_row
		ld  hl, 1
		ret
	.bf_hit
		pop bc
		pop hl
		ld  hl, 0
	#endasm
}

/* collision byte of cell (t_row, t_col) */
unsigned char coll_at (void) {
	return dlist [(((t_row << 5) + t_col) << 2) + 1];
}

/* ------------------------------------------------------------------ */
/* Game state                                                          */
/* ------------------------------------------------------------------ */

unsigned char np;                       /* screen 0..11                 */
unsigned char px, py, pox, poy;         /* player; entry position       */
unsigned char pfacing;                  /* 0 right, 1 left              */
unsigned char pjump, pjc;               /* jumping, pixels risen        */
unsigned char pfal, pmoved, pfire_prev;
unsigned char plife, pwat, pbon, pscore, show_score;

unsigned char ex, ey, etype, estate, ect, ejump, ejc;

unsigned char tick;                     /* frames to the next tick      */
unsigned int  grow;                     /* frames to the next growth    */
unsigned char anim;                     /* fire frame 0..3              */
unsigned char eye_open, eye_ct;

unsigned char *thirds [] = { zx0_p1t1, zx0_p1t2, zx0_p1t3, zx0_p2t1, zx0_p2t2, zx0_p2t3 };
unsigned char layout_buf [LAYOUT_MAX];  /* current screen's layout      */
unsigned char *layouts [] = {             /* ZX0-packed                   */
	layout_0, layout_1, layout_2, layout_3, layout_4, layout_5,
	layout_6, layout_7, layout_8, layout_9, layout_10, layout_11
};

/* blinking eyes in the dark: row, col (0 = none) */
unsigned char eye_row [] = { 0,  5,  6,  4,  7, 14, 0, 6, 5,  12, 0, 10 };
unsigned char eye_col [] = { 0, 22, 16, 23, 11,  8, 0, 5, 9, 23, 0,  6 };

/* fires: up to two per screen (row 0 = none) */
unsigned char fire_row [] = { 17, 0, 0, 0, 0, 0,  9, 0, 0, 0, 19, 0 };
unsigned char fire_col [] = { 10, 0, 0, 0, 0, 0,  9, 0, 0, 0, 13, 0 };
unsigned char fire_col2 [] = { 0, 0, 0, 0, 0, 0, 16, 0, 0, 0,  0, 0 };
unsigned char fire_attr [] = { 2, 6, 2, 6 };

/* boniato patch by state: tile, attribute */
unsigned char bon_tile [] = { 95, 55, 55, 56, 57, 57, 57, 57, 58, 0 };
unsigned char bon_attr [] = {  6,  5,  4,  4,  4,  6,  2,  3,  3, BG_ATTR };

/* ------------------------------------------------------------------ */
/* Boniatos: boniatos_init holds 12 screens x 4 slots x (row, col,     */
/* state). State 0 bare, 1 planted, 2 watered, 3-4 growing, 5-7 ripe,  */
/* 8 rotten, 9 harvested. Row 0 = unused slot.                          */
/* ------------------------------------------------------------------ */

void bon_draw_slot (void) {
	/* slot at bp */
	if (bp [0]) {
		t_row = bp [0]; t_col = bp [1];
		t_tile = bon_tile [bp [2]]; t_attr = bon_attr [bp [2]];
		put_raw ();
	}
}

void bon_draw_all (void) {
	bp = boniatos_init + np * 12;
	for (gj = 0; gj < 4; gj ++) { bon_draw_slot (); bp += 3; }
}

/* ------------------------------------------------------------------ */
/* HUD: rows 22-23 (the original's lower screen)                       */
/* ------------------------------------------------------------------ */

void hud_draw (void) {
	/* restore the screen's own rows 22-23 */
	t_minrow = 22; lp = layout_buf; draw_layout ();

	/* lives: UDG U, '=' and the number, in the game font, INK 6 */
	t_row = 23; t_col = 0; t_attr = 6;
	t_tile = TILE_UDG + 20; put_raw ();
	t_col = 1; t_tile = TILE_GFX + '=' - 32; put_raw ();
	t_col = 2; t_num = plife; num_font = TILE_GFX; print_num ();

	/* boniatos being carried: "op" in INK 3 */
	t_col = 22; t_attr = 3;
	for (gi = 0; gi < pbon; gi ++) {
		t_tile = TILE_GFX + 'o' - 32; put_raw (); ++ t_col;
		t_tile = TILE_GFX + 'p' - 32; put_raw (); ++ t_col;
	}

	/* water */
	if (pwat) {
		t_col = 29; t_attr = 5; sp_s = "W="; print_str ();
		t_num = pwat; num_font = TILE_FONT; print_num ();
	}

	/* score, after visiting the cauldron */
	if (show_score) {
		t_col = 11; t_attr = 0x47; sp_s = "BONIATOS:"; print_str ();
		t_num = pscore; num_font = TILE_FONT; print_num ();
	}
}

/* ------------------------------------------------------------------ */
/* Animated tiles: fires and eyes, once per tick                       */
/* ------------------------------------------------------------------ */

void draw_eyes (void) {
	t_row = eye_row [np];
	if (t_row) {
		t_col = eye_col [np];
		if (eye_open) { t_tile = 95; t_attr = 2; }      /* "\*\*" INK 2  */
		else { t_tile = '-' - 32; t_attr = BG_ATTR; }   /* "--" (INK 1)  */
		put_raw (); ++ t_col; put_raw ();
	}
}

void anim_tick (void) {
	/* fires: M N O P with INK 2 6 2 6 */
	t_row = fire_row [np];
	if (t_row) {
		t_tile = 'M' - 32 + anim; t_attr = fire_attr [anim];
		t_col = fire_col [np]; put_raw ();
		if (fire_col2 [np]) { t_col = fire_col2 [np]; put_raw (); }
	}
	anim = (anim + 1) & 3;

	/* eyes: open 10-29 ticks, closed 4-5 */
	if (-- eye_ct == 0) {
		if (eye_open) { eye_open = 0; eye_ct = 4 + (rnd () & 1); }
		else { eye_open = 1; eye_ct = 10 + rnd () % 20; }
		draw_eyes ();
	}
}

/* ------------------------------------------------------------------ */
/* Screens                                                             */
/* ------------------------------------------------------------------ */

void draw_screen (void) {
	/* woods: three ZX0 thirds straight into the shadow */
	gp = backdrop_thirds + np * 3;
	sp_ZX0 (thirds [gp [0] - 1], sp_BgThird (0));
	sp_ZX0 (thirds [gp [1] - 1], sp_BgThird (1));
	sp_ZX0 (thirds [gp [2] - 1], sp_BgThird (2));
	sp_BgAttrFill (BG_ATTR);           /* + all cells dirty */

	/* collision map: clear, then the layout sets it */
	for (gw = 1; gw < 768 * 4; gw += 4) dlist [gw] = 0;

	sp_ZX0 (layouts [np], layout_buf);
	t_minrow = 0; lp = layout_buf; draw_layout ();
	bon_draw_all ();
	eye_open = 0; eye_ct = 10; draw_eyes ();
	anim = 0;
	hud_draw ();
}

void enemy_spawn (void) {
	estate = ect = ejump = 0;
	while (1) {
		ex = (rnd () % 30) << 3;
		ey = (rnd () % 20) << 3;
		bx = ex; by = ey;
		if (!box_free ()) continue;
		/* not right on top of the player (the original allowed it) */
		if (ex + 32 > px && px + 32 > ex && ey + 32 > py && py + 32 > ey) continue;
		break;
	}
	etype = rnd () % 3;
}

void enter_screen (void) {
	pox = px; poy = py;
	show_score = 0;
	draw_screen ();
	enemy_spawn ();
}

/* ------------------------------------------------------------------ */
/* Player                                                              */
/* ------------------------------------------------------------------ */

void player_move (void) {
	pfal = pmoved = 0;

	/* gravity */
	if (!pjump) {
		for (mk = 0; mk < PLAYER_FALL_SPEED; mk ++) {
			if (py >= MAX_Y) break;
			bx = px; by = py + 1;
			if (!box_free ()) break;
			++ py; pfal = 1;
		}
	}

	if (k_left) {
		pfacing = 1;
		for (mk = 0; mk < PLAYER_HORZ_SPEED; mk ++) {
			if (px == 0) break;
			bx = px - 1; by = py;
			if (!box_free ()) break;
			-- px; pmoved = 1;
		}
	}
	if (k_right) {
		pfacing = 0;
		for (mk = 0; mk < PLAYER_HORZ_SPEED; mk ++) {
			if (px >= MAX_X) break;
			bx = px + 1; by = py;
			if (!box_free ()) break;
			++ px; pmoved = 1;
		}
	}
	if (pmoved && !pfal && !pjump && (px & 7) == 0) play_step ();

	/* jump: only from solid ground (or the bottom of the screen) */
	if (k_up && !pjump) {
		bx = px; by = py + 1;
		if (py >= MAX_Y || !box_free ()) {
			pjump = 1; pjc = 0;
			sfx (SFX_JUMP);
		}
	}

	if (pjump) {
		for (mk = 0; mk < PLAYER_JUMP_SPEED; mk ++) {
			if (py) {
				bx = px; by = py - 1;
				if (box_free ()) -- py;
			}
		}
		pjc += PLAYER_JUMP_SPEED;
		if (pjc >= JUMP_HEIGHT) pjump = 0;
	}
}

/* returns 1 if the player left the screen (new screen drawn) */
unsigned char player_exits (void) {
	if (px == 0 && k_left && np) {
		-- np; px = MAX_X;
	} else if (px == MAX_X && k_right && np < 11) {
		++ np; px = 0;
	} else if (py == 0 && pjump && np == 9) {
		np = 4; py = MAX_Y; pjc = 0;
	} else if (py == 0 && pjump && np == 11) {
		/* the original gives one more character of jump here */
		np = 8; py = MAX_Y; if (pjc >= 8) pjc -= 8;
	} else if (py == MAX_Y && !pjump && np == 4) {
		np = 9; py = 0;
	} else if (py == MAX_Y && !pjump && np == 8) {
		np = 11; py = 0;
	} else return 0;
	enter_screen ();
	return 1;
}

void player_hurt (void) {
	sfx (SFX_HURT);
	-- plife;
	hud_draw ();
}

/* fell into the water (row 23)? */
void check_water (void) {
	if (py >= 168) {
		t_row = 23; t_col = px >> 3;
		if (coll_at () == COLL_WATER) goto drown;
		t_col = (px + 15) >> 3;
		if (coll_at () == COLL_WATER) goto drown;
	}
	return;
drown:
	px = pox; py = poy; pjump = 0;
	player_hurt ();
}

/* ------------------------------------------------------------------ */
/* Enemy: the original's random walker                                 */
/* ------------------------------------------------------------------ */

void enemy_move (void) {
	if (tick == 0) {
		if (ect) -- ect;
		else {
			mk = rnd () % 25;
			ect = 3 + rnd () % 7;
			if (mk < 2) estate = 1;
			else if (mk < 4) estate = 2;
			else if (mk < 6) estate = 3;
			else estate = 0;
		}
	}

	if (!ejump) {
		for (mk = 0; mk < ENEMY_SPEED; mk ++) {
			if (ey >= MAX_Y) break;
			bx = ex; by = ey + 1;
			if (!box_free ()) break;
			++ ey;
		}
	}
	if (estate == 1) {
		for (mk = 0; mk < ENEMY_SPEED; mk ++) {
			if (ex == 0) break;
			bx = ex - 1; by = ey;
			if (!box_free ()) break;
			-- ex;
		}
	} else if (estate == 2) {
		for (mk = 0; mk < ENEMY_SPEED; mk ++) {
			if (ex >= MAX_X) break;
			bx = ex + 1; by = ey;
			if (!box_free ()) break;
			++ ex;
		}
	} else if (estate == 3 && !ejump) {
		bx = ex; by = ey + 1;
		if (ey >= MAX_Y || !box_free ()) {
			ejump = 1; ejc = 0; estate = 0; ect = 0;
		}
	}
	if (ejump) {
		for (mk = 0; mk < ENEMY_SPEED; mk ++) {
			if (ey) {
				bx = ex; by = ey - 1;
				if (box_free ()) -- ey;
			}
		}
		ejc += ENEMY_SPEED;
		if (ejc >= JUMP_HEIGHT) ejump = 0;
	}
}

void check_enemy (void) {
	if (ex + 8 >= px && px + 8 >= ex && ey + 8 >= py && py + 8 >= ey) {
		player_hurt ();
		enemy_spawn ();
	}
}

/* ------------------------------------------------------------------ */
/* SPACE: fill the can, plant, water, harvest, deliver                 */
/* ------------------------------------------------------------------ */

unsigned char pcx, pcy;

void action (void) {
	pcx = (px + 4) >> 3; pcy = (py + 4) >> 3;    /* nearest character */

	/* the well (screen 11) */
	if (np == 11 && pcy == 18 && pcx >= 4 && pcx <= 6) {
		sfx (SFX_FILL);
		pwat = WATER;
		hud_draw ();
		return;
	}

	/* boniato patches under the player's feet */
	bp = boniatos_init + np * 12;
	for (gj = 0; gj < 4; gj ++) {
		if (bp [0] == pcy + 1 && (bp [1] == pcx || bp [1] == pcx + 1)) {
			if (bp [2] == 0) {
				sfx (SFX_PLANT); bp [2] = 1; bon_draw_slot ();
			} else if (bp [2] == 1 && pwat) {
				-- pwat; sfx (SFX_WATERING); bp [2] = 2; bon_draw_slot (); hud_draw ();
			} else if (bp [2] >= 5 && bp [2] <= 7 && pbon < MAX_CARRIED) {
				sfx (SFX_HARVEST); bp [2] = 9; bon_draw_slot ();
				++ pbon; hud_draw ();
			}
		}
		bp += 3;
	}

	/* Grand Bolote's cauldron (screen 1) */
	if (np == 1 && pcy == 17 && pcx >= 15 && pcx <= 17) {
		if (pbon) {
			sfx (SFX_PLANT);
			pscore += pbon; pbon = 0;
		}
		show_score = 1;
		hud_draw ();
	}
}

/* every GROW_FRAMES: watered patches grow, ripe ones rot */
void grow_all (void) {
	bp = boniatos_init + 2;
	for (gj = 0; gj < 48; gj ++) {
		if (*bp > 1 && *bp < 8) ++ *bp;
		bp += 3;
	}
	bon_draw_all ();
}

/* ------------------------------------------------------------------ */
/* Sprites for this frame                                              */
/* ------------------------------------------------------------------ */

void draw_sprites (void) {
	s_n = 0; s_f = SPR_PLAYER_R + pfacing; s_x = px; s_y = py; spr_put ();
	s_n = 1; s_f = SPR_ENEMY + etype; s_x = ex; s_y = ey; spr_put ();
	s_n = 2;
	if (pbon && py >= 8) { s_f = SPR_BONUS; s_x = px; s_y = py - 8; spr_put (); }
	else spr_hide ();
}

/* ------------------------------------------------------------------ */
/* Title, game over, ending                                            */
/* ------------------------------------------------------------------ */

/* shadow: cell row lb <- cell row la (bitmap only) */
void row_copy (void) {
	#asm
		ld  a, (_la)
		ld  c, 0
		call SPBgAddr_c
		push hl
		ld  a, (_lb)
		ld  c, 0
		call SPBgAddr_c
		ex  de, hl
		pop hl
		ld  b, 8
	.rc_l
		push bc
		push hl
		push de
		ld  bc, 32
		ldir
		pop de
		pop hl
		pop bc
		inc h
		inc d
		djnz rc_l
		ret
	.SPBgAddr_c
		LIB SPBgAddr
		jp  SPBgAddr
	#endasm
}

void title (void) {
	spr_hide_all ();
	shadow_clear ();
	sp_BgAttrFill (6);

	/* the logo comes up from the bottom, like the original's 17 scrolls */
	sp_ZX0 (zx0_logo, sp_BgThird (2));
	sp_BgShow ();
	for (gi = 0; gi < 17; gi ++) {
		for (la = 1; la < 24; la ++) { lb = la - 1; row_copy (); }
		cp_dst = (unsigned int) sp_BgAddr (23, 0);
		for (gj = 0; gj < 8; gj ++) { cp_len = 32; fill_mem (0); cp_dst += 256; }
		wait_frames (2);
		sp_BgShow ();
	}

	/* the woods in the middle third, cyan */
	sp_ZX0 (zx0_p2t3, sp_BgThird (1));
	for (gw = 256 * 4; gw < 512 * 4; gw += 4) dlist [gw] = BG_ATTR;

	t_minrow = 0; lp = title_layout; draw_layout ();
	sp_BgShow ();
	wait_key ();
}

void game_over (void) {
	t_row = 11; t_col = 11; t_attr = 0x47; sp_s = "GAME OVER!"; print_str ();
	sp_UpdateNow ();
	for (gi = 0; gi < 10; gi ++) sfx (SFX_FILL);
	wait_key ();
}

unsigned char *ending_texts [] = {
	" GREAT! YOU MADE IT! YOU         COLLECTED ENOUGH BONIATOS      ",
	" GRAND BOLOTE IS NOW CURED SO    YOU CAN ESCAPE THE ISLAND...    ",
	" SEE YOU IN FUTURE ADVENTURES!   STAY TUNED TO MOJONIA FM       "
};

void ending (void) {
	spr_hide_all ();
	shadow_clear ();
	sp_BgAttrFill (BG_ATTR);
	sp_ZX0 (zx0_ending, sp_BgThird (1));
	sp_UpdateNow ();

	for (gi = 0; gi < 3; gi ++) {
		/* typewriter, rows 17 and 19, INK 7 */
		sp_s = ending_texts [gi];
		t_attr = 7;
		for (gj = 0; *sp_s; gj ++) {
			t_row = 17; t_col = gj;
			if (gj >= 32) { t_row = 19; t_col = gj - 32; }
			t_tile = TILE_FONT - 32 + *sp_s ++;
			if (gj < 64) put_raw ();
			sp_UpdateNow ();
			wait_frames (4);
		}
		wait_frames (100);
		/* clear rows 17-19 */
		t_tile = TILE_FONT;
		for (t_row = 17; t_row < 20; t_row ++)
			for (t_col = 0; t_col < 32; t_col ++) put_raw ();
		sp_UpdateNow ();
	}
	wait_key ();
}

/* ------------------------------------------------------------------ */
/* The game                                                            */
/* ------------------------------------------------------------------ */

void game (void) {
	/* new game: every patch bare */
	bp = boniatos_init + 2;
	for (gj = 0; gj < 48; gj ++) { *bp = 0; bp += 3; }

	pscore = pbon = pwat = 0;
	plife = LIVES;
	np = 10; px = 144; py = 80;
	pfacing = pjump = pjc = 0;
	pfire_prev = 1;
	tick = TICK_FRAMES; grow = GROW_FRAMES;
	enter_screen ();

	while (1) {
		read_keys ();
		player_move ();

		if (player_exits ()) {
			draw_sprites (); sp_UpdateNow (); isrc = 0;
			continue;
		}

		if (-- tick == 0) { tick = TICK_FRAMES; anim_tick (); }
		enemy_move ();
		check_enemy ();
		check_water ();

		if (k_fire && !pfire_prev) action ();
		pfire_prev = k_fire;

		if (-- grow == 0) { grow = GROW_FRAMES; grow_all (); }

		draw_sprites ();
		sp_UpdateNow ();

		if (plife == 0) { game_over (); return; }
		if (pscore >= WIN_SCORE) { ending (); return; }

		wait_frame ();
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

	sp_Initialize (BG_ATTR, 0);
	dlist = sp_CompDListAddr (0, 0);
	sp_Border (BLACK);
	*((unsigned char *) 23624) = 0;           /* BORDCR: the beeper routines
	                                             keep the border black      */

	s_n = 0; s_f = 1; spr_create ();
	s_n = 1; s_f = 2; spr_create ();
	s_n = 2; s_f = 0; spr_create ();

	while (1) {
		title ();
		rnd_seed += isrc;
		game ();
	}
}
