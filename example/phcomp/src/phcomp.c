/*
 * Phantomas Compilation (LOKO SOFT 1991-1992, remade)
 * z88dk classic (sccz80) + splib2 (splib2tc.lib: tile mode with coloured
 * sprites, see ../../SPconfig.def SPRCOLOUR).
 *
 * Six Phantomas games written in Sinclair BASIC between 1990 and 1992,
 * some compiled with ERE's compiler, remade on one engine: pheng2, a
 * revamp of pheng (the Phantomas Engine, Mojon Twins 2013):
 *
 *   1 Phantomas en el pais brujil       (1991, compiled)
 *   2 Phantomas en la luna              (1992, compiled)
 *   3 Phantomas V, first version        (BASIC)
 *   4 Phantomas V, second version       (compiled)
 *   5 Phantomas Infinito, first version (BASIC)
 *   6 Phantomas Infinito, third version (compiled)
 *
 * pheng2, what changed from pheng:
 *   - Rooms are made with the room editor (gfx/rooms.txt, compact
 *     encoding) on one common tileset (gfx/tiles.png), instead of RLE
 *     strings. BRIGHT is solid, FLASH deadly, both a platform.
 *   - Each game is data (gfx/games.txt): its rooms, the exits, the
 *     enemies, where it starts and how it ends.
 *   - Generic enemies any game can use (enemies.h): linear, chaser,
 *     random, drop, ghost, bouncer, hopper, platform, goal, faller and
 *     toggle (tiles that switch on and off).
 *   - Masked sprites in colour (splib2tc); collision from a map (cmap)
 *     instead of the screen attributes.
 *   - The hot paths (player, enemies, drawing) are commented assembly.
 *   - No local variables, no function parameters: globals.
 *
 * Code style: mojon-c-style.md (tabs; preprocessor and #asm indented as
 * code).
 *
 * Memory map (48K)
 *   24200 - ...     this binary: code, splib2, data, the display list
 *   ...   - $EC88   stack (grows down)
 *   $EC88 - $EFFF   splib2 block memory (NUMBLOCKS * 15)
 *   $F000 - $F100   IM2 vector table
 *   $F1F1 - $F1FF   generic ISR
 *   $F200 - $FFFF   splib2 horizontal rotation tables
 */

#pragma output STACKPTR=60552

#include <spritepack.h>

#define NUMBLOCKS       59              /* 5 sprites x 10 blocks + hook + spare */
#define AD_FREE         60552           /* $F000 - NUMBLOCKS * 15        */

/* collision map cells (put_tile in printer.h) */
#define C_SOLID         1
#define C_DEADLY        2
#define C_PLATFORM      3

#include "assets.h"

/* the game table (assets.h): 12 bytes a game */
#define GAME_MENU       0
#define GAME_TITLE      2
#define GAME_ENDING     4
#define GAME_ROFF       6
#define GAME_ROOM       8
#define GAME_X          9
#define GAME_Y          10
#define GAME_INK        11

/* a room of a game: rooms.txt index, exits L R U D, flags, enemies */
#define R_ROOM          0
#define R_EXITS         1
#define R_FLAGS         5
#define R_ENEMIES       6
#define RF_FLY          1

#define FLICKER         50              /* frames you can't die after dying */

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

/* the sprites' clipping rectangle, as SPMoveSprAbs reads it (IY): top
   row, bottom row + 1, left column, right column + 1: the play area */
extern unsigned char clip [];
#asm
	._clip
		defb 0, 22, 0, 32
#endasm

/* ====================================================================== */
/* Globals instead of locals and parameters.                               */
/*                                                                         */
/* Who uses what (a function may use the globals of the functions it      */
/* calls, so callers must not keep values there across the call):          */
/*   drawing (printer.h) the table at the top of printer.h                 */
/*   player (player.h) . p* cm_* exit_dir dead                             */
/*   enemies.h ......... en [] en_* rnd_seed                               */
/*   sfx ............... sfx_n bp_*                                        */
/*   game level ........ gi gj gp                                          */
/* ====================================================================== */

