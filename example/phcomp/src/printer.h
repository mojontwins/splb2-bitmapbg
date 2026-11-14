/*
 * Phantomas Compilation - printer.h
 * Everything that draws: tiles, texts, numbers, rooms, the HUD and the
 * sprites. All of it is inline assembly calling splib2's routines
 * directly (not through their C wrappers): the registers they take are
 * listed below. Inputs and outputs are globals (see phcomp.c).
 *
 *   function        reads                       writes
 *   put_raw         t_row t_col t_attr t_tile   the screen (dlist)
 *   put_tile        t_row t_col t_attr t_tile   the screen, cmap
 *   print_str       sp_s t_row t_col t_attr     t_col
 *   print_num       t_num t_row t_col t_attr    t_col
 *   cls_area        t_attr                      play area + cmap
 *   room_texts      gk                          t_*
 *   room_objects    lp                          lp ro_* t_*
 *   room_draw       room_n                      lp + the above
 *   hud_draw        lives game                  t_*
 *   spr_create      s_n                         spr spr_cur
 *   spr_put         s_n s_f s_x s_y             spr_cur
 *   spr_hide        s_n                         spr_on
 *   spr_colour      s_n s_c                     the sprite's char structs
 *
 * splib2 entry points used, and their registers:
 *   SPPrintAtInv   A row, C column, D attribute, E tile; marks it dirty
 *   SPMoveSprAbs   IX sprite, BC frame displacement, H row, L column,
 *                  D horizontal pixel, E vertical pixel, IY clip
 *   SPCreateSpr    B rows, C type | plane, DE first column -> IX sprite
 *   SPAddColSpr    IX sprite, DE column
 *   SPUpdateNow    -
 */

#asm
		LIB SPPrintAtInv, SPUpdateNow
		LIB SPMoveSprAbs, SPCreateSpr, SPAddColSpr, SPNullSprPtr
#endasm

/* ====================================================================== */
/* Tiles and texts                                                         */
/* ====================================================================== */

/* tile t_tile at (t_row, t_col) in colour t_attr; cmap untouched.
   Inside: pr_raw (A = tile, colour t_attr) and pr_put (A = tile, B =
   colour), used by the other functions here. */
void put_raw (void) {
	#asm
			ld  a, (_t_tile)
		.pr_raw
			ld  hl, _t_attr
			ld  b, (hl)
		.pr_put
			ld  e, a
			ld  d, b
			ld  a, (_t_col)
			ld  c, a
			ld  a, (_t_row)
			jp  SPPrintAtInv
	#endasm
}

/* a room tile: tile t_tile at (t_row, t_col) in colour t_attr. The colour
   also says what the cell is (gfx/rooms.txt): BRIGHT = solid, FLASH =
   deadly, both = a platform (solid from above only). FLASH isn't shown.
   Tile 255 draws nothing (an invisible wall). */
void put_tile (void) {
	#asm
			ld  a, (_t_row)             ; HL = cmap + row * 32 + col
			ld  l, a
			ld  h, 0
			add hl, hl
			add hl, hl
			add hl, hl
			add hl, hl
			add hl, hl
			ld  a, (_t_col)
			ld  e, a
			ld  d, 0
			add hl, de
			ld  de, _cmap
			add hl, de
			ld  a, (_t_attr)
			ld  b, a
			rlca                        ; bit 7 (FLASH) -> bit 1,
			rlca                        ; bit 6 (BRIGHT) -> bit 0
			and 3
			ld  (hl), a                 ; C_SOLID 1, C_DEADLY 2, C_PLATFORM 3
			res 7, b                    ; FLASH not shown
			ld  a, (_t_tile)
			cp  255
			jp  nz, pr_put
	#endasm
}

/* zero-terminated string at sp_s from (t_row, t_col), colour t_attr */
void print_str (void) {
	#asm
		.ps_loop
			ld  hl, (_sp_s)
			ld  a, (hl)
			or  a
			ret z
			inc hl
			ld  (_sp_s), hl
			sub 32
			call pr_raw
			ld  hl, _t_col
			inc (hl)
			jr  ps_loop
	#endasm
}

