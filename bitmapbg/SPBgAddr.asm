;
; SPBgAddr
; BITMAPBG extension for Sprite Pack 2.2
;
; Address in the shadow screen of the first (top) pixel line of a
; character cell. The shadow has the Spectrum display file layout, so the
; following 7 lines of the cell are at +$100, +$200 ... +$700 (inc h).
;
; After writing to the shadow, invalidate the cell(s) (SPInvalidate or
; sp_Invalidate) so SPUpdateNow redraws them. To change a cell's colour,
; write byte 0 of its display list entry (SPCompDListAddr).
;
; enter: A = row 0..23, C = col 0..31
; exit : HL = shadow address
; uses : af, hl

INCLUDE "SPconfig.def"

XLIB SPBgAddr

.SPBgAddr
IF BITMAPBG
   ld l,a
   and $18                       ; screen third
   add a,BGSHADOW
   ld h,a
   ld a,l
   and $07
   rrca
   rrca
   rrca                          ; (row & 7) * 32
   or c
   ld l,a
   ret
ELSE
   ret                           ; library built without BITMAPBG: no-op
ENDIF