unsigned char gi, gj, gk;
unsigned int  gw;
unsigned char *gp;

unsigned char t_row, t_col, t_attr, t_tile, t_num;
unsigned char *lp;
unsigned char *sp_s;
unsigned char s_n, s_f, s_x, s_y, s_c;

unsigned char cmap [768];               /* the room's collision map       */

/* the game */
unsigned char game;                     /* 0-5                            */
unsigned char *g_tab;                   /* its entry in game_tab          */
unsigned char *g_title;
unsigned char *g_menu;                  /* its short name (the HUD)       */
unsigned char *g_roff;                  /* its rooms (offsets)            */
unsigned char g_ink;                    /* the player's colour            */
unsigned char lives;
unsigned char room;                     /* the room of the game           */
unsigned char room_n;                   /* the room of rooms.txt          */
unsigned char *room_p;                  /* its entry in the game's rooms  */
unsigned char room_fly;
unsigned char ex_l, ex_r, ex_u, ex_d;   /* exits: rooms, 255 none         */
unsigned char exit_dir;                 /* 1 L 2 R 3 U 4 D, 0 none        */
unsigned char dead, won, flicker;
unsigned char has_item;                /* the item of the game is taken  */
unsigned char frame_ct;

/* the player (player.h) */
unsigned char px, py, pstate, pfacing, pframe, pfr;
unsigned char pjt, pfi, pvx, pkpress;
unsigned char *ptab;
unsigned char p_plat;                   /* the platform it stands on      */
/* where it came into the room: it comes back there after dying */
unsigned char o_px, o_py, o_pstate, o_pfacing, o_pjt, o_pfi, o_pvx;
unsigned char *o_ptab;

/* keys: O P / Q (SPACE) high jump / A long jump; Sinclair 6 7 9 8 0;
   Kempston */
unsigned char k_left, k_right, k_hi, k_lo;

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

unsigned char wf_n;
void wait_frames (void) {
	isrc = 0;
	while (isrc < wf_n);
	isrc = 0;
}

void read_keys (void) {
	#asm
			ld  bc, $DFFE               ; P O
			in  a, (c)
			cpl
			ld  d, a
			and 1
			ld  (_k_right), a
			ld  a, d
			rrca
			and 1
			ld  (_k_left), a
			ld  bc, $EFFE               ; 0 9 8 7 6 (Sinclair 1)
			in  a, (c)
			cpl
			ld  d, a
			bit 4, d                    ; 6 left
			jr  z, rk_n6
			ld  a, 1
			ld  (_k_left), a
		.rk_n6
			bit 3, d                    ; 7 right
			jr  z, rk_n7
			ld  a, 1
			ld  (_k_right), a
		.rk_n7
			ld  a, d                    ; 9 up, 0 fire: high jump
			and 3
			ld  e, a
			ld  a, d                    ; 8 down: long jump
			rrca
			rrca
			and 1
			ld  l, a
			ld  bc, $FBFE               ; Q
			in  a, (c)
			cpl
			and 1
			or  e
			ld  e, a
			ld  bc, $7FFE               ; SPACE
			in  a, (c)
			cpl
			and 1
			or  e
			ld  e, a
			ld  bc, $FDFE               ; A
			in  a, (c)
			cpl
			and 1
			or  l
			ld  l, a
			ld  bc, 31                  ; Kempston: only if bits 5-7 are 0
			in  a, (c)
			ld  d, a
			and $E0
			jr  nz, rk_nokemp
			bit 0, d
			jr  z, rk_kr
			ld  a, 1
			ld  (_k_right), a
		.rk_kr
			bit 1, d
			jr  z, rk_kl
			ld  a, 1
			ld  (_k_left), a
		.rk_kl
			ld  a, d
			and $18                     ; up, fire
			or  e
			ld  e, a
			ld  a, d
			and 4                       ; down
			or  l
			ld  l, a
		.rk_nokemp
			ld  a, e
			or  a
			jr  z, rk_hi
			ld  a, 1
		.rk_hi
			ld  (_k_hi), a
			ld  a, l
			or  a
			jr  z, rk_lo
			ld  a, 1
		.rk_lo
			ld  (_k_lo), a
			ld  a, (_k_left)            ; both: neither
			ld  hl, _k_right
			and (hl)
			ret z
			xor a
			ld  (_k_left), a
			ld  (_k_right), a
	#endasm
}

