/*
 * Phantomas Lhikoj - C + inline asm port for z88dk (classic, sccz80) + splib2
 *
 * Remake of a lost LOKOsoft game (1991/1992). Original ZX Basic version by
 * The Mojon Twins (Suvleir 3.0 vector screens + fourSpriter 2.1).
 *
 * This port: splib2 (Sprite Pack 2.2) with the BITMAPBG patch. The
 * background is a 6144 byte "shadow screen" (same layout as the Spectrum
 * display file) at SHADOW. SPUpdateNow reads each dirty cell's background
 * from there instead of SPTileArray, then composes masked sprites on top,
 * pixel by pixel, at 25 fps.
 *
 * Memory map
 *   23296 - 23551   msg_attrs (printer buffer)
 *   24200 - ~$D400  this binary (code + splib2 + data + display list)
 *   ...   - $D5C0   stack (a few hundred bytes; the flood fill runs on a
 *                   temporary stack inside SHADOW, see do_fill)
 *   $D600 - $D7FF   splib2 block memory for sprite structs (NUMBLOCKS * 15)
 *   $D800 - $EFFF   SHADOW: background bitmap, screen layout
 *   $F000 - $F100   IM2 vector table
 *   $F101 - $F1C0   ice_map (free gap)
 *   $F1F1 - $F1FF   generic ISR
 *   $F200 - $FFFF   splib2 horizontal rotation tables
 */

#pragma output STACKPTR=54784

#include <spritepack.h>

#asm
	DEFC SHADOW_TOP = $F000
	LIB SPGetScrnAddr
	XREF SPDisplayList
#endasm

#define SHADOW      SP_BGSHADOW     /* = BGSHADOW*256 in SPconfig.def */
#define SHADOW_HI   0xD8
#define NUMBLOCKS   31
#define AD_FREE     (SHADOW - NUMBLOCKS * 15)

#include "assets.h"

/* ------------------------------------------------------------------ */
/* splib2 glue                                                         */
/* ------------------------------------------------------------------ */

void *my_malloc (uint bytes) { return sp_BlockAlloc (0); }
void *u_malloc = my_malloc;
void *u_free = sp_FreeBlock;

struct sp_Rect clip_full = {0, 0, 24, 32};
struct sp_Rect rect;

struct sp_SS *spr [3];
unsigned char *spr_cur [3];
unsigned char spr_on [3];

unsigned char isrc;

/* ISR hook. splib2's generic ISR uses the two bytes *before* a hook as the
   hook list link, so it must be preceded by two spare bytes. */
extern void isr (void);
#asm
	defw 0
._isr
	ld  hl, _isrc
	inc (hl)
	ret
#endasm

/* scratch globals (sccz80 is much happier with globals than locals) */
unsigned char gx, gy, gi, gj, gk, gc, gt;
unsigned int  gw, gw2;
unsigned char *gp, *gp2;

/* asm helper parameters */
unsigned int  cp_src, cp_dst, cp_len;
unsigned char cx, cy, nx, ny;          /* vector pen / target          */
unsigned char vd_dx, vd_dy, vd_xi, vd_yi;

/* ------------------------------------------------------------------ */
/* Low level helpers                                                   */
/* ------------------------------------------------------------------ */

void copy_mem (void) {
	#asm
		ld  hl, (_cp_src)
		ld  de, (_cp_dst)
		ld  bc, (_cp_len)
		ldir
	#endasm
}

void fill_mem (unsigned char v) {
	/* fills cp_len bytes at cp_dst with v */
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
	/* 25 fps: wait until two interrupts have passed since last call */
	while (isrc < 2);
	isrc = 0;
}

void wait_frames (unsigned char n) {
	isrc = 0;
	while (isrc < n);
	isrc = 0;
}

unsigned char rnd_seed = 77;
unsigned char rnd (void) {
	rnd_seed = (rnd_seed * 5) + 17;
	return rnd_seed;
}

/* keys: QAOP + SPACE, as in the original */
unsigned char k_left, k_right, k_up, k_down, k_fire;

void read_keys (void) {
	gc = inport (0xDFFE);
	k_right = !(gc & 1);
	k_left = !(gc & 2);
	if (k_right && k_left) { k_right = k_left = 0; }
	k_up = !(inport (0xFBFE) & 1);
	k_down = !(inport (0xFDFE) & 1);
	k_fire = !(inport (0x7FFE) & 1);
}

unsigned char any_key (void) {
	return ((inport (0x00FE) & 0x1F) != 0x1F);
}

void wait_key (void) {
	while (any_key ());
	while (!any_key ());
}

/* ------------------------------------------------------------------ */
/* Background / shadow screen management                               */
/* ------------------------------------------------------------------ */

void invalidate (unsigned char r, unsigned char c, unsigned char h, unsigned char w) {
	rect.row_coord = r; rect.col_coord = c; rect.height = h; rect.width = w;
	sp_Invalidate (&rect, &clip_full);
}

void invalidate_all (void) {
	sp_Invalidate (&clip_full, &clip_full);
}

