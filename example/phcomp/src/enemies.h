/*
 * Phantomas Compilation - enemies.h
 * The generic enemies every game uses (gfx/games.txt names them):
 *
 *   linear    goes back and forth between (X1,Y1) and (X2,Y2), SPEED
 *             pixels a frame on each axis that moves (pheng's type 1)
 *   chaser    walks on row Y1, within X1..X2, towards the player (the
 *             policemen of Phantomas V)
 *   random    walks on row Y1 within X1..X2, changing its mind at random
 *             (the androids of Infinito)
 *   drop      falls from (X1,Y1) to Y2, faster and faster, splashes,
 *             waits X2 frames and falls again (the drips of V)
 *   ghost     shows up at a random column within X1..X2 on row Y1 for
 *             SPEED * 8 frames, then vanishes for as long ("la sombra
 *             fantasma" of Infinito 3)
 *   bouncer   bounces diagonally inside the box (X1,Y1)-(X2,Y2)
 *   hopper    walks X1..X2 on row Y1, hopping Y2 pixels high (the
 *             witches of Brujas, the green things of V)
 *   platform  like linear, but harmless: you can stand on it and it
 *             carries you (Brujas' raft over the water)
 *   goal      what you came for: touch it and the game is finished (if
 *             Y2 is 1, only once you carry the item)
 *   item      something to pick up (the broom, the treasure): touch it
 *             and you carry it (has_item); it doesn't come back
 *   faller    falls from the sky at a random column within X1..X2 (from
 *             Y1 to Y2), then walks there at random (Infinito)
 *   toggle    not a sprite: the cells (X1,Y1)-(X2,Y2) (in characters!)
 *             switch every SPEED * 8 frames between tile FRAME in colour
 *             MODE (deadly) and nothing (the lamps' lightning in Brujas)
 *
 * Coordinates are pixels (the figure's top-left corner) except toggle's.
 * One struct of EN_SIZE bytes per enemy, in en []: the assembly reads it
 * through IX.
 *
 *   enemies_init    gp (the room's enemy records)   en [] p_plat
 *   enemies_move    en [], px py                    en [] px py p_plat
 *   enemies_hit     en [], px py                    dead won
 *   enemies_draw    en []                           sprites 1-4
 */

#define EN_T            0
#define EN_X            1
#define EN_Y            2
#define EN_X1           3
#define EN_Y1           4
#define EN_X2           5
#define EN_Y2           6
#define EN_MX           7
#define EN_MY           8
#define EN_SP           9
#define EN_F            10
#define EN_MODE         11
#define EN_CT           12
#define EN_AN           13
#define EN_FACE         14
#define EN_ON           15
#define EN_SIZE         16

unsigned char en [MAXEN * EN_SIZE];
unsigned char en_i;
unsigned char en_count;

/* a hop: pixels above the floor, 16 steps (scaled to Y2 by halving) */
unsigned char hop_tab [] = { 0, 6, 11, 15, 18, 20, 22, 23, 24, 23, 22, 20, 18, 15, 11, 6 };

/* ====================================================================== */
/* Random numbers: 16 bit xorshift. A = a random byte (uses HL).           */
/* ====================================================================== */

unsigned int rnd_seed = 0x5A3C;

unsigned char rnd (void) {
	#asm
		.rnd_a
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
			ld  l, a
			ld  h, 0
			ret
	#endasm
}

/* ====================================================================== */
/* Setting up a room's enemies                                             */
/* ====================================================================== */

