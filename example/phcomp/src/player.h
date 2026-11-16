/*
 * Phantomas Compilation - player.h
 * The player: walking, the two jumps of pheng (high and long), falling,
 * flying (rooms marked "fly" in games.txt), the collision map and the
 * screen edges. All of it is assembly: it runs every frame.
 *
 * Positions are pixels: (px, py) is the top-left corner of the 16x16
 * figure. The play area is 256x176 (rows 0-21): px 0-240, py 0-160.
 *
 *   player_move     keys (k_*), cmap, the room    px py p* exit_dir dead
 *   player_touch    px py, cmap                   dead (deadly cells)
 *
 * The collision map (cmap, 32x22 bytes, see put_tile in printer.h):
 *   0 free, C_SOLID 1, C_DEADLY 2, C_PLATFORM 3 (solid from above only).
 */

/* The jumps, as in Phantomas (Dinamic): a fixed arc, frame by frame, a
   pair each: pixels down (negative: up), pixels across (in the direction
   of the jump). 128 ends the arc: from there it falls (fall_tab), still
   drifting a pixel a frame. Landing anywhere ends it before.
     high (Q, 9, SPACE, 0): 34 px up (onto a platform 4 characters
                            higher), 16 px across back at the same height
     long (A, 8):           16 px up, 64 px across (32 up, 32 down)
   Both go the way the player faces: there is no jumping straight up.
   (pheng: 8 frames up at 4 or 2 px, 8 down; the BASIC originals jumped 5
   or 6 characters straight up.) tools/reach.py reads these tables. */
unsigned char jump_hi [] = {
	-6, 0, -5, 0, -5, 1, -4, 1, -4, 1, -3, 1, -3, 1, -2, 1, -1, 1, -1, 1,
	1, 1, 1, 1, 2, 1, 3, 1, 3, 1, 4, 1, 5, 1, 5, 1, 5, 0, 5, 0,
	128
};
unsigned char jump_lo [] = {
	-4, 4, -3, 4, -3, 6, -2, 4, -2, 4, -1, 6, -1, 4,
	1, 4, 1, 6, 2, 4, 2, 4, 3, 6, 3, 4, 4, 4,
	128
};
/* falling: pixels down a frame, frame after frame; the last one repeats */
unsigned char fall_tab [] = { 1, 2, 2, 3, 3, 4, 4, 5, 6 };
#define FALL_LAST       8
#define FALL_AFTER_JUMP 6               /* where a jump's fall goes on   */
#define WALK_SPEED      2
#define FLY_SPEED       2

/* walking animation (pheng's phwc): frame offsets over 8 steps */
unsigned char walk_anim [] = { 0, 0, 1, 1, 0, 0, 2, 2 };

#define P_GROUND        0
#define P_RISE          1
#define P_FALL          2

/* ====================================================================== */
/* Collision helpers (assembly only; they keep BC unless they say so)      */
/* ====================================================================== */

unsigned char cm_n, cm_col, cm_row;

void cm_lib (void) {
	#asm
		; A = cmap [row A][column C], 0 outside rows 0-21 / columns 0-31.
		; Uses DE HL.
		.cm_cell
			cp  22
			jr  nc, cm_zero
			ld  l, a
			ld  a, c
			cp  32
			jr  nc, cm_zero
			ld  h, 0
			add hl, hl
			add hl, hl
			add hl, hl
			add hl, hl
			add hl, hl
			ld  e, c
			ld  d, 0
			add hl, de
			ld  de, _cmap
			add hl, de
			ld  a, (hl)
			ret
		.cm_zero
			xor a
			ret

		; Column C, the rows the player covers (py >> 3 .. (py + 15) >> 3):
		; carry set if one is C_SOLID. Uses A B DE HL.
		.cm_col_solid
			ld  a, (_py)
			ld  b, 2
			and 7
			jr  z, ccs_two
			inc b
		.ccs_two
			ld  a, (_py)
			rrca
			rrca
			rrca
			and 31
		.ccs_loop
			push af
			call cm_cell
			cp  1
			jr  z, ccs_hit
			pop af
			inc a
			djnz ccs_loop
			and a
			ret
		.ccs_hit
			pop af
			scf
			ret

		; Row A, the columns the player covers (px >> 3 .. (px + 15) >> 3):
		; A = the OR of their cells, and E = 1 if one is C_SOLID (not a
		; platform). Uses B C D HL.
		.cm_row_or
			ld  (_cm_row), a
			ld  a, (_px)
			ld  b, 2
			and 7
			jr  z, cro_two
			inc b
		.cro_two
			ld  a, (_px)
			rrca
			rrca
			rrca
			and 31
			ld  c, a
			xor a
			ld  (_cm_n), a
			ld  (_cm_col), a            ; cm_col: 1 if a C_SOLID was seen
		.cro_loop
			ld  a, (_cm_row)
			call cm_cell
			cp  1
			jr  nz, cro_notsolid
			ld  (_cm_col), a
		.cro_notsolid
			ld  hl, _cm_n
			or  (hl)
			ld  (hl), a
			inc c
			djnz cro_loop
			ld  a, (_cm_col)
			ld  e, a
			ld  a, (_cm_n)
			ret
	#endasm
}