/* real screen -> background (bitmap to the shadow, attrs to the display
   list, everything dirty). The real screen must not contain sprites!    */
void bg_capture (void) {
	sp_BgCapture ();
}

/* background -> real screen (removes sprites from the display for a while) */
void bg_show (void) {
	sp_BgShow ();
}

void cls_screen (unsigned char attr) {
	cp_dst = 16384; cp_len = 6144; fill_mem (0);
	cp_dst = 22528; cp_len = 768; fill_mem (attr);
}

/* middle third save / restore (screen layout makes this a single copy) */
unsigned char msg_buffer [2048];
/* 256 bytes: lives in the 48K printer buffer */
/* (sccz80 1.10 miscompiles ((T*)const)[i], so use pointer variables) */
unsigned char *msg_attrs = 23296;

void msg_save (void) {
	cp_src = SHADOW + 2048; cp_dst = msg_buffer; cp_len = 2048; copy_mem ();
	gp = sp_CompDListAddr (8, 0);
	for (gw = 0; gw < 256; gw ++) { msg_attrs [gw] = *gp; gp += 4; }
}

void msg_restore (void) {
	cp_src = msg_buffer; cp_dst = SHADOW + 2048; cp_len = 2048; copy_mem ();
	gp = sp_CompDListAddr (8, 0);
	for (gw = 0; gw < 256; gw ++) { *gp = msg_attrs [gw]; gp += 4; }
	invalidate (8, 0, 8, 32);
}

/* address of char cell (x,y) in a screen-layout buffer whose MSB base is pbase */
unsigned char pbase = SHADOW_HI;

unsigned char *cell_addr (unsigned char x, unsigned char y) {
	return (unsigned char *) (((pbase + (y & 0x18)) << 8) + ((y & 7) << 5) + x);
}

void set_attr (unsigned char x, unsigned char y, unsigned char attr) {
	gp = sp_CompDListAddr (y, x);
	*gp = attr;
	if (pbase == 0x40) *((unsigned char *) (22528 + (y << 5) + x)) = attr;
}

/* put a charset tile into the background */
void put_char (unsigned char x, unsigned char y, unsigned char attr, unsigned char t) {
	gp = cell_addr (x, y);
	gp2 = charset + (t << 3);
	for (gi = 0; gi < 8; gi ++) { *gp = *gp2 ++; gp += 256; }
	set_attr (x, y, attr);
	invalidate (y, x, 1, 1);
}

void put_char4 (unsigned char x, unsigned char y, unsigned char attr, unsigned char t) {
	put_char (x, y, attr, t);
	put_char (x + 1, y, attr, t + 1);
	put_char (x, y + 1, attr, t + 2);
	put_char (x + 1, y + 1, attr, t + 3);
}

/* text: ROM font, BOLD 1 + ITALIC 1 like the ZX Basic original */
unsigned char glyph [8];

void print_char (unsigned char x, unsigned char y, unsigned char c, unsigned char inv) {
	gp2 = (unsigned char *) (0x3C00 + (c << 3));
	for (gi = 0; gi < 8; gi ++) {
		gc = gp2 [gi];
		glyph [gi] = gc | ((gc << 1) | (gc >> 7));            /* bold   */
	}
	glyph [0] >>= 1; glyph [1] >>= 1; glyph [2] >>= 1;         /* italic */
	glyph [5] <<= 1; glyph [6] <<= 1; glyph [7] <<= 1;
	gp = cell_addr (x, y);
	for (gi = 0; gi < 8; gi ++) {
		*gp = inv ? ~glyph [gi] : glyph [gi];
		gp += 256;
	}
}

unsigned char str_len (unsigned char *s) {
	gk = 0; while (s [gk]) gk ++;
	return gk;
}

void print_at (unsigned char x, unsigned char y, unsigned char *s, unsigned char inv) {
	gt = x;
	while (*s) { print_char (x ++, y, *s ++, inv); }
	if (pbase == SHADOW_HI) invalidate (y, gt, 1, x - gt);
}

void print_centered (unsigned char y, unsigned char *s, unsigned char inv) {
	print_at (16 - ((str_len (s) + 1) >> 1), y, s, inv);
}

void clear_box (unsigned char x, unsigned char y, unsigned char w, unsigned char h, unsigned char inv) {
	for (gy = y; gy < y + h; gy ++) for (gx = x; gx < x + w; gx ++) print_char (gx, gy, ' ', inv);
	if (pbase == SHADOW_HI) invalidate (y, x, h, w);
}

/* ------------------------------------------------------------------ */
/* Vector screens (Suvleir 3.0 format)                                 */
/* ------------------------------------------------------------------ */

/* The hardness map lives in the display list's tile byte (byte 1 of each
   4-byte cell), which BITMAPBG leaves unused. */
unsigned char *dlist;

#asm
; plot pixel (cx, cy) on the real screen
.vd_plot
		ld  a, (_cy)
		cp  192
		ret nc
		ld  h, a
		ld  a, (_cx)
		ld  l, a
		ld  a, h
		call SPGetScrnAddr
		ld  a, (de)
		or  b
		ld  (de), a
		ret