void enemies_init (void) {
	#asm
			ld  hl, _en                 ; clear them all
			ld  de, _en + 1
			ld  bc, MAXEN * EN_SIZE - 1
			ld  (hl), 0
			ldir
			ld  a, 255
			ld  (_p_plat), a
			ld  hl, (_gp)
			ld  a, (hl)                 ; how many
			ld  (_en_count), a
			inc hl
			or  a
			ret z
			ld  b, a
			ld  ix, _en
		.ei_loop
			push bc
			ld  a, (hl)                 ; T X1 Y1 X2 Y2 SPEED FRAME MODE
			ld  (ix+EN_T), a
			inc hl
			ld  a, (hl)
			ld  (ix+EN_X1), a
			ld  (ix+EN_X), a
			inc hl
			ld  a, (hl)
			ld  (ix+EN_Y1), a
			ld  (ix+EN_Y), a
			inc hl
			ld  a, (hl)
			ld  (ix+EN_X2), a
			inc hl
			ld  a, (hl)
			ld  (ix+EN_Y2), a
			inc hl
			ld  a, (hl)
			ld  (ix+EN_SP), a
			inc hl
			ld  a, (hl)
			ld  (ix+EN_F), a
			inc hl
			ld  a, (hl)
			ld  (ix+EN_MODE), a
			inc hl
			push hl
			ld  (ix+EN_ON), 1
			call ei_type
			pop hl
			ld  de, EN_SIZE
			add ix, de
			pop bc
			djnz ei_loop
			ret

		.ei_type
			ld  a, (ix+EN_T)
			cp  E_LINEAR
			jr  z, ei_linear
			cp  E_PLATFORM
			jr  z, ei_linear
			cp  E_GHOST
			jr  z, ei_ghost
			cp  E_BOUNCER
			jr  z, ei_bouncer
			cp  E_HOPPER
			jr  z, ei_hopper
			cp  E_FALLER
			jr  z, ei_faller
			cp  E_TOGGLE
			jr  z, ei_toggle
			cp  E_ITEM
			jr  z, ei_item
			cp  E_CHASER
			ret nz
			ld  a, (ix+EN_X2)           ; the chaser starts at X2
			ld  (ix+EN_X), a
			ret

		; linear, platform: the direction from 1 to 2 on each axis, then
		; X1 <= X2 and Y1 <= Y2
		.ei_linear
			ld  a, (ix+EN_X2)
			ld  b, (ix+EN_X1)
			call ei_sgn
			ld  (ix+EN_MX), a
			ld  a, (ix+EN_Y2)
			ld  b, (ix+EN_Y1)
			call ei_sgn
			ld  (ix+EN_MY), a
			ld  a, (ix+EN_X1)
			ld  b, (ix+EN_X2)
			cp  b
			jr  c, eil_xok
			ld  (ix+EN_X1), b
			ld  (ix+EN_X2), a
		.eil_xok
			ld  a, (ix+EN_Y1)
			ld  b, (ix+EN_Y2)
			cp  b
			ret c
			ld  (ix+EN_Y1), b
			ld  (ix+EN_Y2), a
			ret
		; A = SPEED, -SPEED or 0 as A >, < or = B
		.ei_sgn
			cp  b
			jr  z, eis_zero
			ld  a, (ix+EN_SP)
			ret nc
			neg
			ret
		.eis_zero
			xor a
			ret

		.ei_ghost
			ld  (ix+EN_ON), 0
			ld  (ix+EN_CT), 20
			ret

		.ei_bouncer
			ld  a, (ix+EN_SP)
			ld  (ix+EN_MX), a
			ld  (ix+EN_MY), a
			ret

		.ei_hopper
			ld  a, (ix+EN_SP)
			ld  (ix+EN_MX), a
			ret

		.ei_faller
			call ef_column
			ret

		.ei_item                        ; already taken: gone
			ld  a, (_has_item)
			xor 1
			ld  (ix+EN_ON), a
			ret

		.ei_toggle
			ld  (ix+EN_ON), 0
			ld  (ix+EN_CT), 1           ; switches on at once
			ret

		; EN_X = a random column between X1 and X2, on a multiple of 8
		.ef_column
			call rnd_a
			ld  b, a
			ld  a, (ix+EN_X2)
			sub (ix+EN_X1)
			jr  z, efc_x1
			ld  c, a
			ld  a, b
		.efc_mod
			cp  c
			jr  c, efc_ok
			sub c
			jr  efc_mod
		.efc_ok
			add a, (ix+EN_X1)
			and $F8
			ld  (ix+EN_X), a
			ret
		.efc_x1
			ld  a, (ix+EN_X1)
			ld  (ix+EN_X), a
			ret
	#endasm
}

/* ====================================================================== */
/* Moving them, one frame                                                  */
/* ====================================================================== */

unsigned char en_dx;