/* t_num (0..99) at (t_row, t_col), colour t_attr, two digits */
void print_num (void) {
	#asm
			ld  a, (_t_num)
			ld  b, 0                    ; B = tens
		.pn_tens
			cp  10
			jr  c, pn_units
			sub 10
			inc b
			jr  pn_tens
		.pn_units
			push af                     ; the units
			ld  a, b
			add a, 16                   ; tile 16 = 0
			call pr_raw
			ld  hl, _t_col
			inc (hl)
			pop af
			add a, 16
			call pr_raw
			ld  hl, _t_col
			inc (hl)
	#endasm
}

/* the play area (rows 0-21) blank (tile 0) in colour t_attr, and the
   whole collision map free */
void cls_area (void) {
	#asm
			ld  hl, _cmap
			ld  de, _cmap + 1
			ld  bc, 767
			ld  (hl), 0
			ldir
			xor a
			ld  (_t_row), a
		.ca_row
			xor a
			ld  (_t_col), a
		.ca_col
			xor a
			call pr_raw
			ld  hl, _t_col
			inc (hl)
			ld  a, (hl)
			cp  32
			jr  nz, ca_col
			ld  hl, _t_row
			inc (hl)
			ld  a, (hl)
			cp  22
			jr  nz, ca_row
	#endasm
}

/* the texts set gk (gfx/texts.txt; 0 = none): ROW COL ATTR text 0, ...,
   FF. Nothing becomes solid. A space on black paper isn't drawn. */
void room_texts (void) {
	#asm
			ld  a, (_gk)
			or  a
			ret z
			dec a
			ld  l, a                    ; HL = texts_data + texts_off [gk - 1]
			ld  h, 0
			add hl, hl
			ld  de, _texts_off
			add hl, de
			ld  e, (hl)
			inc hl
			ld  d, (hl)
			ld  hl, _texts_data
			add hl, de
		.rt_text
			ld  a, (hl)
			cp  $FF
			ret z
			ld  (_t_row), a
			inc hl
			ld  a, (hl)
			ld  (_t_col), a
			inc hl
			ld  a, (hl)
			ld  (_t_attr), a
			inc hl
		.rt_char
			ld  a, (hl)
			inc hl
			or  a
			jr  z, rt_text
			push hl
			sub 32
			jr  nz, rt_draw
			ld  a, (_t_attr)            ; a space: only on a coloured paper
			and $38
			jr  z, rt_skip
			xor a
		.rt_draw
			call pr_raw
		.rt_skip
			ld  hl, _t_col
			inc (hl)
			pop hl
			jr  rt_char
	#endasm
}

/* ====================================================================== */
/* Room objects (gfx/rooms.txt, made with the room editor; compact         */
/* encoding, see tools/conv.py). Each object:                              */
/*   X Y TYPE CODE ATTR [X2 Y2]                                            */
/*     TYPE 0                  char: one cell                              */
/*     TYPE 1                  rect: (X,Y)-(X2,Y2) filled with CODE        */
/*     0x80|(W-1)<<3|(H-1)     figure: one W x H figure of the tiles       */
/*                             CODE .. CODE+W*H-1, row by row              */
/*     0x40|(W-1)<<3|(H-1)     pattern: (X,Y)-(X2,Y2) filled with it       */
/*     TYPE 2                  zigzag: (X,Y)-(X2,Y2) filled with CODE and  */
/*                             CODE+1 taking turns, each row starting one  */
/*                             cell on (staggered bricks of two halves)    */
/*   0xFF ends the room.                                                   */
/* ====================================================================== */

unsigned char ro_x, ro_y, ro_x2, ro_y2, ro_w, ro_h, ro_code;
unsigned char ro_i, ro_j, ro_rowcode, ro_zz;

/* fill (ro_x, ro_y)-(ro_x2, ro_y2) with the ro_w x ro_h figure of tiles
   from ro_code (255: nothing drawn), colour t_attr; or, if ro_zz, with
   ro_code and ro_code + 1 taking turns (the zigzag) */