unsigned char any_key (void) {
	return ((inport (0x00FE) & 0x1F) != 0x1F);
}

/* wait for a key (after the keys are released) */
void wait_key (void) {
	while (any_key ());
	while (!any_key ());
}

/* ====================================================================== */
/* Sound: beeper sweeps. sfx_n (set by anyone during the frame) is played  */
/* at the end of the frame: 1 jump, 2 land, 3 splash, 4 zap, 5 die,        */
/* 6 the end, 7 a key in the menu / a letter                               */
/* ====================================================================== */

unsigned char sfx_n;
/* per effect: length (half periods), first period, change a half period */
unsigned char sfx_tab [] = {
	0, 0, 0,
	24, 40, 254,        /* 1 jump: up      */
	6, 120, 0,          /* 2 land: a thud  */
	16, 30, 3,          /* 3 splash        */
	10, 12, 1,          /* 4 zap           */
	120, 20, 2,         /* 5 die: down     */
	200, 90, 255,       /* 6 the end       */
	4, 30, 0            /* 7 tick          */
};

void sfx (void) {
	#asm
			ld  a, (_sfx_n)
			or  a
			ret z
			ld  e, a
			add a, a
			add a, e
			ld  e, a
			ld  d, 0
			ld  hl, _sfx_tab
			add hl, de
			ld  b, (hl)                 ; B = half periods
			inc hl
			ld  c, (hl)                 ; C = period
			inc hl
			ld  e, (hl)                 ; E = its change
			xor a
			ld  (_sfx_n), a
			di
			ld  d, 0                    ; D = the speaker bit (border black)
		.sfx_half
			ld  a, d
			xor 16
			ld  d, a
			out ($FE), a
			ld  a, c
		.sfx_wait
			push af
			pop af
			dec a
			jr  nz, sfx_wait
			ld  a, c
			add a, e
			jr  nz, sfx_pnz
			inc a
		.sfx_pnz
			ld  c, a
			djnz sfx_half
			ei
	#endasm
}

/* ====================================================================== */
/* Drawing, the player, the enemies                                        */
/* ====================================================================== */

void hud_lives (void);
#include "printer.h"
#include "player.h"
#include "enemies.h"

/* ====================================================================== */
/* Texts                                                                   */
/* ====================================================================== */

/* the screen (rows 0-23) blank in colour t_attr */
void cls_all (void) {
	cls_area ();
	for (t_row = 22; t_row < 24; ++ t_row)
		for (t_col = 0; t_col < 32; ++ t_col) {
			#asm
					xor a
					call pr_raw
			#endasm
		}
}

/* the string sp_s centred on row t_row, colour t_attr */
void print_centre (void) {
	gp = sp_s; gi = 0;
	while (*gp ++) ++ gi;
	t_col = 16 - (gi >> 1);
	print_str ();
}

/* the string sp_s typed from row t_row, 30 columns a line from column 1,
   whole words, a tick a letter; stops at row 21 */
void type_text (void) {
	t_col = 1;
	while (*sp_s) {
		/* the next word's length: does it fit? */
		gp = sp_s; gi = 0;
		while (*gp && *gp != ' ') { ++ gp; ++ gi; }
		if (t_col + gi > 31) { t_row += 2; t_col = 1; }
		if (t_row > 21) return;
		while (gi --) {
			t_tile = *sp_s ++ - 32; put_raw (); ++ t_col;
			sp_UpdateNow ();
			sfx_n = 7; sfx ();
			wf_n = 2; wait_frames ();
		}
		if (*sp_s == ' ') {
			++ sp_s;
			if (t_col > 1) ++ t_col;
		}
	}
}