void enemies_move (void) {
	#asm
			ld  a, (_en_count)
			or  a
			jp  z, em_plat
			ld  b, a
			ld  ix, _en
			xor a
			ld  (_en_i), a
		.em_loop
			push bc
			ld  a, (_frame_ct)          ; walking animation: 4 frames each
			rrca
			rrca
			and 1
			ld  (ix+EN_AN), a
			ld  a, (ix+EN_T)
			add a, a
			ld  e, a
			ld  d, 0
			ld  hl, em_jump
			add hl, de
			ld  a, (hl)
			inc hl
			ld  h, (hl)
			ld  l, a
			ld  de, em_next
			push de
			jp  (hl)
		.em_next
			ld  de, EN_SIZE
			add ix, de
			ld  hl, _en_i
			inc (hl)
			pop bc
			djnz em_loop
			jp  em_plat

		.em_jump
			defw em_ret, em_linear, em_chaser, em_random, em_drop, em_ghost
			defw em_bouncer, em_hopper, em_platform, em_ret, em_faller, em_toggle
			defw em_ret
		.em_ret
			ret

		; ----------------------------------------------- linear, platform
		.em_platform
			ld  a, (ix+EN_X)
			push af
			call em_linear
			pop bc
			ld  a, (_p_plat)            ; the player on it moves with it
			ld  hl, _en_i
			cp  (hl)
			ret nz
			ld  a, (ix+EN_X)
			sub b
			ld  hl, _px
			add a, (hl)
			cp  241
			jr  c, emp_xok
			ld  a, (hl)                 ; not past the edges
		.emp_xok
			ld  (hl), a
			ld  a, (ix+EN_Y)
			sub 16
			ld  (_py), a
			ret

		.em_linear
			ld  a, (ix+EN_MX)
			or  a
			jr  z, eml_y
			add a, (ix+EN_X)
			ld  (ix+EN_X), a
			bit 7, (ix+EN_MX)
			jr  nz, eml_xneg
			ld  (ix+EN_FACE), 2
			cp  (ix+EN_X2)
			jr  c, eml_y
			ld  a, (ix+EN_X2)
			jr  eml_xturn
		.eml_xneg
			ld  (ix+EN_FACE), 0
			cp  (ix+EN_X1)
			jr  z, eml_xat1
			jr  nc, eml_y
		.eml_xat1
			ld  a, (ix+EN_X1)
		.eml_xturn
			ld  (ix+EN_X), a
			ld  a, (ix+EN_MX)
			neg
			ld  (ix+EN_MX), a
		.eml_y
			ld  a, (ix+EN_MY)
			or  a
			ret z
			add a, (ix+EN_Y)
			ld  (ix+EN_Y), a
			bit 7, (ix+EN_MY)
			jr  nz, eml_yneg
			cp  (ix+EN_Y2)
			ret c
			ld  a, (ix+EN_Y2)
			jr  eml_yturn
		.eml_yneg
			cp  (ix+EN_Y1)
			jr  z, eml_yat1
			ret nc
		.eml_yat1
			ld  a, (ix+EN_Y1)
		.eml_yturn
			ld  (ix+EN_Y), a
			ld  a, (ix+EN_MY)
			neg
			ld  (ix+EN_MY), a
			ret

		; --------------------------------------------------------- chaser
		.em_chaser
			ld  a, (_frame_ct)          ; a step every other frame
			and 1
			ret nz
			ld  a, (_px)
			sub (ix+EN_X)
			jr  z, emc_still
			jr  c, emc_left
			cp  2
			ret c
			ld  (ix+EN_FACE), 2
			ld  a, (ix+EN_X)
			add a, (ix+EN_SP)
			cp  (ix+EN_X2)
			jr  c, emc_set
			ld  a, (ix+EN_X2)
			jr  emc_set
		.emc_left
			cp  255
			ret nc
			ld  (ix+EN_FACE), 0
			ld  a, (ix+EN_X)
			sub (ix+EN_SP)
			jr  c, emc_x1
			cp  (ix+EN_X1)
			jr  nc, emc_set
		.emc_x1
			ld  a, (ix+EN_X1)
		.emc_set
			ld  (ix+EN_X), a
		.emc_still
			ret

		; --------------------------------------------------------- random
		.em_random
			dec (ix+EN_CT)
			jp  p, emr_move
			call rnd_a                  ; time to choose: 8-39 frames of
			and 31                      ; left, right or standing
			add a, 8
			ld  (ix+EN_CT), a
			call rnd_a
			and 3
			ld  c, 0
			jr  z, emr_set
			ld  c, (ix+EN_SP)
			cp  2
			jr  c, emr_set
			ld  a, c
			neg
			ld  c, a
		.emr_set
			ld  (ix+EN_MX), c
		.emr_move
			ld  a, (ix+EN_MX)
			or  a
			ret z
			bit 7, a
			jr  nz, emr_left
			ld  (ix+EN_FACE), 2
			add a, (ix+EN_X)
			cp  (ix+EN_X2)
			jr  z, emr_ok
			jr  nc, emr_turn
			jr  emr_ok
		.emr_left
			ld  (ix+EN_FACE), 0
			add a, (ix+EN_X)            ; no carry: it went below 0
			jr  nc, emr_turn
			cp  (ix+EN_X1)
			jr  c, emr_turn
		.emr_ok
			ld  (ix+EN_X), a
			ret
		.emr_turn
			ld  a, (ix+EN_MX)
			neg
			ld  (ix+EN_MX), a
			ret

		; ----------------------------------------------------------- drop
		; FACE: 0 falling, 2 splashing; ON 0 while it waits
		.em_drop
			ld  a, (ix+EN_ON)
			or  a
			jr  z, emd_wait
			ld  a, (ix+EN_FACE)
			or  a
			jr  nz, emd_splash
			ld  a, (ix+EN_MY)           ; falling, faster and faster
			cp  6
			jr  nc, emd_vmax
			inc (ix+EN_MY)
		.emd_vmax
			ld  a, (ix+EN_Y)
			add a, (ix+EN_MY)
			cp  (ix+EN_Y2)
			jr  c, emd_sety
			ld  a, (ix+EN_Y2)
			ld  (ix+EN_FACE), 2
			ld  (ix+EN_CT), 8
			ld  hl, _sfx_n
			ld  (hl), 3
		.emd_sety
			ld  (ix+EN_Y), a
			ret
		.emd_splash
			dec (ix+EN_CT)
			ret nz
			ld  (ix+EN_ON), 0
			ld  a, (ix+EN_X2)           ; then wait X2 frames
			ld  (ix+EN_CT), a
			ret
		.emd_wait
			dec (ix+EN_CT)
			ret nz
			ld  (ix+EN_ON), 1
			ld  (ix+EN_FACE), 0
			ld  (ix+EN_MY), 0
			ld  a, (ix+EN_Y1)
			ld  (ix+EN_Y), a
			ret

		; ---------------------------------------------------------- ghost
		.em_ghost
			dec (ix+EN_CT)
			ret nz
			ld  a, (ix+EN_SP)           ; SPEED * 8 frames each way
			add a, a
			add a, a
			add a, a
			ld  (ix+EN_CT), a
			ld  a, (ix+EN_ON)
			xor 1
			ld  (ix+EN_ON), a
			ret z
			call ef_column              ; somewhere new
			ld  a, (_px)                ; facing the player
			cp  (ix+EN_X)
			ld  a, 0
			jr  c, emg_face
			ld  a, 2
		.emg_face
			ld  (ix+EN_FACE), a
			ret

		; -------------------------------------------------------- bouncer
		.em_bouncer
			ld  a, (ix+EN_X)
			add a, (ix+EN_MX)
			ld  (ix+EN_X), a
			bit 7, (ix+EN_MX)
			jr  nz, emb_xneg
			ld  (ix+EN_FACE), 2
			cp  (ix+EN_X2)
			jr  c, emb_y
			ld  a, (ix+EN_X2)
			jr  emb_xturn
		.emb_xneg
			ld  (ix+EN_FACE), 0
			cp  (ix+EN_X1)
			jr  z, emb_xat
			jr  nc, emb_y
		.emb_xat
			ld  a, (ix+EN_X1)
		.emb_xturn
			ld  (ix+EN_X), a
			ld  a, (ix+EN_MX)
			neg
			ld  (ix+EN_MX), a
		.emb_y
			ld  a, (ix+EN_Y)
			add a, (ix+EN_MY)
			ld  (ix+EN_Y), a
			bit 7, (ix+EN_MY)
			jr  nz, emb_yneg
			cp  (ix+EN_Y2)
			ret c
			ld  a, (ix+EN_Y2)
			jr  emb_yturn
		.emb_yneg
			cp  (ix+EN_Y1)
			jr  z, emb_yat
			ret nc
		.emb_yat
			ld  a, (ix+EN_Y1)
		.emb_yturn
			ld  (ix+EN_Y), a
			ld  a, (ix+EN_MY)
			neg
			ld  (ix+EN_MY), a
			ret

		; --------------------------------------------------------- hopper
		; walks X1..X2 as linear does, its height from hop_tab (CT = the
		; step, 0-15), halved if Y2 < 16
		.em_hopper
			ld  a, (ix+EN_X)
			add a, (ix+EN_MX)
			ld  (ix+EN_X), a
			bit 7, (ix+EN_MX)
			jr  nz, emh_neg
			ld  (ix+EN_FACE), 2
			cp  (ix+EN_X2)
			jr  c, emh_hop
			ld  a, (ix+EN_X2)
			jr  emh_turn
		.emh_neg
			ld  (ix+EN_FACE), 0
			cp  (ix+EN_X1)
			jr  z, emh_at
			jr  nc, emh_hop
		.emh_at
			ld  a, (ix+EN_X1)
		.emh_turn
			ld  (ix+EN_X), a
			ld  a, (ix+EN_MX)
			neg
			ld  (ix+EN_MX), a
		.emh_hop
			ld  a, (ix+EN_CT)
			inc a
			and 15
			ld  (ix+EN_CT), a
			ld  e, a
			ld  d, 0
			ld  hl, _hop_tab
			add hl, de
			ld  a, (hl)
			ld  c, a
			ld  a, (ix+EN_Y2)
			cp  16
			jr  nc, emh_full
			srl c
		.emh_full
			ld  a, (ix+EN_Y1)
			sub c
			ld  (ix+EN_Y), a
			ret

		; --------------------------------------------------------- faller
		; FACE 0/2 while walking; MY = the falling speed, 255 once down
		.em_faller
			ld  a, (ix+EN_MY)
			cp  255
			jp  z, em_random
			inc a
			cp  7
			jr  c, emf_v
			ld  a, 6
		.emf_v
			ld  (ix+EN_MY), a
			add a, (ix+EN_Y)
			cp  (ix+EN_Y2)
			jr  c, emf_sety
			ld  a, (ix+EN_Y2)
			ld  (ix+EN_MY), 255
			ld  (ix+EN_CT), 0
		.emf_sety
			ld  (ix+EN_Y), a
			ret

		; --------------------------------------------------------- toggle
		.em_toggle
			dec (ix+EN_CT)
			ret nz
			ld  a, (ix+EN_SP)
			add a, a
			add a, a
			add a, a
			ld  (ix+EN_CT), a
			ld  a, (ix+EN_ON)
			xor 1
			ld  (ix+EN_ON), a
			ld  a, (ix+EN_MODE)         ; on: the tile, deadly (FLASH)
			or  128
			ld  (_t_attr), a
			ld  a, (ix+EN_F)
			ld  (_t_tile), a
			ld  a, (ix+EN_ON)
			or  a
			jr  nz, emt_draw
			ld  a, (ix+EN_MODE)         ; off: a blank cell, harmless
			and $3F
			ld  (_t_attr), a
			xor a
			ld  (_t_tile), a
		.emt_draw
			ld  a, (ix+EN_Y1)
			ld  (_t_row), a
		.emt_row
			ld  a, (ix+EN_X1)
			ld  (_t_col), a
		.emt_col
			push ix
			call _put_tile
			pop ix
			ld  hl, _t_col
			ld  a, (hl)
			inc (hl)
			cp  (ix+EN_X2)
			jr  c, emt_col
			ld  hl, _t_row
			ld  a, (hl)
			inc (hl)
			cp  (ix+EN_Y2)
			jr  c, emt_row
			ld  hl, _sfx_n
			ld  (hl), 4
			ret

		; ------------------------------------------- standing on platforms
		; p_plat = the platform under the feet of the player (255: none). Not
		; while rising. Landing on one stops a fall.
		.em_plat
			ld  a, 255
			ld  (_p_plat), a
			ld  a, (_pstate)
			cp  P_RISE
			ret z
			ld  a, (_en_count)
			or  a
			ret z
			ld  b, a
			ld  ix, _en
			ld  c, 0
		.emp_loop
			ld  a, (ix+EN_T)
			cp  E_PLATFORM
			jr  nz, emp_next
			ld  a, (_px)                ; across: px + 14 > X, px < X + 14
			add a, 14
			cp  (ix+EN_X)
			jr  c, emp_next
			jr  z, emp_next
			ld  a, (ix+EN_X)
			add a, 14
			ld  hl, _px
			cp  (hl)
			jr  c, emp_next
			jr  z, emp_next
			ld  a, (_py)                ; feet: py + 16 - Y in 0..7
			add a, 16
			sub (ix+EN_Y)
			cp  8
			jr  nc, emp_next
			ld  a, c
			ld  (_p_plat), a
			ld  a, (ix+EN_Y)
			sub 16
			ld  (_py), a
			ld  a, (_pstate)
			cp  P_FALL
			ret nz
			xor a                       ; landed on it
			ld  (_pstate), a
			ld  (_pvx), a
			ret
		.emp_next
			ld  de, EN_SIZE
			add ix, de
			inc c
			djnz emp_loop
	#endasm
}