void room_fill (void) {
	#asm
			ld  a, (_ro_y)
			ld  (_t_row), a
			ld  a, (_ro_code)
			ld  (_ro_rowcode), a        ; first tile of the figure row
			xor a
			ld  (_ro_j), a
		.rf_row
			ld  a, (_ro_x)
			ld  (_t_col), a
			xor a
			ld  (_ro_i), a
		.rf_cell
			ld  a, (_ro_zz)
			or  a
			jr  z, rf_figure
			ld  a, (_t_col)             ; zigzag: code + ((column - X +
			ld  hl, _t_row              ; row - Y) AND 1)
			add a, (hl)
			ld  hl, _ro_x
			sub (hl)
			ld  hl, _ro_y
			sub (hl)
			and 1
			ld  hl, _ro_code
			add a, (hl)
			jr  rf_put
		.rf_figure
			ld  a, (_ro_code)
			cp  255
			jr  z, rf_put               ; tile 255 stays 255
			ld  a, (_ro_rowcode)
			ld  hl, _ro_i
			add a, (hl)
		.rf_put
			ld  (_t_tile), a
			call _put_tile
			ld  hl, _ro_i               ; next column of the figure
			inc (hl)
			ld  a, (_ro_w)
			cp  (hl)
			jr  nz, rf_i_ok
			ld  (hl), 0
		.rf_i_ok
			ld  hl, _t_col
			ld  a, (_ro_x2)
			cp  (hl)
			jr  z, rf_row_done
			inc (hl)
			jr  rf_cell
		.rf_row_done
			ld  hl, _ro_j               ; next row of the figure
			inc (hl)
			ld  a, (_ro_h)
			cp  (hl)
			jr  nz, rf_j_ok
			ld  (hl), 0
			ld  a, (_ro_code)
			jr  rf_j_set
		.rf_j_ok
			ld  a, (_ro_w)
			ld  hl, _ro_rowcode
			add a, (hl)
		.rf_j_set
			ld  (_ro_rowcode), a
			ld  hl, _t_row
			ld  a, (_ro_y2)
			cp  (hl)
			ret z
			inc (hl)
			jr  rf_row
	#endasm
}

/* draws the objects at lp, up to the end marker; lp ends after it */
void room_objects (void) {
	#asm
		.ro_loop
			ld  hl, (_lp)
			ld  a, (hl)
			cp  $FF
			jr  z, ro_end
			ld  (_ro_x), a
			inc hl
			ld  a, (hl)
			ld  (_ro_y), a
			inc hl
			ld  c, (hl)                 ; C = type
			inc hl
			ld  a, (hl)
			ld  (_ro_code), a
			inc hl
			ld  a, (hl)
			ld  (_t_attr), a
			inc hl
			ld  a, 1
			ld  (_ro_w), a
			ld  (_ro_h), a
			xor a
			ld  (_ro_zz), a
			ld  a, c
			cp  2                       ; zigzag: X2 Y2 follow
			jr  nz, ro_notzz
			ld  (_ro_zz), a
			jr  ro_x2y2
		.ro_notzz
			and $C0
			jr  z, ro_simple
			ld  a, c                    ; figure, pattern: W = bits 5-3 + 1,
			rrca                        ; H = bits 2-0 + 1
			rrca
			rrca
			and 7
			inc a
			ld  (_ro_w), a
			ld  a, c
			and 7
			inc a
			ld  (_ro_h), a
			bit 7, c
			jr  z, ro_x2y2              ; pattern: X2 Y2 follow
			jr  ro_one                  ; figure: one copy
		.ro_simple
			ld  a, c
			or  a
			jr  nz, ro_x2y2             ; rect
		.ro_one                         ; char, figure: X2 = X + W - 1...
			ld  a, (_ro_w)
			ld  b, a
			ld  a, (_ro_x)
			add a, b
			dec a
			ld  (_ro_x2), a
			ld  a, (_ro_h)
			ld  b, a
			ld  a, (_ro_y)
			add a, b
			dec a
			ld  (_ro_y2), a
			jr  ro_fill
		.ro_x2y2
			ld  a, (hl)
			ld  (_ro_x2), a
			inc hl
			ld  a, (hl)
			ld  (_ro_y2), a
			inc hl
		.ro_fill
			ld  (_lp), hl
			call _room_fill
			jr  ro_loop
		.ro_end
			inc hl
			ld  (_lp), hl
	#endasm
}