#endasm

void vec_plot (void) {
	#asm
		call vd_plot
	#endasm
}

/* Line from (cx,cy) to (nx,ny), same Bresenham decisions as ZX Basic's DRAW */
void vec_draw (void) {
	#asm
		ld  a, (_nx)
		ld  hl, _cx
		sub (hl)
		ld  c, 1
		jr  nc, vd1
		neg
		ld  c, 255
	.vd1
		ld  (_vd_dx), a
		ld  a, c
		ld  (_vd_xi), a

		ld  a, (_ny)
		ld  hl, _cy
		sub (hl)
		ld  c, 1
		jr  nc, vd2
		neg
		ld  c, 255
	.vd2
		ld  (_vd_dy), a
		ld  a, c
		ld  (_vd_yi), a

		ld  a, (_vd_dx)
		ld  b, a
		ld  a, (_vd_dy)
		cp  b
		jr  c, vd_xmajor               ; dy < dx

	; ---- dy >= dx
		ld  a, (_vd_dy)
		srl a
		ld  l, a
		ld  h, 0                       ; hl = error = dy / 2
	.vdy_loop
		ld  a, (_cy)
		ld  b, a
		ld  a, (_ny)
		cp  b
		ret z
		ld  a, (_vd_dx)
		ld  e, a
		ld  d, 0
		or  a
		sbc hl, de                     ; error -= dx
		bit 7, h
		jr  z, vdy_nox
		ld  a, (_vd_dy)
		ld  e, a
		add hl, de                     ; error += dy
		ld  a, (_vd_xi)
		ld  b, a
		ld  a, (_cx)
		add a, b
		ld  (_cx), a
	.vdy_nox
		ld  a, (_vd_yi)
		ld  b, a
		ld  a, (_cy)
		add a, b
		ld  (_cy), a
		push hl
		call vd_plot
		pop hl
		jr  vdy_loop

	; ---- dx > dy
	.vd_xmajor
		ld  a, (_vd_dx)
		srl a
		ld  l, a
		ld  h, 0                       ; hl = error = dx / 2
	.vdx_loop
		ld  a, (_cx)
		ld  b, a
		ld  a, (_nx)
		cp  b
		ret z
		ld  a, (_vd_dy)
		ld  e, a
		ld  d, 0
		or  a
		sbc hl, de                     ; error -= dy
		bit 7, h
		jr  z, vdx_noy
		ld  a, (_vd_dx)
		ld  e, a
		add hl, de                     ; error += dx
		ld  a, (_vd_yi)
		ld  b, a
		ld  a, (_cy)
		add a, b
		ld  (_cy), a
	.vdx_noy
		ld  a, (_vd_xi)
		ld  b, a
		ld  a, (_cx)
		add a, b
		ld  (_cx), a
		push hl
		call vd_plot
		pop hl
		jr  vdx_loop
	#endasm
}

/* Patterned flood fill. SPPFill keeps its queue on the stack (3 bytes per
   entry), so we run it on a temporary stack at the top of the SHADOW buffer:
   whenever we draw vectors, the shadow is about to be recaptured anyway. */
unsigned char fill_x, fill_y;
unsigned int fill_pat;

void do_fill (void) {
	#asm
		LIB SPPFill
		ld  (fill_sp + 1), sp
		ld  sp, SHADOW_TOP
		push ix
		ld  a, (_fill_y)
		ld  h, a
		ld  a, (_fill_x)
		ld  l, a
		ld  de, (_fill_pat)
		ld  bc, 1000                   ; queue depth: 3030 bytes, plenty
		call SPPFill
		pop ix
	.fill_sp
		ld  sp, 0
	#endasm
}

/* Draws a vector screen on the REAL screen and (optionally) decodes its
   hardness layer. */
void vec_parse (unsigned char *bin, unsigned char decode_hard) {
	gw = bin [0] + (bin [1] << 8);
	gp = bin + gw - 2;                     /* end of drawing commands */
	bin += 2;
	while (bin < gp) {
		gc = *bin ++;
		if (gc < 192) {
			nx = *bin ++; ny = gc;
			vec_draw ();
		} else if (gc == 192) {
			cx = *bin ++; cy = *bin ++;
			vec_plot ();
		} else if (gc == 193) {
			gx = *bin ++; gy = *bin ++; gt = *bin ++;
			fill_x = gx; fill_y = gy; fill_pat = fill_patterns + (gt << 3);
			do_fill ();
		}
	}
	if (decode_hard) {
		/* un-RLE: 2 bits value, 6 bits count (0 == 256) */
		gk = 0; gp2 = dlist + 1;
		for (gw = 0; gw < 768; gw ++) {
			if (gk == 0) { gc = *bin ++; gk = gc & 63; gt = gc >> 6; }
			*gp2 = gt; gp2 += 4;
			gk --;
		}
	}
}