/* ====================================================================== */
/* Moving one pixel (assembly). Each sets exit_dir at a screen edge (the   */
/* room has an exit there) or stops there (it hasn't); carry set means     */
/* "couldn't move".                                                        */
/* ====================================================================== */

void step_lib (void) {
	#asm
		.step_left
			ld  a, (_px)
			or  a
			jr  nz, sl_inside
			ld  a, (_ex_l)              ; at the left edge
			cp  255
			jr  z, step_no
			ld  a, 1
			ld  (_exit_dir), a
			jr  step_no
		.sl_inside
			and 7
			jr  nz, sl_go               ; inside a cell: nothing new
			ld  a, (_px)
			rrca
			rrca
			rrca
			and 31
			dec a
			ld  c, a
			call cm_col_solid
			ret c
		.sl_go
			ld  hl, _px
			dec (hl)
			and a
			ret

		.step_right
			ld  a, (_px)
			cp  240
			jr  c, sr_inside
			ld  a, (_ex_r)              ; at the right edge
			cp  255
			jr  z, step_no
			ld  a, 2
			ld  (_exit_dir), a
			jr  step_no
		.sr_inside
			and 7
			jr  nz, sr_go
			ld  a, (_px)
			rrca
			rrca
			rrca
			and 31
			add a, 2
			ld  c, a
			call cm_col_solid
			ret c
		.sr_go
			ld  hl, _px
			inc (hl)
			and a
			ret

		.step_no
			scf
			ret

		.step_up
			ld  a, (_py)
			or  a
			jr  nz, su_inside
			ld  a, (_ex_u)              ; at the top
			cp  255
			jr  z, step_no
			ld  a, 3
			ld  (_exit_dir), a
			jr  step_no
		.su_inside
			and 7
			jr  nz, su_go
			ld  a, (_py)
			rrca
			rrca
			rrca
			and 31
			dec a
			call cm_row_or
			ld  a, e                    ; only C_SOLID stops you from below
			or  a
			jr  nz, step_no
		.su_go
			ld  hl, _py
			dec (hl)
			and a
			ret

		; down: carry set = landed (something under the feet) or left the
		; room (exit_dir) or fell off the bottom with no exit (dead)
		.step_down
			ld  a, (_py)
			cp  160
			jr  c, sd_inside
			ld  a, (_ex_d)              ; at the bottom
			cp  255
			jr  z, sd_fell
			ld  a, 4
			ld  (_exit_dir), a
			jr  step_no
		.sd_fell
			ld  a, 1
			ld  (_dead), a
			jr  step_no
		.sd_inside
			and 7
			jr  nz, sd_go
			call on_floor
			jr  nz, step_no
		.sd_go
			ld  hl, _py
			inc (hl)
			and a
			ret

		; NZ if the player stands on something: a solid cell or a platform
		; cell under the feet (only when py is a multiple of 8), or a
		; moving platform (p_plat, see enemies.h)
		.on_floor
			ld  a, (_p_plat)
			cp  255
			jr  nz, of_yes
			ld  a, (_py)
			ld  b, a
			and 7
			jr  nz, of_no               ; between cells: nothing
			ld  a, b
			rrca
			rrca
			rrca
			and 31
			add a, 2
			call cm_row_or
			and 1                       ; C_SOLID or C_PLATFORM
			ret
		.of_yes
			or  1
			ret
		.of_no
			xor a
			ret
	#endasm
}

/* ====================================================================== */
/* The player, one frame                                                   */
/* ====================================================================== */

unsigned char pk_n;