/* lp = the room room_n of rooms.txt (its header) */
void room_lp (void) {
	#asm
			ld  a, (_room_n)
			ld  l, a
			ld  h, 0
			add hl, hl
			ld  de, _rooms_off
			add hl, de
			ld  e, (hl)
			inc hl
			ld  d, (hl)
			ld  hl, _rooms_data
			add hl, de
			ld  (_lp), hl
	#endasm
}

/* room room_n of rooms.txt: a blank play area, its backdrop (another room
   of rooms.txt, drawn first), its objects, its texts */
unsigned char room_tx;
void room_draw (void) {
	#asm
			ld  a, $07                  ; black paper, white ink
			ld  (_t_attr), a
			call _cls_area
			call _room_lp
			ld  hl, (_lp)
			ld  a, (hl)                 ; backdrop: 0 none, else room + 1
			inc hl
			ld  b, (hl)                 ; the texts set
			inc hl
			ld  (_lp), hl
			ld  hl, _room_tx
			ld  (hl), b
			or  a
			jr  z, rd_objects
			ld  hl, (_lp)
			push hl
			ld  hl, _room_n
			ld  c, (hl)
			push bc
			dec a
			ld  (hl), a
			call _room_lp
			ld  hl, (_lp)               ; skip the backdrop header
			inc hl
			inc hl
			ld  (_lp), hl
			call _room_objects
			pop bc
			ld  a, c
			ld  (_room_n), a
			pop hl
			ld  (_lp), hl
		.rd_objects
			call _room_objects
			ld  a, (_room_tx)
			ld  (_gk), a
			jp  _room_texts
	#endasm
}

/* ====================================================================== */
/* The HUD: rows 22-23. Row 22 a line, row 23 the game and the lives.      */
/* ====================================================================== */

unsigned char *hud_vidas = "VIDAS";

void hud_draw (void) {
	t_row = 22; t_attr = 0x05;
	for (t_col = 0; t_col < 32; ++ t_col) {
		#asm
				ld  a, 95 - 32          ; "_"
				call pr_raw
		#endasm
	}
	t_row = 23; t_col = 0; t_attr = 0x46;
	sp_s = g_menu; print_str ();
	hud_lives ();
}

void hud_lives (void) {
	t_row = 23; t_col = 25; t_attr = 0x47;
	sp_s = hud_vidas; print_str ();
	t_col = 30; t_num = lives; t_attr = 0x46; print_num ();
}

/* ====================================================================== */
/* Sprites: 0 the player, 1-4 the enemies. All 16x16 masked figures in a   */
/* sprite 3 characters tall and 3 wide (the third column of each is        */
/* splib2's null graphic: frames need no blank third column).              */
/* A frame is 96 bytes in sprite_frames, after 16 blank bytes.             */
/* ====================================================================== */

#define NSPR 5
struct sp_SS *spr [NSPR];
unsigned char *spr_cur [NSPR];          /* the frame each one shows       */
unsigned char spr_on [NSPR];            /* 1: on screen                   */

/* HL = sprite_frames + 16 + A * 96 */
void frame_addr (void) {
	#asm
		.fr_addr
			ld  l, a
			ld  h, 0
			add hl, hl
			add hl, hl
			add hl, hl
			add hl, hl
			add hl, hl                  ; A * 32
			ld  d, h
			ld  e, l
			add hl, hl                  ; A * 64
			add hl, de                  ; A * 96
			ld  de, _sprite_frames + 16
			add hl, de
			ret
	#endasm
}

/* sprite s_n shows frame s_f at pixel (s_x, s_y) */
void spr_put (void) {
	#asm
			ld  a, (_s_f)
			call fr_addr
			ex  de, hl                  ; DE = the new frame
			ld  a, (_s_n)
			add a, a
			ld  c, a
			ld  b, 0
			ld  hl, _spr_cur
			add hl, bc
			ld  c, (hl)                 ; BC = the frame it shows
			ld  (hl), e
			inc hl
			ld  b, (hl)
			ld  (hl), d                 ; spr_cur [s_n] = the new one
			ex  de, hl
			and a
			sbc hl, bc
			push hl                     ; the displacement
			ld  a, (_s_n)
			ld  e, a
			ld  d, 0
			ld  hl, _spr_on
			add hl, de
			ld  (hl), 1
			ld  hl, _spr                ; IX = spr [s_n]
			add hl, de
			add hl, de
			ld  a, (hl)
			inc hl
			ld  h, (hl)
			ld  l, a
			push hl
			pop ix
			ld  a, (_s_x)               ; L = column, D = pixel in it
			ld  l, a
			and 7
			ld  d, a
			ld  a, l
			rrca
			rrca
			rrca
			and 31
			ld  l, a
			ld  a, (_s_y)               ; H = row, E = pixel in it
			ld  h, a
			and 7
			ld  e, a
			ld  a, h
			rrca
			rrca
			rrca
			and 31
			ld  h, a
			pop bc
			ld  iy, _clip
			jp  SPMoveSprAbs
	#endasm
}

