;
; SPBgPutTile
; BITMAPBG extension for Sprite Pack 2.2
;
; Paints an 8x8 tile straight into the background (shadow screen), sets the
; cell's attribute in the display list and marks the cell dirty. Nothing is
; written to the real screen: the next SPUpdateNow shows it, with any
; sprites in that cell composed on top.
;
; enter: A = row 0..23, C = col 0..31, B = attribute, DE = 8 bytes of graphic
; uses : af, bc, de, hl

INCLUDE "SPconfig.def"

XLIB SPBgPutTile
LIB SPBgAddr, SPCompDListAddr, SPCompDirtyAddr

.SPBgPutTile
IF BITMAPBG
   push bc                       ; b = attr, c = col
   push af                       ; a = row
   push de
   call SPBgAddr                 ; hl = shadow address
   pop de
   ex de,hl                      ; hl = graphic, de = shadow
   ld b,8
.ptloop
   ld a,(hl)
   ld (de),a
   inc hl
   inc d
   djnz ptloop

   pop af
   pop bc
   push bc
   push af
   call SPCompDListAddr          ; hl = display list entry
   pop af
   pop bc
   ld (hl),b                     ; attribute

   call SPCompDirtyAddr          ; hl = dirty byte, a = col & 7
   ld b,a
   ld a,1
   inc b
.ptmask
   dec b
   jr z, ptset
   rlca
   jr ptmask
.ptset
   or (hl)
   ld (hl),a                     ; cell is dirty
ENDIF
   ret