void player_move (void) {
	#asm
			ld  a, (_room_fly)
			or  a
			jp  nz, pm_fly
			ld  a, (_pstate)
			cp  P_RISE
			jp  z, pm_rise
			cp  P_FALL
			jp  z, pm_fall

		; ---------------------------------------------------- on the ground
		; left: walks if facing left, else only turns; the same right
		.pm_ground
			ld  a, (_k_left)
			or  a
			jr  z, pm_noleft
			ld  a, (_pfacing)
			or  a
			jr  z, pm_goleft
			xor a                       ; facing right: turn
			ld  (_pfacing), a
			jr  pm_still
		.pm_goleft
			ld  b, WALK_SPEED
		.pm_wl
			push bc
			call step_left
			pop bc
			jr  c, pm_walked
			djnz pm_wl
			jr  pm_walked
		.pm_noleft
			ld  a, (_k_right)
			or  a
			jr  z, pm_still
			ld  a, (_pfacing)
			or  a
			jr  nz, pm_goright
			ld  a, 4                    ; facing left: turn
			ld  (_pfacing), a
			jr  pm_still
		.pm_goright
			ld  b, WALK_SPEED
		.pm_wr
			push bc
			call step_right
			pop bc
			jr  c, pm_walked
			djnz pm_wr
		.pm_walked
			ld  hl, _pframe             ; next step of the walk
			ld  a, (hl)
			inc a
			and 7
			ld  (hl), a
			jr  pm_jumpkeys
		.pm_still
			xor a
			ld  (_pframe), a

		.pm_jumpkeys
			ld  a, (_k_hi)
			ld  hl, _k_lo
			or  (hl)
			ld  b, a
			ld  a, (_pkpress)           ; a new press only
			or  a
			ld  a, b
			ld  (_pkpress), a
			jr  nz, pm_nojump
			or  a
			jr  z, pm_nojump
			ld  hl, _jump_hi            ; both jumps go the way you face
			ld  a, (_k_hi)
			or  a
			jr  nz, pm_jface
			ld  hl, _jump_lo
		.pm_jface
			ld  c, 1
			ld  a, (_pfacing)
			or  a
			jr  nz, pm_jdir
			ld  c, 255
		.pm_jdir
			ld  a, c                    ; pvx: the direction, 1 / -1 / 0
			ld  (_pvx), a
			ld  (_ptab), hl
			xor a
			ld  (_pjt), a
			ld  a, P_RISE
			ld  (_pstate), a
			ld  a, 1
			ld  (_sfx_n), a
			jp  pm_frame

		.pm_nojump
			call on_floor               ; nothing under the feet: fall
			jp  nz, pm_frame
			xor a
			ld  (_pvx), a
			ld  (_pfi), a
			ld  a, P_FALL
			ld  (_pstate), a
			jp  pm_frame

		; ----------------------------------------------- the arc of the jump
		.pm_rise
			ld  hl, (_ptab)             ; HL = ptab + pjt * 2
			ld  a, (_pjt)
			ld  e, a
			ld  d, 0
			add hl, de
			add hl, de
			ld  a, (hl)                 ; A = down, C = across
			inc hl
			ld  c, (hl)
			cp  128
			jr  z, pm_arcend
			ld  hl, _pjt
			inc (hl)
			or  a
			jr  z, pm_arcx
			bit 7, a
			jr  z, pm_arcdown
			neg                         ; up A pixels
			ld  b, a
		.pm_up
			push bc
			call step_up
			pop bc
			jr  c, pm_bump
			djnz pm_up
		.pm_arcx
			ld  b, c
			call pm_across
			jp  pm_frame
		.pm_arcdown                     ; down A pixels: may land
			ld  b, a
		.pm_adown
			push bc
			call step_down
			pop bc
			jp  c, pm_landed
			djnz pm_adown
			jr  pm_arcx
		.pm_bump                        ; head against the ceiling (or
			ld  a, (_exit_dir)          ; out of the room): falls
			or  a
			jp  nz, pm_frame
			xor a
			ld  (_pvx), a
			ld  (_pfi), a
			jr  pm_tofall
		.pm_arcend                      ; the arc is over: falls on
			ld  a, FALL_AFTER_JUMP
			ld  (_pfi), a
		.pm_tofall
			ld  a, P_FALL
			ld  (_pstate), a
			jp  pm_fall

		; ------------------------------------------------------- falling
		.pm_fall
			ld  a, (_pfi)
			ld  e, a
			ld  d, 0
			ld  hl, _fall_tab
			add hl, de
			ld  b, (hl)
			cp  FALL_LAST
			jr  z, pm_fmax
			inc a
			ld  (_pfi), a
		.pm_fmax
		.pm_down
			push bc
			call step_down
			pop bc
			jr  c, pm_landed
			djnz pm_down
			ld  b, 1                    ; drifting a pixel a frame
			call pm_across
			jp  pm_frame
		.pm_landed
			ld  a, (_exit_dir)
			ld  hl, _dead
			or  (hl)
			jp  nz, pm_frame
			xor a
			ld  (_pstate), a            ; P_GROUND
			ld  (_pvx), a
			ld  (_pframe), a
			ld  a, 2
			ld  (_sfx_n), a
			jp  pm_frame

		; across in the air: B pixels in the direction pvx (1, -1, 0)
		.pm_across
			ld  a, b
			or  a
			ret z
			ld  a, (_exit_dir)
			or  a
			ret nz
			ld  a, (_pvx)
			or  a
			ret z
			bit 7, a
			jr  nz, pma_l
		.pma_r
			push bc
			call step_right
			pop bc
			ret c
			djnz pma_r
			ret
		.pma_l
			push bc
			call step_left
			pop bc
			ret c
			djnz pma_l
			ret

		; ------------------------------------- flying (rooms marked fly)
		.pm_fly
			ld  a, (_k_left)
			or  a
			jr  z, pf_noleft
			xor a
			ld  (_pfacing), a
			ld  b, FLY_SPEED
		.pf_l
			push bc
			call step_left
			pop bc
			jr  c, pf_noleft
			djnz pf_l
		.pf_noleft
			ld  a, (_k_right)
			or  a
			jr  z, pf_noright
			ld  a, 4
			ld  (_pfacing), a
			ld  b, FLY_SPEED
		.pf_r
			push bc
			call step_right
			pop bc
			jr  c, pf_noright
			djnz pf_r
		.pf_noright
			ld  a, (_k_hi)
			or  a
			jr  z, pf_noup
			ld  b, FLY_SPEED
		.pf_u
			push bc
			call step_up
			pop bc
			jr  c, pf_noup
			djnz pf_u
		.pf_noup
			ld  a, (_k_lo)
			or  a
			jr  z, pf_frame
			ld  b, FLY_SPEED
		.pf_d
			push bc
			call step_down
			pop bc
			jr  c, pf_frame
			djnz pf_d
		.pf_frame
			ld  a, (_pfacing)
			add a, 3
			ld  (_pfr), a
			ret

		; ------------------------------------------- the frame to show
		.pm_frame
			ld  a, (_pstate)
			or  a
			jr  z, pmf_walk
			ld  a, (_pfacing)           ; in the air: the jump frame
			add a, 3
			ld  (_pfr), a
			ret
		.pmf_walk
			ld  a, (_pframe)
			ld  e, a
			ld  d, 0
			ld  hl, _walk_anim
			add hl, de
			ld  a, (_pfacing)
			add a, (hl)
			ld  (_pfr), a
	#endasm
}

/* dead = 1 if the player touches a deadly cell: the cells under the
   middle of the figure (px + 4 .. px + 11, py + 2 .. py + 15) */
void player_touch (void) {
	#asm
			ld  a, (_py)
			add a, 2
			rrca
			rrca
			rrca
			and 31
			ld  d, a                    ; D = first row
			ld  a, (_py)
			add a, 15
			rrca
			rrca
			rrca
			and 31
			sub d
			inc a
			ld  b, a                    ; B = rows
		.pt_rows
			push bc
			push de
			ld  a, (_px)
			add a, 4
			rrca
			rrca
			rrca
			and 31
			ld  c, a                    ; first column
			ld  a, (_px)
			add a, 11
			rrca
			rrca
			rrca
			and 31
			sub c
			inc a
			ld  b, a                    ; columns
		.pt_cols
			push bc
			ld  a, d
			push de
			call cm_cell
			pop de
			pop bc
			cp  C_DEADLY
			jr  z, pt_dead
			inc c
			djnz pt_cols
			pop de
			pop bc
			inc d
			djnz pt_rows
			ret
		.pt_dead
			pop de
			pop bc
			ld  a, 1
			ld  (_dead), a
	#endasm
}