unsigned char *screens [] = {
	vec_001_P1, vec_001_P2, vec_001_P3, vec_001_P4,
	vec_002_P1, vec_002_P2, vec_002_P3, vec_002_P4, vec_002_P5
};

/* ------------------------------------------------------------------ */
/* Sprites                                                             */
/* ------------------------------------------------------------------ */

void spr_put (unsigned char n, unsigned char frame, unsigned char x, unsigned char y) {
	gp = sprite_frames + frame * SPR_FRAME_SIZE;
	sp_MoveSprAbs (spr [n], &clip_full, gp - spr_cur [n], y >> 3, x >> 3, x & 7, y & 7);
	spr_cur [n] = gp;
	spr_on [n] = 1;
}

void spr_hide (unsigned char n) {
	if (spr_on [n]) {
		sp_MoveSprAbs (spr [n], &clip_full, 0, 0, 34, 0, 0);
		spr_on [n] = 0;
	}
}

void spr_hide_all (void) {
	spr_hide (0); spr_hide (1); spr_hide (2);
}

extern unsigned int null_ptr;
#asm
	LIB SPNullSprPtr
._null_ptr
	defw SPNullSprPtr
#endasm

void spr_create (unsigned char n, unsigned char plane) {
	spr [n] = sp_CreateSpr (sp_MASK_SPRITE, 3, sprite_frames, plane);
	sp_AddColSpr (spr [n], sprite_frames + 48);
	sp_AddColSpr (spr [n], sprite_frames + 48);
	/* third column: empty, it only receives the pixels rotated out of the
	   second one. Point it at SPNullSprPtr, which SPMoveSprAbs never
	   animates, so frames need no blank third column (saves 48 bytes/frame) */
	gp = (unsigned char *) spr [n];
	gw = (gp [6] << 8) | gp [7];                  /* first char struct (BE) */
	for (gi = 0; gi < 9; gi ++) {
		gp = (unsigned char *) gw;
		if (gi >= 6) { gp [7] = null_ptr & 255; gp [8] = null_ptr >> 8; }
		gw = (gp [0] << 8) | gp [1];              /* next in sprite (BE)    */
	}
	spr_cur [n] = sprite_frames;
	spr_on [n] = 0;
}

/* ------------------------------------------------------------------ */
/* Game state                                                          */
/* ------------------------------------------------------------------ */

unsigned char px, py;          /* player, pixels             */
unsigned char pl_dir;          /* 0 down 1 up 2 right 3 left */
unsigned char pl_anim;
unsigned char npant;
unsigned char obonabilobos [4];
unsigned char pl_obs;
unsigned char flag_caves;      /* flags (1) in the original  */
unsigned char ob_dirty;

unsigned char step_frame [] = {0, 1, 2, 1};

unsigned char player_frame (void) {
	return SPR_PLAYER + pl_dir * 3 + step_frame [(pl_anim >> 2) & 3];
}

/* attribute fade to white (fadeWhite), on real attrs */
void fade_white (void) {
	#asm
		halt
		ld  hl, 22528
		ld  bc, 768
	.fw_loop
		ld  a, (hl)
		ld  d, a
		and 7
		cp  7
		jr  z, fw_ink
		inc a
	.fw_ink
		ld  e, a
		ld  a, d
		and 0xf8
		or  e
		ld  (hl), a
		inc hl
		dec bc
		ld  a, b
		or  c
		jr  nz, fw_loop
	#endasm
}

void clear_screen (void) {
	for (gj = 0; gj < 8; gj ++) { fade_white (); wait_frames (1); }
	spr_hide_all ();
	sp_UpdateNow ();
	cls_screen (56);
	bg_capture ();
	sp_UpdateNow ();
}

void display_screen (unsigned char n) {
	cls_screen (56);
	if (n == 99) vec_parse (vec_000_P0, 1); else vec_parse (screens [n], 1);
	bg_capture ();
}

void display_extra (unsigned char n) {
	if (n == 2) put_char4 (24, 10, 57, 23);
	ob_dirty = 1;
}

/* The Suvleir message box over the middle of the screen */
void scroll_msg (unsigned char *t1, unsigned char *t2, unsigned char *t3) {
	spr_hide_all ();
	sp_UpdateNow ();
	bg_show ();
	pbase = 0x40;
	clear_box (4, 9, 24, 5, 0);
	for (gy = 9; gy < 14; gy ++) for (gx = 4; gx < 28; gx ++) set_attr (gx, gy, 56);
	vec_parse (vec_scrolly, 0);
	print_centered (10, t1, 1);
	print_centered (11, t2, 1);
	print_centered (12, t3, 1);
	pbase = SHADOW_HI;
	bg_capture ();
	sp_UpdateNow ();
}

/* ------------------------------------------------------------------ */
/* Intro / cutscenes                                                   */
/* ------------------------------------------------------------------ */

