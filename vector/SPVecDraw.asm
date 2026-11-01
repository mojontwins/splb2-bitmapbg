;
; SPVecDraw
; Suvleir 4 vector screens (SVL) for Sprite Pack 2.2
; The Mojon Twins
;
; Draws an SVL screen (lines, plots, patterned fills) on the real display
; ($4000) and optionally decodes its hardness map. Pixel for pixel the same
; result as the Suvleir 4 editor shows.
;
; SVL format (see tools/suvleir4/docs/FORMAT.md):
;   +0  2 bytes  little-endian offset of the hardness section (0 = none)
;   +2  commands:
;         y x        (y < 192)  line from the pen to (x,y); start pixel not drawn
;         $C0 x y               plot (x,y), pen = (x,y)
;         $C1 x y p             patterned flood fill at (x,y) with pattern p
;         $C2 x y               pen = (x,y), nothing drawn
;         $FF                   end of commands
;   hardness: RLE bytes (value << 6) | count, count 1..63, 768 cells row major
;
; enter: HL = SVL data
;        DE = where to write the hardness map (0 = don't decode)
;        A  = stride between cells in the destination (1 = plain 768-byte
;             array, 4 = byte 1 of each display list entry, ...)
; uses : af, bc, de, hl, ix (preserved), iy untouched
;
; Configuration (set before calling, or use the defaults):
;   SPVecPatterns  (2 bytes)  pointer to the fill pattern table, 8 bytes per
;                             pattern. Default: SPVecDefPatterns (the 8
;                             Suvleir 3 / Lhikoj patterns).
;   SPVecFillSP    (2 bytes)  if not 0, SP is switched to this address while
;                             SPPFill runs (its queue lives on the stack and
;                             needs 3*depth+30 bytes). With BITMAPBG the top
;                             of the shadow screen is a good place: it gets
;                             overwritten by SPBgCapture anyway.
;   SPVecFillDepth (2 bytes)  SPPFill queue depth. Default 300 (930 bytes).

INCLUDE "SPconfig.def"

XLIB SPVecDraw
XDEF SPVecPatterns, SPVecFillSP, SPVecFillDepth, SPVecDefPatterns
LIB SPGetScrnAddr, SPPFill

.SPVecPatterns   defw SPVecDefPatterns
.SPVecFillSP     defw 0
.SPVecFillDepth  defw 300

.SPVecDefPatterns
   defb $FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF
   defb $77,$FF,$FF,$FF,$77,$FF,$FF,$FF
   defb $77,$FF,$DD,$FF,$77,$FF,$DD,$FF
   defb $55,$FF,$55,$FF,$55,$FF,$55,$FF
   defb $AA,$55,$AA,$55,$AA,$55,$AA,$55
   defb $FF,$10,$10,$10,$FF,$01,$01,$01
   defb $88,$00,$22,$00,$88,$00,$22,$00
   defb $88,$00,$00,$00,$88,$00,$00,$00

.vd_base    defw 0
.vd_hard    defw 0
.vd_stride  defb 0
.vd_cx      defb 0
.vd_cy      defb 0
.vd_nx      defb 0
.vd_ny      defb 0
.vd_dx      defb 0
.vd_dy      defb 0
.vd_xi      defb 0
.vd_yi      defb 0
.vd_sp      defw 0

.SPVecDraw
   ld (vd_stride),a
   ld (vd_hard),de
   ld (vd_base),hl
   xor a
   ld (vd_cx),a
   ld (vd_cy),a                  ; pen starts at 0,0
   inc hl
   inc hl

.cmdloop
   ld a,(hl)
   inc hl
   cp 192
   jr c, cmd_line
   jr z, cmd_plot
   cp $C1
   jr z, cmd_fill
   cp $C2
   jr z, cmd_move
   jp hardness                   ; $FF (or anything unknown): end of commands

.cmd_line
   ld (vd_ny),a
   ld a,(hl)
   inc hl
   ld (vd_nx),a
   push hl
   call vd_draw
   pop hl
   jr cmdloop

.cmd_plot
   call cmd_pen
   push hl
   call vd_plot
   pop hl
   jr cmdloop

.cmd_move
   call cmd_pen
   jr cmdloop

.cmd_pen
   ld a,(hl)
   inc hl
   ld (vd_cx),a
   ld a,(hl)
   inc hl
   ld (vd_cy),a
   ret

.cmd_fill
   ld e,(hl)                     ; x
   inc hl
   ld d,(hl)                     ; y
   inc hl
   ld a,(hl)                     ; pattern
   inc hl
   push hl
   ld l,a
   ld h,0
   add hl,hl
   add hl,hl
   add hl,hl
   ld bc,(SPVecPatterns)
   add hl,bc                     ; hl = pattern
   ld a,d
   cp 192
   jr nc, fill_skip              ; off screen: SPPFill would scribble on RAM
   ex de,hl                      ; de = pattern, h = y, l = x
   ld bc,(SPVecFillDepth)
   push ix
   ld a,(SPVecFillSP)
   push hl
   ld hl,SPVecFillSP+1
   or (hl)
   pop hl
   jr z, fill_here
   ld (vd_sp),sp
   ld sp,(SPVecFillSP)           ; run the fill on the borrowed stack
   call SPPFill
   ld sp,(vd_sp)
   jr fill_done
.fill_here
   call SPPFill
.fill_done
   pop ix
.fill_skip
   pop hl
   jp cmdloop

; ---- hardness section

.hardness
   ld hl,(vd_hard)
   ld a,h
   or l
   ret z                         ; caller doesn't want it
   ld hl,(vd_base)
   ld e,(hl)
   inc hl
   ld d,(hl)
   ld a,d
   or e
   ret z                         ; file has no hardness section
   dec hl
   add hl,de                     ; hl = RLE data
   ld de,(vd_hard)
   ld bc,768

.hloop
   ld a,(hl)
   inc hl
   push hl
   ld h,a
   and 63
   ld l,a                        ; l = run length (0 counts as 256)
   ld a,h
   rlca
   rlca
   and 3
   ld h,a                        ; h = value

.hrun
   ld a,h
   ld (de),a
   ld a,(vd_stride)
   add a,e
   ld e,a
   jr nc, hnc
   inc d
.hnc
   dec bc
   ld a,b
   or c
   jr z, hdone
   dec l
   jr nz, hrun
   pop hl
   jr hloop

.hdone
   pop hl
   ret

; ---- plot (vd_cx, vd_cy) on the real screen

.vd_plot
   ld a,(vd_cy)
   cp 192
   ret nc
   ld h,a
   ld a,(vd_cx)
   ld l,a
   ld a,h
   call SPGetScrnAddr            ; de = screen address, b = pixel mask
   ld a,(de)
   or b
   ld (de),a
   ret

; ---- line from (vd_cx, vd_cy) to (vd_nx, vd_ny)
; error starts at major/2, minor axis steps when it goes negative,
; the start pixel is not plotted (same decisions as ZX Basic's DRAW)

.vd_draw
   ld a,(vd_nx)
   ld hl,vd_cx
   sub (hl)
   ld c,1
   jr nc, vd1
   neg
   ld c,255
.vd1
   ld (vd_dx),a
   ld a,c
   ld (vd_xi),a

   ld a,(vd_ny)
   ld hl,vd_cy
   sub (hl)
   ld c,1
   jr nc, vd2
   neg
   ld c,255
.vd2
   ld (vd_dy),a
   ld a,c
   ld (vd_yi),a

   ld a,(vd_dx)
   ld b,a
   ld a,(vd_dy)
   cp b
   jr c, vd_xmajor               ; dy < dx

   ld a,(vd_dy)                  ; dy >= dx
   srl a
   ld l,a
   ld h,0                        ; hl = error = dy/2
.vdy_loop
   ld a,(vd_cy)
   ld b,a
   ld a,(vd_ny)
   cp b
   ret z
   ld a,(vd_dx)
   ld e,a
   ld d,0
   or a
   sbc hl,de                     ; error -= dx
   bit 7,h
   jr z, vdy_nox
   ld a,(vd_dy)
   ld e,a
   add hl,de                     ; error += dy
   ld a,(vd_xi)
   ld b,a
   ld a,(vd_cx)
   add a,b
   ld (vd_cx),a
.vdy_nox
   ld a,(vd_yi)
   ld b,a
   ld a,(vd_cy)
   add a,b
   ld (vd_cy),a
   push hl
   call vd_plot
   pop hl
   jr vdy_loop

.vd_xmajor
   ld a,(vd_dx)
   srl a
   ld l,a
   ld h,0                        ; hl = error = dx/2
.vdx_loop
   ld a,(vd_cx)
   ld b,a
   ld a,(vd_nx)
   cp b
   ret z
   ld a,(vd_dy)
   ld e,a
   ld d,0
   or a
   sbc hl,de                     ; error -= dy
   bit 7,h
   jr z, vdx_noy
   ld a,(vd_dx)
   ld e,a
   add hl,de                     ; error += dx
   ld a,(vd_yi)
   ld b,a
   ld a,(vd_cy)
   add a,b
   ld (vd_cy),a
.vdx_noy
   ld a,(vd_xi)
   ld b,a
   ld a,(vd_cx)
   add a,b
   ld (vd_cx),a
   push hl
   call vd_plot
   pop hl
   jr vdx_loop
