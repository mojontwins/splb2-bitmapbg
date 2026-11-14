/*
 * printer.h - CJMC93: everything that draws. Tiles, texts, rooms, the
 * backdrops and pictures, lines, and the sprites (creating and moving
 * them). #included once by cjmc93.c, after the game's globals.
 *
 * Every function is inline assembly that calls splib2's own routines
 * (SPBgPutTile, SPMoveSprAbs...) with their arguments in registers, not
 * the C wrappers (sp_BgPutTile...), which take them from the stack. As in
 * the rest of the game: no locals, no parameters; globals in, globals out.
 *
 *   Function        In                          Changes
 *   put_raw         t_row t_col t_attr t_tile   (screen)
 *   put_tile        the same; tile 255 = none   the cell's collision byte
 *   print_str       sp_s, t_row t_col t_attr    sp_s (to the 0), t_col
 *   print_num       t_num, t_row t_col t_attr   t_col (+2)
 *   room_texts      gk (text set, 0 none)       sp_s t_row t_col t_attr
 *   room_fill       ro_*, t_attr                t_row t_col t_tile ro_i ro_j
 *   room_objects    lp                          lp (past the end), ro_*
 *   shadow_clear    -                           the shadow, every attribute
 *   backdrop_draw   gk (BD_*)                   the shadow, attributes
 *   pic_unpack      pic_src pic_y               the shadow
 *   room_part       lp (a room's header)        lp, gk, room_tx
 *   room_draw       room, boxes_gone            the collision map, lp...
 *   hud_draw        room, lives                 t_*
 *   msg_row10       sp_s                        t_*; updates the screen
 *   type_msg        typing                      sp_s t_*; sound, waits
 *   line_xor        ly lx0 lx1                  the shadow
 *   flash_box       frame                       4 attributes
 *   spr_put         s_n s_f s_x s_y             spr_cur spr_on
 *   spr_hide        s_n                         spr_on
 *   spr_hide_all    -                           s_n
 *   spr_create      s_n                         spr spr_cur spr_on gk
 *   draw_sprites    px py pfacing, the actors   s_* an spr_used
 *
 * splib2's entry points used, and their registers:
 *   SPBgPutTile    A row, C column, B attribute, DE the 8 bytes
 *   SPBgAttrFill   A attribute (every cell, all dirty)
 *   SPZX0Depack    HL packed data, DE destination
 *   SPInvalidate   B C top row and column, D E bottom row and column,
 *                  IY clip rectangle (SPClipStruct: the whole screen)
 *   SPMoveSprAbs   IX sprite, BC frame displacement, H row, L column,
 *                  D horizontal pixel, E vertical pixel, IY clip
 *   SPCreateSpr    B rows, C type | plane, DE first column -> IX sprite
 *   SPAddColSpr    IX sprite, DE column
 *   SPUpdateNow    -
 */

#asm
	LIB SPBgPutTile, SPBgAttrFill, SPZX0Depack, SPInvalidate, SPUpdateNow
	LIB SPMoveSprAbs, SPCreateSpr, SPAddColSpr, SPClipStruct, SPNullSprPtr
#endasm

/* ====================================================================== */
/* Tiles and texts                                                         */
/* ====================================================================== */

/* tile t_tile at (t_row, t_col) in colour t_attr. The map is untouched.
   Inside: pr_raw (A = tile, colour t_attr) and pr_put (A = tile, B =
   colour), used by the other functions here. */
void put_raw (void) {
	#asm
		ld  a, (_t_tile)
	.pr_raw
		ld  hl, _t_attr
		ld  b, (hl)
	.pr_put
		ld  l, a
		ld  h, 0
		add hl, hl
		add hl, hl
		add hl, hl
		ld  de, _tiles
		add hl, de
		ex  de, hl                  ; DE = the tile
		ld  a, (_t_col)
		ld  c, a
		ld  a, (_t_row)
		jp  SPBgPutTile
	#endasm
}