/* scroll the top third (sky) 8 pixels left, on the real screen */
void scroll_sky (void) {
	#asm
		ld  hl, 16384
		ld  c, 64                ; 64 pixel lines in the top third
	.ss_loop
		push bc
		ld  a, (hl)
		ld  d, h
		ld  e, l
		inc hl
		ld  bc, 31
		ldir
		ld  (de), a
		pop bc
		dec c
		jr  nz, ss_loop
	#endasm
}

void sky_depack (void) {
	#asm
		ld  hl, _sky_apl
		ld  de, 16384
		di
		push ix
		push iy
		call ap_depack
		pop iy
		pop ix
		ei
		jp  ap_end

	; aPPack decompressor - dwedit / utopian / Metalbrain
	; (ixl/ixh/iyl/iyh written as prefixed opcodes for z80asm)
	.ap_depack
		defb $dd, $2e, 128          ; ld ixl,128
	.apbranch1
		ldi
	.aploop0
		defb $dd, $26, 1            ; ld ixh,1
	.aploop
		call ap_getbit
		jr  nc, apbranch1
		call ap_getbit
		jr  nc, apbranch2
		ld  b, 0
		call ap_getbit
		jr  nc, apbranch3
		ld  c, 16
	.apget4bits
		call ap_getbit
		rl  c
		jr  nc, apget4bits
		jr  nz, apbranch4
		ld  a, b
	.apwritebyte
		ld  (de), a
		inc de
		jr  aploop0
	.apbranch4
		and a
		ex  de, hl
		sbc hl, bc
		ld  a, (hl)
		add hl, bc
		ex  de, hl
		jr  apwritebyte
	.apbranch3
		ld  c, (hl)
		inc hl
		rr  c
		ret z
		ld  a, 2
		adc a, b
		push hl
		defb $fd, $60               ; ld iyh,b
		defb $fd, $69               ; ld iyl,c
		ld  h, d
		ld  l, e
		sbc hl, bc
		ld  c, a
		jr  ap_finishup2
	.apbranch2
		call ap_getgamma
		dec c
		ld  a, c
		defb $dd, $94               ; sub ixh
		jr  z, ap_r0_gamma
		dec a
		ld  b, a
		ld  c, (hl)
		inc hl
		defb $fd, $60               ; ld iyh,b
		defb $fd, $69               ; ld iyl,c
		push bc
		call ap_getgamma
		ex  (sp), hl
		push de
		ex  de, hl
		ld  a, 4
		cp  d
		jr  nc, apskip2
		inc bc
		or  a
	.apskip2
		ld  hl, 127
		sbc hl, de
		jr  c, apskip3
		inc bc
		inc bc
	.apskip3
		pop hl
		push hl
		or  a
	.ap_finishup
		sbc hl, de
		pop de
	.ap_finishup2
		ldir
		pop hl
		defb $dd, $60               ; ld ixh,b
		jr  aploop
	.ap_r0_gamma
		call ap_getgamma
		push hl
		push de
		ex  de, hl
		defb $fd, $54               ; ld d,iyh
		defb $fd, $5d               ; ld e,iyl
		jr  ap_finishup
	.ap_getbit
		defb $dd, $7d               ; ld a,ixl
		add a, a
		defb $dd, $6f               ; ld ixl,a
		ret nz
		ld  a, (hl)
		inc hl
		rla
		defb $dd, $6f               ; ld ixl,a
		ret
	.ap_getgamma
		ld  bc, 1
	.ap_getgammaloop
		call ap_getbit
		rl  c
		rl  b
		call ap_getbit
		jr  c, ap_getgammaloop
		ret
	.ap_end
	#endasm
}

void intro_message (unsigned char *t1, unsigned char *t2) {
	clear_box (4, 10, 24, 4, 1);
	print_centered (11, t1, 1);
	print_centered (12, t2, 1);
}

unsigned char st, mstate, ct, ct2, flipflop, done;