/* ====================================================================== */
/* Touching them: dead = 1, or won = 1 for the goal                        */
/* ====================================================================== */

void enemies_hit (void) {
	#asm
			ld  a, (_en_count)
			or  a
			ret z
			ld  b, a
			ld  ix, _en
		.eh_loop
			ld  a, (ix+EN_ON)
			or  a
			jr  z, eh_next
			ld  a, (ix+EN_T)
			cp  E_PLATFORM
			jr  z, eh_next
			cp  E_TOGGLE
			jr  z, eh_next
			ld  a, (_px)                ; |X - px| < 12
			sub (ix+EN_X)
			jr  nc, eh_xpos
			neg
		.eh_xpos
			cp  12
			jr  nc, eh_next
			ld  a, (_py)                ; |Y - py| < 13
			sub (ix+EN_Y)
			jr  nc, eh_ypos
			neg
		.eh_ypos
			cp  13
			jr  nc, eh_next
			ld  a, (ix+EN_T)
			cp  E_GOAL
			jr  z, eh_goal
			cp  E_ITEM
			jr  z, eh_item
			ld  a, 1
			ld  (_dead), a
			jr  eh_next
		.eh_goal
			bit 0, (ix+EN_Y2)           ; needs the item?
			jr  z, eh_won
			ld  a, (_has_item)
			or  a
			jr  z, eh_next
		.eh_won
			ld  a, 1
			ld  (_won), a
			jr  eh_next
		.eh_item
			ld  (ix+EN_ON), 0
			ld  a, 1
			ld  (_has_item), a
			ld  a, 6
			ld  (_sfx_n), a
		.eh_next
			ld  de, EN_SIZE
			add ix, de
			djnz eh_loop
	#endasm
}