/* sprite s_n off the screen (column 34) */
void spr_hide (void) {
	#asm
			ld  a, (_s_n)
			ld  e, a
			ld  d, 0
			ld  hl, _spr_on
			add hl, de
			ld  a, (hl)
			or  a
			ret z
			ld  (hl), 0
			ld  hl, _spr
			add hl, de
			add hl, de
			ld  a, (hl)
			inc hl
			ld  h, (hl)
			ld  l, a
			push hl
			pop ix
			ld  bc, 0
			ld  hl, 34                  ; row 0, column 34
			ld  de, 0
			ld  iy, _clip
			jp  SPMoveSprAbs
	#endasm
}

void spr_hide_all (void) {
	for (s_n = 0; s_n < NSPR; ++ s_n) spr_hide ();
}

/* sprite s_n: masked, 3 x 3 characters, showing frame 0 */
void spr_create (void) {
	#asm
			xor a
			call fr_addr
			ex  de, hl                  ; DE = frame 0
			ld  a, (_s_n)
			ld  c, a
			ld  b, 0
			ld  hl, _spr_on
			add hl, bc
			ld  (hl), 0
			ld  hl, _spr_cur
			add hl, bc
			add hl, bc
			ld  (hl), e
			inc hl
			ld  (hl), d
			push de
			ld  b, 3                    ; masked (type 0), plane 1
			ld  c, 1
			call SPCreateSpr            ; IX = the sprite
			push ix
			pop de
			ld  a, (_s_n)
			ld  c, a
			ld  b, 0
			ld  hl, _spr
			add hl, bc
			add hl, bc
			ld  (hl), e
			inc hl
			ld  (hl), d
			pop hl                      ; DE = second column: frame + 48
			ld  de, 48
			add hl, de
			ex  de, hl
			push de
			call SPAddColSpr            ; second column
			pop de
			call SPAddColSpr            ; third (pointed at the null graphic)
			ld  h, (ix+6)               ; HL = first char struct
			ld  l, (ix+7)
			ld  b, 6                    ; skip the first two columns
		.sc_skip
			ld  a, (hl)                 ; next char struct (big endian)
			inc hl
			ld  l, (hl)
			ld  h, a
			djnz sc_skip
			ld  b, 3
			ld  de, SPNullSprPtr
		.sc_null
			push hl
			ld  a, l                    ; graphic pointer at +7
			add a, 7
			ld  l, a
			jr  nc, sc_nc
			inc h
		.sc_nc
			ld  (hl), e
			inc hl
			ld  (hl), d
			pop hl
			ld  a, (hl)
			inc hl
			ld  l, (hl)
			ld  h, a
			djnz sc_null
	#endasm
}

/* sprite s_n in colour s_c: $80 + INK (+ 64 BRIGHT): the ink on the cell's
   paper; 0: the cell's own colours (splib2tc, SPRCOLOUR) */
void spr_colour (void) {
	#asm
			ld  a, (_s_n)
			ld  e, a
			ld  d, 0
			ld  hl, _spr
			add hl, de
			add hl, de
			ld  a, (hl)
			inc hl
			ld  h, (hl)
			ld  l, a
			push hl
			pop ix
			ld  h, (ix+6)               ; HL = first char struct
			ld  l, (ix+7)
			ld  b, 9
			ld  a, (_s_c)
			ld  c, a
		.scol_loop
			push hl
			ld  de, 10                  ; colour at +10
			add hl, de
			ld  (hl), c
			pop hl
			ld  a, (hl)                 ; next char struct (big endian)
			inc hl
			ld  l, (hl)
			ld  h, a
			djnz scol_loop
	#endasm
}