void intro (unsigned char n) {
	clear_screen ();

	display_screen (99);
	msg_save ();

	put_char4 (12, 18, 56, 32);
	put_char (15, 19, 58, 27 + (rnd () % 5));

	/* sky: white on white, depack, fade in */
	for (gw = 0; gw < 256; gw ++) {
		gp = sp_CompDListAddr (gw >> 5, gw & 31);
		*gp = 63;
		*((unsigned char *) (22528 + gw)) = 63;
	}
	cp_dst = 16384; cp_len = 2048; fill_mem (0);
	cp_dst = SHADOW; fill_mem (0);
	sp_UpdateNow ();
	sky_depack ();
	for (gj = 0; gj < 8; gj ++) {
		gc = 56 + 7 - gj;
		cp_dst = 22528; cp_len = 256; fill_mem (gc);
		for (gw = 0; gw < 256; gw ++) { gp = sp_CompDListAddr (gw >> 5, gw & 31); *gp = gc; }
		wait_frames (3);
	}

	pl_dir = 3; pl_anim = 0;
	if (n == 0) {
		st = 0; px = 240; py = 144; mstate = 8;
		spr_hide (0);
	} else {
		st = 1; px = 136; py = 144; mstate = 7;
		spr_put (0, player_frame (), px, py);
	}

	ct = ct2 = flipflop = done = 0;
	isrc = 0;
	while (!done) {
		if (k_fire = !(inport (0x7FFE) & 1)) done = 1;

		wait_frame ();
		scroll_sky ();
		sp_UpdateNow ();

		ct ++; if (ct == 4) { ct = 0; ct2 ++; flipflop = 1 - flipflop; }

		put_char (15, 19, 58, 27 + (rnd () % 5));
		put_char (12, 18, 56, 32 + (flipflop << 2));
		put_char (13, 18, 56, 33 + (flipflop << 2));

		/* walking in (pixel by pixel) */
		if (st == 0 && ct2 > 16 && spr_on [0]) {
			px -= 2; pl_anim ++;
			spr_put (0, player_frame (), px, py);
			if (px == 136) { st = 1; ct2 = 0; ct = 0; pl_anim = 0; spr_put (0, player_frame (), px, py); }
		}

		if (ct == 0) {
			if (st == 0 && ct2 == 16 && !spr_on [0]) spr_put (0, player_frame (), px, py);
			if (st > 0 && st < mstate) {
				if (ct2 == 4) {
					if (n == 0) {
						if (st == 1) intro_message ("HELLO, ELDERLY WELLA.", "I'M TERRIBLY TROUBLED");
						if (st == 2) intro_message ("I KEEP DREAMING THE SAME", "NONSENSE EVERY NIGHT");
						if (st == 3) intro_message ("I FIND MYSELF IN STRANGE", "PLACES UNKNOWN TO ME");
						if (st == 4) intro_message ("I NEED SOME ANSWERS.", "IT'S DRIVING ME MAD.");
						if (st == 5 || st == 7) ct2 = 24;
						if (st == 6) intro_message ("WELLA: LOOK INSIDE YOUR", "MIND TO FIND OUT...");
					} else {
						if (st == 1) intro_message ("THEN EVERYTHING SUDDENLY", "DISAPPEARED...");
						if (st == 2) intro_message ("I FOUND MYSELF IN A", "DIFFERENT PLACE.");
						if (st == 3) intro_message ("BUT I FELT A STRANGE", "POWER IN MY HANDS!");
						if (st == 4 || st == 6) ct2 = 24;
						if (st == 5) intro_message ("I KNEW I HAD TO USE IT.", "IT WAS VERY STRANGE.");
					}
				}
				if (ct2 == 32) { st ++; ct2 = 0; msg_restore (); }
			}
			if (st == mstate) done = 1;
		}
	}
	while (any_key ());
	clear_screen ();
}

/* ------------------------------------------------------------------ */
/* Ice caves (mirror platform puzzle)                                  */
/* ------------------------------------------------------------------ */

/* 16 x 12 tiles of 16x16 pixels; lives in the free gap between the IM2
   table and the ISR ($F101-$F1F0) */
unsigned char *ice_map = 0xF101;
unsigned char *ice_levels [] = { ice_level_0, ice_level_1, ice_level_2, ice_level_3 };

void ice_draw_tile (unsigned char x, unsigned char y, unsigned char attr, unsigned char n) {
	unsigned char *tp;
	tp = ice_tiles + (n << 2);
	x <<= 1; y <<= 1;
	put_char (x, y, attr, tp [0]);
	put_char (x + 1, y, attr, tp [1]);
	put_char (x, y + 1, attr, tp [2]);
	put_char (x + 1, y + 1, attr, tp [3]);
}

void ice_level_draw (unsigned char n) {
	unsigned char x, y, t, c;
	unsigned char *lp;
	lp = ice_levels [n];
	x = y = c = 0;
	while (y < 12) {
		if (c == 0) {
			t = *lp >> 5; c = *lp & 31; if (c == 0) c = 32;
			lp ++;
		} else {
			c --;
			if (y < 6) ice_draw_tile (x, y, 56, t); else ice_draw_tile (x, y, 41, t + 5);
			ice_map [x + (y << 4)] = t;
			x ++; if (x == 16) { x = 0; y ++; }
		}
	}
}

unsigned char ice_tile (unsigned char px, unsigned char py) {
	return ice_map [(px >> 4) + ((py >> 4) << 4)];
}

/* is a 16x16 box at (x,y) free of solid ice tiles (>= 2)? */
unsigned char ice_free (unsigned char x, unsigned char y) {
	if (ice_tile (x, y) > 1) return 0;
	if (ice_tile (x + 15, y) > 1) return 0;
	if (ice_tile (x, y + 15) > 1) return 0;
	if (ice_tile (x + 15, y + 15) > 1) return 0;
	return 1;
}

unsigned char ice_kills (unsigned char x, unsigned char y) {
	if (ice_tile (x + 3, y + 3) == 1) return 1;
	if (ice_tile (x + 12, y + 3) == 1) return 1;
	if (ice_tile (x + 3, y + 12) == 1) return 1;
	if (ice_tile (x + 12, y + 12) == 1) return 1;
	return 0;
}

/* two players: [0] top half, gravity down; [1] bottom half, gravity up */
unsigned char ix [2], iy [2], ijump [2], iground [2];
unsigned char i_facing, i_anim;