/* ====================================================================== */
/* Their sprites (1-4): the frame from FRAME, the facing and the walking   */
/* animation as MODE says (bits 4-5: 4, 2 or 1 frames)                     */
/* ====================================================================== */

void enemies_draw (void) {
	#asm
			ld  a, 1
			ld  (_s_n), a
			ld  ix, _en
		.ed_loop
			ld  a, (ix+EN_T)
			or  a
			jr  z, ed_hide
			cp  E_TOGGLE
			jr  z, ed_hide
			ld  a, (ix+EN_ON)
			or  a
			jr  z, ed_hide
			ld  a, (ix+EN_MODE)
			and $30
			jr  z, ed_four
			cp  $20
			ld  a, (ix+EN_F)
			jr  z, ed_set               ; one frame
			add a, (ix+EN_AN)           ; two frames
			jr  ed_set
		.ed_four
			ld  a, (ix+EN_F)
			add a, (ix+EN_FACE)
			add a, (ix+EN_AN)
		.ed_set
			ld  (_s_f), a
			ld  a, (ix+EN_X)
			ld  (_s_x), a
			ld  a, (ix+EN_Y)
			ld  (_s_y), a
			push ix
			call _spr_put
			pop ix
			jr  ed_next
		.ed_hide
			push ix
			call _spr_hide
			pop ix
		.ed_next
			ld  de, EN_SIZE
			add ix, de
			ld  hl, _s_n
			inc (hl)
			ld  a, (hl)
			cp  MAXEN + 1
			jr  c, ed_loop
	#endasm
}

/* the enemies' sprite colours for this room: $80 + MODE's ink and BRIGHT */
void enemies_colour (void) {
	for (s_n = 1; s_n <= MAXEN; ++ s_n) {
		gp = en + (s_n - 1) * EN_SIZE;
		s_c = 0x80 | (gp [EN_MODE] & 0x47);
		spr_colour ();
	}
}