/* a room tile: tile t_tile at (t_row, t_col) in colour t_attr. The
   colour also says what the player can do with the cell (gfx/rooms.txt):
   BRIGHT = solid, FLASH = deadly (the water; FLASH itself isn't shown).
   Other colours leave the cell's collision as it is. Tile 255 draws
   nothing: a wall that is part of the backdrop. */
void put_tile (void) {
	#asm
		ld  a, (_t_row)             ; HL = the collision byte:
		ld  l, a                    ; dlist + (row * 32 + col) * 4 + 1
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
		add hl, hl
		add hl, hl
		ld  de, (_dlist)
		add hl, de
		inc hl
		ld  a, (_t_attr)
		ld  b, a
		bit 6, b                    ; BRIGHT: solid
		jr  z, pt_notsolid
		set 0, (hl)                 ; C_SOLID
	.pt_notsolid
		bit 7, b                    ; FLASH: deadly
		jr  z, pt_notdeadly
		set 1, (hl)                 ; C_DEADLY
	.pt_notdeadly
		res 7, b                    ; not shown
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

/* t_num (0..99) at (t_row, t_col), colour t_attr, two characters (a
   space for the tens below 10) */
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
		or  a
		jr  z, pn_space             ; tile 0 = space
		add a, 16                   ; tile 16 = 0
	.pn_space
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

/* the room texts set gk (gfx/texts.txt; 0 = none): ROW COL ATTR text 0,
   ..., FF. Nothing becomes solid. A space on black paper isn't drawn, so
   the picture or the backdrop under it shows. */
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
/*   0xFF ends the room.                                                   */
/* ====================================================================== */

unsigned char ro_x, ro_y, ro_x2, ro_y2, ro_w, ro_h, ro_code;
unsigned char ro_i, ro_j, ro_rowcode;

/* fill (ro_x, ro_y)-(ro_x2, ro_y2) with the ro_w x ro_h figure of tiles
   from ro_code (255: nothing drawn), colour t_attr */
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
		ld  a, c
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

/* ====================================================================== */
/* Backdrops and pictures                                                  */
/* ====================================================================== */

/* shadow screen: clear, every cell PAPER 0 INK 7 and dirty */
void shadow_clear (void) {
	#asm
		ld  hl, SHADOW
		ld  de, SHADOW + 1
		ld  bc, 6143
		ld  (hl), 0
		ldir
		ld  a, 7
		jp  SPBgAttrFill
	#endasm
}

/* backdrop gk (BD_*): depacked straight into the shadow screen (the
   original copied it to the screen with RANDOMIZE USR 61913); its
   attributes into the display list, every cell dirty */
void backdrop_draw (void) {
	#asm
		ld  a, (_gk)
		cp  BD_NONE
		ret z
		cp  BD_BLACK
		jp  z, _shadow_clear
		add a, a
		ld  e, a
		ld  d, 0
		push de
		ld  hl, _bd_bitmap
		add hl, de
		ld  a, (hl)
		inc hl
		ld  h, (hl)
		ld  l, a
		ld  de, SHADOW
		call SPZX0Depack            ; the bitmap
		pop de
		ld  hl, _bd_attrs
		add hl, de
		ld  a, (hl)
		inc hl
		ld  h, (hl)
		ld  l, a
		ld  de, _attr_buf
		call SPZX0Depack            ; the attributes
		ld  a, 7
		call SPBgAttrFill           ; everything dirty
		ld  hl, _attr_buf           ; attributes -> display list byte 0
		ld  de, (_dlist)
		ld  bc, 768
	.bd_copy
		ld  a, (hl)
		ld  (de), a
		inc hl
		inc de
		inc de
		inc de
		inc de
		dec bc
		ld  a, b
		or  c
		jr  nz, bd_copy
	#endasm
}

/* PICTURES: a decoration in the game area (gfx/pictures), unpacked
   straight into the shadow screen.

   The picture is packed as plain lines of 24 bytes, top to bottom. In the
   shadow (display-file layout) those lines are 24 bytes apart from each
   other in no simple way, so sp_ZX0 can't write them: it writes one
   unbroken run. pic_unpack is the standard ZX0 (v2) decoder with its two
   LDIRs replaced:
     - literals go through zw_put, which writes a byte at the destination
       cursor and moves it on: next column, or at column 24 the start of
       the next pixel line;
     - a copy from "offset bytes back" becomes a source cursor: the
       offset is split into lines and columns (offset = 24 * lines +
       columns) and taken from the destination's line and column
       (zw_from). Then zw_get reads and moves on the same way.
   No intermediate buffer. About 5 frames for a full picture.

   In: pic_src = the packed picture, pic_y = its top pixel line. The
   picture covers columns 4-27. Colours are not touched: the backdrop's
   (see PLAY_INK in tools/conv.py). */
unsigned char *pic_src;
unsigned char pic_y;

void pic_unpack (void) {
	#asm
		ld  a, (_pic_y)
		ld  (zw_dy), a
		call zw_addr
		ld  (zw_dp), hl
		xor a
		ld  (zw_dc), a
		ld  hl, (_pic_src)
		ld  bc, $ffff               ; last offset = 1
		push bc
		inc bc
		ld  a, $80
	.zw_literals
		call zw_elias               ; BC = how many literals
		push af
	.zw_lit_loop
		ld  a, (hl)
		inc hl
		call zw_put
		dec bc
		ld  a, b
		or  c
		jr  nz, zw_lit_loop
		pop af
		add a, a                    ; then a copy: last offset or a new one?
		jr  c, zw_new_offset
		call zw_elias               ; BC = length
	.zw_copy
		ex  (sp), hl                ; HL = offset (negative), source kept
		push hl
		push af
		push bc
		call zw_from                ; source cursor = destination - offset
		pop bc
	.zw_copy_loop
		call zw_get
		call zw_put
		dec bc
		ld  a, b
		or  c
		jr  nz, zw_copy_loop
		pop af
		pop hl
		ex  (sp), hl                ; offset kept, HL = source again
		add a, a                    ; then literals, or a new offset?
		jr  nc, zw_literals
	.zw_new_offset
		pop bc                      ; forget the last offset
		ld  c, $fe
		call zw_elias_loop          ; offset, high part
		inc c
		ret z                       ; the end
		ld  b, c
		ld  c, (hl)                 ; offset, low part
		inc hl
		rr  b
		rr  c
		push bc                     ; the new offset (negative)
		ld  bc, 1
		call nc, zw_elias_backtrack ; length
		inc bc
		jr  zw_copy
	.zw_elias                       ; interlaced Elias gamma number -> BC
		inc c
	.zw_elias_loop
		add a, a
		jr  nz, zw_elias_skip
		ld  a, (hl)
		inc hl
		rla
	.zw_elias_skip
		ret c
	.zw_elias_backtrack
		add a, a
		rl  c
		rl  b
		jr  zw_elias_loop

	; zw_addr: A = pixel line -> HL = its column 4 in the shadow (uses E)
	.zw_addr
		ld  e, a
		and 7
		ld  h, a
		ld  a, e
		rrca
		rrca
		rrca
		and $18
		or  h
		add a, SHADOW / 256
		ld  h, a
		ld  a, e
		and $38
		rlca
		rlca
		add a, 4
		ld  l, a
		ret

	; zw_put: A -> destination cursor, which moves on (keeps BC HL)
	.zw_put
		ld  de, (zw_dp)
		ld  (de), a
		inc e
		ld  (zw_dp), de
		ld  a, (zw_dc)
		inc a
		cp  24
		jr  z, zw_put_line
		ld  (zw_dc), a
		ret
	.zw_put_line
		xor a
		ld  (zw_dc), a
		push hl
		ld  a, (zw_dy)
		inc a
		ld  (zw_dy), a
		call zw_addr
		ld  (zw_dp), hl
		pop hl
		ret

	; zw_get: A <- source cursor, which moves on (keeps BC HL)
	.zw_get
		ld  de, (zw_sp)
		ld  a, (de)
		inc e
		ld  (zw_sp), de
		push af
		ld  a, (zw_sc)
		inc a
		cp  24
		jr  nz, zw_get_col
		push hl
		ld  a, (zw_sy)
		inc a
		ld  (zw_sy), a
		call zw_addr
		ld  (zw_sp), hl
		pop hl
		xor a
	.zw_get_col
		ld  (zw_sc), a
		pop af
		ret

	; zw_from: HL = minus the offset -> source cursor (uses all but IX IY)
	.zw_from
		xor a
		sub l
		ld  e, a
		sbc a, a
		sub h
		ld  d, a                    ; DE = offset
		ld  b, 0                    ; B = whole lines back
	.zw_from_10
		ld  a, d                    ; 10 lines (240 bytes) at a time
		or  a
		jr  nz, zw_from_sub
		ld  a, e
		cp  240
		jr  c, zw_from_1
	.zw_from_sub
		ld  a, e
		sub 240
		ld  e, a
		jr  nc, zw_from_nb
		dec d
	.zw_from_nb
		ld  a, b
		add a, 10
		ld  b, a
		jr  zw_from_10
	.zw_from_1
		ld  a, e                    ; then line by line
	.zw_from_1l
		cp  24
		jr  c, zw_from_col
		sub 24
		inc b
		jr  zw_from_1l
	.zw_from_col                    ; A = columns back
		ld  c, a
		ld  a, (zw_dc)
		sub c
		jr  nc, zw_from_c
		add a, 24                   ; from the line before
		inc b
	.zw_from_c
		ld  (zw_sc), a
		ld  c, a
		ld  a, (zw_dy)
		sub b
		ld  (zw_sy), a
		call zw_addr
		ld  a, l
		add a, c
		ld  l, a
		ld  (zw_sp), hl
		ret

	.zw_dp  defw 0                  ; destination: address, column, line
	.zw_dc  defb 0
	.zw_dy  defb 0
	.zw_sp  defw 0                  ; source: the same
	.zw_sc  defb 0
	.zw_sy  defb 0
	#endasm
}

/* ====================================================================== */
/* Rooms                                                                   */
/* ====================================================================== */

/* lives at row 21, column 13 (after the backdrop's "VIDAS:"); none on
   the bridge and at the end */
void hud_draw (void) {
	#asm
		ld  a, (_room)
		cp  R_4_1
		ret z
		cp  R_END
		ret z
		ld  a, 21
		ld  (_t_row), a
		ld  a, 13
		ld  (_t_col), a
		ld  a, 7
		ld  (_t_attr), a
		ld  a, (_lives)
		ld  (_t_num), a
		jp  _print_num
	#endasm
}

/* one room of gfx/rooms.txt, at lp: its header (backdrop, picture,
   texts), its objects, then its texts */
unsigned char room_tx;
void room_part (void) {
	#asm
		ld  hl, (_lp)
		ld  a, (hl)                 ; the backdrop
		inc hl
		ld  (_lp), hl
		ld  (_gk), a
		call _backdrop_draw
		ld  hl, (_lp)
		ld  b, (hl)                 ; the picture
		inc hl
		ld  a, (hl)                 ; the texts
		inc hl
		ld  (_lp), hl
		ld  (_room_tx), a
		ld  a, b
		or  a
		jr  z, rp_nopic
		dec a
		ld  e, a
		ld  d, 0
		ld  hl, _pic_top
		add hl, de
		ld  a, (hl)
		ld  (_pic_y), a
		ld  hl, _pic_data
		add hl, de
		add hl, de
		ld  a, (hl)
		inc hl
		ld  h, (hl)
		ld  l, a
		ld  (_pic_src), hl
		call _pic_unpack
	.rp_nopic
		call _room_objects
		ld  a, (_room_tx)
		ld  (_gk), a
		jp  _room_texts
	#endasm
}

/* draws the current room, whatever it is: no collisions, then the room
   (and, in 1.S, the boxes until one is chosen), then the HUD */
void room_draw (void) {
	#asm
		ld  hl, (_dlist)            ; every collision byte = 0
		inc hl
		ld  de, 4
		ld  bc, 768
	.rd_clear
		ld  (hl), 0
		add hl, de
		dec bc
		ld  a, b
		or  c
		jr  nz, rd_clear
		ld  a, (_room)              ; lp = the room (room_layout [room])
		ld  e, a
		ld  d, 0
		ld  hl, _room_layout
		add hl, de
		ld  a, (hl)
		call rd_lp
		call _room_part
		ld  a, (_room)
		cp  R_1_S
		jr  nz, rd_hud
		ld  a, (_boxes_gone)
		or  a
		jr  nz, rd_hud
		ld  a, L_1_SBOXES
		call rd_lp
		call _room_part
	.rd_hud
		ld  a, (_room)
		cp  R_CARD2
		ret z
		cp  R_CARD3
		ret z
		cp  R_CARD4
		ret z
		jp  _hud_draw
	.rd_lp                          ; A = room of rooms.txt -> lp
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
		ret
	#endasm
}

/* ====================================================================== */
/* Messages                                                                */
/* ====================================================================== */

/* the string at sp_s on row 10 (PAPER 7 INK 0, as lines 3100 and 4010),
   shown at once */
void msg_row10 (void) {
	#asm
		ld  a, 10
		ld  (_t_row), a
		xor a
		ld  (_t_col), a
		ld  a, $38
		ld  (_t_attr), a
		call _print_str
		jp  SPUpdateNow
	#endasm
}

/* "AQUI BOLON MAYOR..." (sub 9200): from row 19, column 0, INK 4, 31
   characters a row. typing = 1: a letter at a time, with a cursor (UDG
   U) and a click; 0: all at once */
unsigned char *msg_guardian = "AQUI BOLON MAYOR:SAL DEL CASTI- LLO MATANDO AL GUARDIAN CON     SPACE.!OJO QUE DISPARA!";
unsigned char typing;

void type_msg (void) {
	#asm
		ld  hl, (_msg_guardian)
		ld  (_sp_s), hl
		ld  a, 19
		ld  (_t_row), a
		xor a
		ld  (_t_col), a
		ld  a, 4
		ld  (_t_attr), a
	.tm_loop
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
		ld  a, (hl)
		cp  31
		jr  nz, tm_typing
		xor a                       ; column 31: a space, next row
		call pr_raw
		xor a
		ld  (_t_col), a
		ld  hl, _t_row
		inc (hl)
	.tm_typing
		ld  a, (_typing)
		or  a
		jr  z, tm_loop
		ld  a, T_UDG + 20           ; the cursor
		call pr_raw
		call SPUpdateNow
		ld  a, 0 - 25 - (BEEP_MIN)  ; BEEP 0.01, -25
		ld  (_bp_n), a
		ld  hl, 10
		ld  (_bp_ms), hl
		call _beep
		ld  a, 2
		ld  (_wf_n), a
		call _wait_frames
		jr  tm_loop
	#endasm
}

/* ====================================================================== */
/* Lines and colours                                                       */
/* ====================================================================== */

/* XOR one pixel line (ly) of columns lx0..lx1-1 in the shadow, and have
   that character row redrawn (the rays of 4.1, as DRAW with OVER 1) */
void line_xor (void) {
	#asm
		ld  a, (_ly)                ; HL = the line in the shadow
		ld  e, a
		and 7
		ld  h, a
		ld  a, e
		rrca
		rrca
		rrca
		and $18
		or  h
		add a, SHADOW / 256
		ld  h, a
		ld  a, e
		and $38
		rlca
		rlca
		ld  l, a
		ld  a, (_lx0)
		ld  c, a
		add a, l
		ld  l, a
		ld  a, (_lx1)
		sub c
		ld  b, a                    ; B = how many bytes
	.lx_loop
		ld  a, (hl)
		cpl
		ld  (hl), a
		inc l
		djnz lx_loop
		ld  a, (_ly)                ; redraw its character row
		rrca
		rrca
		rrca
		and 31
		ld  b, a
		ld  d, a
		ld  c, 0
		ld  e, 31
		ld  iy, SPClipStruct
		jp  SPInvalidate
	#endasm
}

/* 1.6: the box at row 18, column 20 flashes (FLASH 1 in line 610; splib2
   uses the FLASH bit for something else, so ink and paper are swapped
   by hand every FLASH_FRAMES) */
void flash_box (void) {
	#asm
		ld  a, (_frame)
		and FLASH_FRAMES - 1
		ret nz
		ld  hl, (_dlist)
		ld  de, 4 * (18 * 32 + 20)
		add hl, de
		ld  b, 2                    ; two rows
	.fb_row
		push bc
		call fb_swap
		inc hl
		inc hl
		inc hl
		inc hl
		call fb_swap
		ld  de, 31 * 4              ; column 21 -> next row, column 20
		add hl, de
		pop bc
		djnz fb_row
		ld  bc, 18 * 256 + 0
		ld  de, 19 * 256 + 31
		ld  iy, SPClipStruct
		jp  SPInvalidate
	.fb_swap                        ; (HL): swap ink and paper
		ld  a, (hl)
		ld  c, a
		and $C0
		ld  b, a
		ld  a, c
		and 7
		rlca
		rlca
		rlca
		or  b
		ld  b, a
		ld  a, c
		rrca
		rrca
		rrca
		and 7
		or  b
		ld  (hl), a
		ret
	#endasm
}

/* ====================================================================== */
/* Sprites. splib2 redraws every character a sprite covers, so each figure */
/* goes in a sprite of its height (frames: spr_class in assets.h):         */
/* class 0 = 8 px tall (2 characters), 1 = 16 px (3), 2 = 24 px (4).       */
/*   0 the player (16)                                                     */
/*   1 actor 0, tall (24): the knight, the shapeshifter                    */
/*   2 actor 0, 16 px      3 actor 1, 16 px                                */
/*   4 actor 0, 8 px       5 actor 1, 8 px      6 actor 2, 8 px            */
/* ====================================================================== */

#define NSPR 7
struct sp_SS *spr [NSPR];
unsigned char *spr_cur [NSPR];          /* the frame each one shows       */
unsigned char spr_on [NSPR];            /* 1: on screen                   */
unsigned char spr_used [NSPR];          /* draw_sprites: used this frame  */
unsigned char spr_rows [] = { 3, 4, 3, 3, 2, 2, 2 };
/* the sprite of actor n for a frame of class c: slot_spr [n * 3 + c] */
unsigned char slot_spr [] = { 4, 2, 1,   5, 3, 255,   6, 255, 255 };

/* sprite s_n shows frame s_f at pixel (s_x, s_y) */
void spr_put (void) {
	#asm
		ld  a, (_s_f)               ; DE = sprite_frames + spr_off [s_f]
		ld  l, a
		ld  h, 0
		add hl, hl
		ld  de, _spr_off
		add hl, de
		ld  e, (hl)
		inc hl
		ld  d, (hl)
		ld  hl, _sprite_frames
		add hl, de
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
		ld  iy, SPClipStruct
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
		ld  iy, SPClipStruct
		jp  SPMoveSprAbs
	#endasm
}

void spr_hide_all (void) {
	#asm
		xor a
	.sh_loop
		ld  (_s_n), a
		call _spr_hide
		ld  a, (_s_n)
		inc a
		cp  NSPR
		jr  c, sh_loop
	#endasm
}

/* sprite s_n: 3 columns x spr_rows [s_n] rows. A column of a frame is its
   lines + 8 blank lines (the vertical shift), 2 bytes each: rows * 16
   bytes. The third column only receives the pixels shifted out of the
   second one: its graphic is pointed at SPNullSprPtr (which splib2 never
   animates), so frames need no blank third column. */
unsigned char *spr_base;
void spr_create (void) {
	#asm
		ld  a, (_s_n)
		ld  e, a
		ld  d, 0
		ld  hl, _spr_rows
		add hl, de
		ld  a, (hl)
		ld  (_gk), a                ; gk = rows
		sub 2
		ld  c, a                    ; a frame of that height to start with
		ld  hl, _spr_class
		ld  b, 0
	.sc_find
		ld  a, (hl)
		cp  c
		jr  z, sc_found
		inc hl
		inc b
		jr  sc_find
	.sc_found
		ld  l, b                    ; spr_base = sprite_frames + spr_off [B]
		ld  h, 0
		add hl, hl
		ld  de, _spr_off
		add hl, de
		ld  e, (hl)
		inc hl
		ld  d, (hl)
		ld  hl, _sprite_frames
		add hl, de
		ld  (_spr_base), hl
		ex  de, hl
		ld  a, (_s_n)               ; spr_cur [s_n] = it, spr_on [s_n] = 0
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
		ld  a, (_gk)                ; the sprite: masked, plane 1
		ld  b, a
		ld  c, 1                    ; sp_MASK_SPRITE (0) + plane 1
		call SPCreateSpr            ; IX = the sprite
		push ix
		pop de
		ld  a, (_s_n)               ; spr [s_n] = IX
		ld  c, a
		ld  b, 0
		ld  hl, _spr
		add hl, bc
		add hl, bc
		ld  (hl), e
		inc hl
		ld  (hl), d
		ld  a, (_gk)                ; DE = second column: base + rows * 16
		ld  l, a
		ld  h, 0
		add hl, hl
		add hl, hl
		add hl, hl
		add hl, hl
		ld  de, (_spr_base)
		add hl, de
		ex  de, hl
		push de
		call SPAddColSpr            ; second column
		pop de
		call SPAddColSpr            ; third (pointed at the null graphic below)
		ld  h, (ix+6)               ; HL = first char struct
		ld  l, (ix+7)
		ld  a, (_gk)                ; skip the first two columns
		add a, a
		ld  b, a
	.sc_skip
		ld  a, (hl)                 ; next char struct (big endian)
		inc hl
		ld  l, (hl)
		ld  h, a
		djnz sc_skip
		ld  a, (_gk)
		ld  b, a
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

/* every sprite for this frame: the player, and each actor in the sprite
   of its frame's height; the sprites not used are hidden */
void draw_sprites (void) {
	#asm
		xor a
		ld  (_s_n), a
		ld  a, (_pfacing)
		add a, SPR_PLAYER_R
		ld  (_s_f), a
		ld  a, (_px)
		ld  (_s_x), a
		ld  a, (_py)
		ld  (_s_y), a
		call _spr_put
		ld  hl, _spr_used + 1
		ld  b, NSPR - 1
	.ds_clear
		ld  (hl), 0
		inc hl
		djnz ds_clear
		xor a
		ld  (_an), a
	.ds_actor
		ld  a, (_an)
		ld  e, a
		ld  d, 0
		ld  hl, _a_type
		add hl, de
		ld  a, (hl)
		or  a
		jr  z, ds_next              ; A_NONE
		ld  hl, _a_frame
		add hl, de
		ld  a, (hl)
		ld  (_s_f), a
		ld  l, a                    ; C = spr_class [frame]
		ld  h, 0
		ld  bc, _spr_class
		add hl, bc
		ld  c, (hl)
		ld  a, e                    ; s_n = slot_spr [an * 3 + class]
		add a, a
		add a, e
		add a, c
		ld  l, a
		ld  h, 0
		ld  bc, _slot_spr
		add hl, bc
		ld  a, (hl)
		ld  (_s_n), a
		ld  hl, _a_x
		add hl, de
		ld  a, (hl)
		ld  (_s_x), a
		ld  hl, _a_y
		add hl, de
		ld  a, (hl)
		ld  (_s_y), a
		call _spr_put
		ld  a, (_s_n)
		ld  e, a
		ld  d, 0
		ld  hl, _spr_used
		add hl, de
		ld  (hl), 1
	.ds_next
		ld  hl, _an
		inc (hl)
		ld  a, (hl)
		cp  NACT
		jr  c, ds_actor
		ld  a, 1                    ; hide the unused ones
	.ds_hide
		ld  (_s_n), a
		ld  e, a
		ld  d, 0
		ld  hl, _spr_used
		add hl, de
		ld  a, (hl)
		or  a
		call z, _spr_hide
		ld  a, (_s_n)
		inc a
		cp  NSPR
		jr  c, ds_hide
	#endasm
}