void ice_player_move (unsigned char p) {
	unsigned char ymin, ymax;
	if (p == 0) { ymin = 0; ymax = 80; } else { ymin = 96; ymax = 176; }

	/* horizontal */
	if (k_left && ix [p] > 0 && ice_free (ix [p] - 2, iy [p])) ix [p] -= 2;
	if (k_right && ix [p] < 240 && ice_free (ix [p] + 2, iy [p])) ix [p] += 2;

	/* vertical: "down" is +4 for the top guy and -4 for the bottom guy */
	if (p == 0) iground [p] = (iy [p] == ymax || !ice_free (ix [p], iy [p] + 4));
	else iground [p] = (iy [p] == ymin || !ice_free (ix [p], iy [p] - 4));

	if (ijump [p]) {
		if (p == 0) { if (iy [p] > ymin && ice_free (ix [p], iy [p] - 4)) iy [p] -= 4; else ijump [p] = 1; }
		else { if (iy [p] < ymax && ice_free (ix [p], iy [p] + 4)) iy [p] += 4; else ijump [p] = 1; }
		ijump [p] --;
	} else if (!iground [p]) {
		if (p == 0) iy [p] += 4; else iy [p] -= 4;
	} else if (k_fire) {
		ijump [p] = 12;     /* 12 x 4 = 48 pixels, 3 tiles, as the original */
	}
}

unsigned char ice_on_exit (unsigned char p) {
	if (!iground [p]) return 0;
	if (p == 0) return (ice_tile (ix [p] + 8, iy [p] + 16) == 4);
	return (ice_tile (ix [p] + 8, iy [p] - 1) == 4);
}

void ice_game (unsigned char which) {
	unsigned char repeat, finished;
	spr_hide (0);
	repeat = 1;
	while (repeat == 1) {
		ice_level_draw (which);
		ix [0] = ix [1] = 0; iy [0] = 0; iy [1] = 176;
		ijump [0] = ijump [1] = 0; i_facing = 0; i_anim = 0;
		finished = 0;
		while (!finished) {
			read_keys ();
			if (k_left) { i_facing = 2; i_anim ++; }
			if (k_right) { i_facing = 0; i_anim ++; }
			ice_player_move (0);
			ice_player_move (1);
			gc = i_facing + ((i_anim >> 2) & 1);
			spr_put (1, SPR_ICE_TOP + gc, ix [0], iy [0]);
			spr_put (2, SPR_ICE_BOT + gc, ix [1], iy [1]);
			wait_frame ();
			sp_UpdateNow ();

			if (ice_on_exit (0) && ice_on_exit (1)) {
				if (obonabilobos [which] == 0) {
					scroll_msg ("CONGRATULATIONS!", " ", "YOU GOT AN OBONABILOBO");
					obonabilobos [which] = 1;
					pl_obs ++;
				} else {
					scroll_msg ("CONGRATULATIONS!", "BUT THE OBONABILOBO HERE", "WAS ALREADY TAKEN...");
				}
				wait_key ();
				finished = 1; repeat = 2;
			} else if (ice_kills (ix [0], iy [0]) || ice_kills (ix [1], iy [1])) {
				scroll_msg ("OUCH!", " ", "TRY AGAIN Y/N?");
				repeat = 0;
				while (repeat == 0) {
					if (!(inport (0xDFFE) & 0x10)) repeat = 1;   /* Y */
					if (!(inport (0x7FFE) & 0x08)) repeat = 2;   /* N */
				}
				finished = 1;
			}
		}
	}
	spr_hide (1); spr_hide (2);
	while (any_key ());
}

/* ------------------------------------------------------------------ */
/* Overworld                                                           */
/* ------------------------------------------------------------------ */

/* is the 16x16 box at (x,y) on walkable cells? (hardness 1 = wall) */
unsigned char pl_free (unsigned char x, unsigned char y) {
	unsigned char c0, c1, r0, r1, r, c;
	c0 = x >> 3; c1 = (x + 15) >> 3; r0 = y >> 3; r1 = (y + 15) >> 3;
	for (r = r0; r <= r1; r ++) {
		gp = dlist + 1 + (r << 7) + (c0 << 2);
		for (c = c0; c <= c1; c ++) { if (*gp == 1) return 0; gp += 4; }
	}
	return 1;
}