/* ====================================================================== */
/* The menu                                                                */
/* ====================================================================== */

unsigned char *m_lines [] = {
	"LOKO SOFT PRESENTA",
	"P H A N T O M A S",
	"LA COLECCION 1991-1992",
	"O P ANDAR  Q/SPACE SALTO ALTO",
	"A SALTO LARGO  O 67890 KEMPSTON",
	"REMAKE CON PHENG2 - SPLIB2"
};

void menu (void) {
	spr_hide_all ();
	t_attr = 0x07; cls_all ();
	t_attr = 0x45; t_row = 1; sp_s = m_lines [0]; print_centre ();
	t_attr = 0x46; t_row = 3; sp_s = m_lines [1]; print_centre ();
	t_attr = 0x44; t_row = 5; sp_s = m_lines [2]; print_centre ();
	for (gi = 0; gi < NGAMES; ++ gi) {
		gp = game_tab + gi * 12;
		t_row = 7 + (gi << 1); t_col = 5;
		t_attr = 0x47; t_tile = 17 + gi; put_raw ();      /* "1".. */
		t_col = 7; t_attr = 0x05;
		sp_s = *(unsigned char **) (gp + GAME_MENU); print_str ();
	}
	t_attr = 0x03; t_row = 20; sp_s = m_lines [3]; print_centre ();
	t_row = 21; sp_s = m_lines [4]; print_centre ();
	t_attr = 0x01; t_row = 23; sp_s = m_lines [5]; print_centre ();

	/* Phantomas walks along row 19 while you choose */
	s_n = 0; s_c = 0x87; spr_colour ();
	px = 0; pfacing = 4; pframe = 0;
	while (1) {
		wait_frame ();
		++ frame_ct;
		if (pfacing) {
			px += 2; if (px >= 240) pfacing = 0;
		} else {
			px -= 2; if (px == 0) pfacing = 4;
		}
		pframe = (pframe + 1) & 7;
		s_f = pfacing + walk_anim [pframe]; s_x = px; s_y = 144;
		spr_put ();
		sp_UpdateNow ();
		gk = sp_GetKey ();
		if (gk >= '1' && gk < '1' + NGAMES) break;
	}
	game = gk - '1';
	sfx_n = 6; sfx ();
	spr_hide_all ();
}

/* ====================================================================== */
/* A game                                                                  */
/* ====================================================================== */

/* the room `room` of the game: its entry, exits, flags, enemies; draws it */
void room_enter (void) {
	gp = g_roff + (room << 1);
	room_p = *(unsigned char **) gp;
	room_n = room_p [R_ROOM];
	ex_l = room_p [R_EXITS];
	ex_r = room_p [R_EXITS + 1];
	ex_u = room_p [R_EXITS + 2];
	ex_d = room_p [R_EXITS + 3];
	room_fly = room_p [R_FLAGS] & RF_FLY;
	spr_hide_all ();
	sp_UpdateNow ();
	room_draw ();
	gp = room_p + R_ENEMIES;
	enemies_init ();
	enemies_colour ();
	/* where the player came in */
	o_px = px; o_py = py; o_pstate = pstate; o_pfacing = pfacing;
	o_pjt = pjt; o_pfi = pfi; o_pvx = pvx; o_ptab = ptab;
	exit_dir = 0;
}

void player_restore (void) {
	px = o_px; py = o_py; pstate = o_pstate; pfacing = o_pfacing;
	pjt = o_pjt; pfi = o_pfi; pvx = o_pvx; ptab = o_ptab;
	if (pstate == P_RISE) { pstate = P_FALL; pfi = 0; }
}

/* the player's sprite: flickers while flicker counts */
void player_draw (void) {
	s_n = 0;
	if (flicker && (frame_ct & 2)) spr_hide ();
	else {
		s_f = pfr; s_x = px; s_y = py; spr_put ();
	}
}