void player_move (void) {
	unsigned char moved, nx2, ny2, t;
	read_keys ();
	moved = 0;

	if (k_left || k_right) {
		if (k_left) { pl_dir = 3; nx2 = px - 2; } else { pl_dir = 2; nx2 = px + 2; }
		moved = 1;
		if ((k_left && px > 0) || (k_right && px < 240)) {
			if (pl_free (nx2, py)) px = nx2;
			else if (py & 7) {
				/* corner sliding: nudge towards the nearest row */
				t = ((py & 7) < 4) ? (py & 0xF8) : ((py | 7) + 1);
				if (pl_free (nx2, t)) py += (t < py) ? -2 : 2;
			}
		}
	}
	if (k_up || k_down) {
		if (k_up) { pl_dir = 1; ny2 = py - 2; } else { pl_dir = 0; ny2 = py + 2; }
		moved = 1;
		if ((k_up && py > 0) || (k_down && py < 176)) {
			if (pl_free (px, ny2)) py = ny2;
			else if (px & 7) {
				t = ((px & 7) < 4) ? (px & 0xF8) : ((px | 7) + 1);
				if (pl_free (t, ny2)) px += (t < px) ? -2 : 2;
			}
		}
	}
	if (moved) pl_anim ++;
	spr_put (0, player_frame (), px, py);
}

unsigned char num_buf [4];

void show_ob (void) {
	if (flag_caves == 1) {
		num_buf [0] = '0' + pl_obs; num_buf [1] = ' '; num_buf [2] = 0;
		print_at (28, 0, "OB:", 1);
		print_at (31, 0, num_buf, 1);
	}
	ob_dirty = 0;
}

void init_world (unsigned char n) {
	npant = n;
	px = 32; py = 32;
	spr_hide (0);
	display_screen (npant);
	display_extra (npant);
	spr_put (0, player_frame (), px, py);
}

void game (void) {
	unsigned char i, j, cx8, cy8, k, newscr;
	unsigned char *c, *h;

	npant = 0;
	for (i = 0; i < 4; i ++) obonabilobos [i] = 0;
	flag_caves = 99;
	pl_obs = 0;

	intro (0);

	pl_dir = 0; pl_anim = 0;
	init_world (0);

	while (1) {
		player_move ();
		wait_frame ();
		sp_UpdateNow ();

		if (ob_dirty) show_ob ();

		cx8 = (px + 4) >> 3; cy8 = (py + 4) >> 3;
		c = connections + npant * 28;
		h = hotspots + npant * 24;
		for (i = 0; i < 4; i ++) {
			/* connections: X1, Y1, X2, Y2, screen, X, Y */
			if ((c [2] | c [3]) && cx8 >= c [0] && cx8 <= c [2] && cy8 >= c [1] && cy8 <= c [3]) {
				newscr = (npant != c [4]);
				npant = c [4];
				if (c [5] != 255) px = c [5] << 3;
				if (c [6] != 255) py = c [6] << 3;
				if (newscr) {
					spr_hide (0);
					display_screen (npant);
					display_extra (npant);
					spr_put (0, player_frame (), px, py);
				}
				break;
			}
			c += 7;

			/* hotspots: X1, Y1, X2, Y2, type, aux */
			if (h [4] && cx8 >= h [0] && cx8 <= h [2] && cy8 >= h [1] && cy8 <= h [3]) {
				if (h [4] == 1) {
					if (flag_caves == 1) {
						ice_game (h [5]);
						display_screen (npant);
						display_extra (npant);
					} else {
						msg_save ();
						scroll_msg ("SADLY...", " ", "THE DOOR IS CLOSED.");
						wait_key ();
						msg_restore ();
					}
					py += 8;
				} else if (h [4] == 2) {
					k = 0;
					msg_save ();
					if (flag_caves != 1) {
						flag_caves = 1;
						scroll_msg ("LE VIELHO SAYS", "BRING ME 4 OBONABILOBOS", "I OPENED THE ICE CAVES");
					} else if (pl_obs < 4) {
						scroll_msg ("LE VIELHO SAYS", "'I ALREADY TOLD YOU", "WHAT TO DO'");
					} else {
						k = 1;
						scroll_msg ("LE VIELHO SAYS", "'OH! MY OBONABILOBOS", "THANK YOU'");
						wait_key ();
						scroll_msg ("LE VIELHO SAYS", "'I HAVE A PRESENT:", "THE KILLING SPELL!'");
						wait_key ();
						scroll_msg ("LE VIELHO SAYS", "'USE IT WISELY...", "PREPARE TO TRANSPORT!'");
						wait_key ();
						flag_caves = 0;
						intro (1);
						init_world (4);
					}
					if (k == 0) {
						wait_key ();
						msg_restore ();
						px = 168;
					}
					ob_dirty = 1;
				}
				spr_put (0, player_frame (), px, py);
				break;
			}
			h += 6;
		}
	}
}

/* ------------------------------------------------------------------ */

main () {
	#asm
		di
	#endasm
	/* block memory first: the ISR hook list also lives in u_malloc'ed blocks */
	sp_AddMemory (0, NUMBLOCKS, 14, AD_FREE);
	sp_InitIM2 (0xf1f1);
	sp_CreateGenericISR (0xf1f1);
	sp_RegisterHook (255, isr);
	#asm
		ei
	#endasm

	sp_Initialize (56, 0);
	dlist = sp_CompDListAddr (0, 0);
	sp_Border (BLACK);

	spr_create (0, 1);
	spr_create (1, 1);
	spr_create (2, 1);

	cls_screen (56);
	bg_capture ();
	sp_UpdateNow ();

	while (1) game ();
}