/* 1 = finished, 0 = game over */
unsigned char play (void) {
	g_tab = game_tab + game * 12;
	g_title = *(unsigned char **) (g_tab + GAME_TITLE);
	g_menu = *(unsigned char **) (g_tab + GAME_MENU);
	g_roff = *(unsigned char **) (g_tab + GAME_ROFF);
	g_ink = g_tab [GAME_INK] & 7;
	lives = g_tab [GAME_INK] >> 4;
	room = g_tab [GAME_ROOM];
	px = g_tab [GAME_X]; py = g_tab [GAME_Y];
	pstate = P_FALL; pfi = 0; pvx = 0; pfacing = 4; pframe = 0; pkpress = 1;
	pjt = 0; ptab = jump_hi;
	flicker = 0; has_item = 0;

	t_attr = 0x07; cls_all ();
	hud_draw ();
	s_n = 0; s_c = 0x80 | g_ink; spr_colour ();

	while (1) {
		room_enter ();
		dead = won = 0;
		while (1) {
			wait_frame ();
			++ frame_ct;
			if (flicker) -- flicker;
			read_keys ();
			enemies_move ();
			player_move ();
			if (!exit_dir) {
				player_touch ();
				enemies_hit ();
			}
			if (flicker) dead = 0;
			enemies_draw ();
			player_draw ();
			sp_UpdateNow ();
			sfx ();

			if (won) return 1;
			if (dead) {
				sfx_n = 5; sfx ();
				for (gi = 0; gi < 8; ++ gi) {
					sp_Border (gi & 7);
					wf_n = 3; wait_frames ();
				}
				sp_Border (BLACK);
				if (lives == 0) return 0;
				-- lives;
				hud_lives ();
				player_restore ();
				flicker = FLICKER;
				dead = 0;
				/* the enemies start again too */
				gp = room_p + R_ENEMIES;
				enemies_init ();
				/* the toggles' tiles back as they were drawn */
				room_draw ();
				continue;
			}
			if (exit_dir) {
				if (exit_dir == 1) { room = ex_l; px = 240; }
				else if (exit_dir == 2) { room = ex_r; px = 0; }
				else if (exit_dir == 3) { room = ex_u; py = 160; }
				else { room = ex_d; py = 0; }
				break;
			}
		}
	}
}

unsigned char *t_over = "GAME OVER";
unsigned char *t_key = "PULSA UNA TECLA";

void the_end (void) {
	spr_hide_all ();
	t_attr = 0x07; cls_all ();
	sp_UpdateNow ();
	t_attr = 0x46; t_row = 2; sp_s = g_title; print_centre ();
	t_attr = 0x07; t_row = 5;
	sp_s = *(unsigned char **) (g_tab + GAME_ENDING);
	type_text ();
	sfx_n = 6; sfx ();
	t_attr = 0x45; t_row = 23; sp_s = t_key; print_centre ();
	sp_UpdateNow ();
	wait_key ();
}

void game_over (void) {
	spr_hide_all ();
	t_attr = 0x0F; t_row = 10; t_col = 10;
	for (gi = 0; gi < 12; ++ gi) { t_tile = 0; put_raw (); ++ t_col; }
	t_row = 10; sp_s = t_over; print_centre ();
	sp_UpdateNow ();
	wf_n = 50; wait_frames ();
	wait_key ();
}

/* ====================================================================== */
/* Main                                                                    */
/* ====================================================================== */

void main (void) {
	#asm
			di
	#endasm
	sp_AddMemory (0, NUMBLOCKS, 14, AD_FREE);   /* before sp_RegisterHook! */
	sp_InitIM2 (0xf1f1);
	sp_CreateGenericISR (0xf1f1);
	sp_RegisterHook (255, isr);
	gp = tiles;
	for (gi = 0; gi < NTILES; ++ gi) {
		sp_TileArray (gi, gp);
		gp += 8;
	}
	sp_Initialize (INK_WHITE | PAPER_BLACK, 0);
	sp_Border (BLACK);
	for (s_n = 0; s_n < NSPR; ++ s_n) spr_create ();
	#asm
			ei
	#endasm

	while (1) {
		menu ();
		if (play ()) the_end ();
		else game_over ();
	}
}
