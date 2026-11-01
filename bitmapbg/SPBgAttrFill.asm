;
; SPBgAttrFill
; BITMAPBG extension for Sprite Pack 2.2
;
; Sets the attribute of every managed cell (display list byte 0) and marks
; everything dirty. Use it after painting a whole background straight into
; the shadow screen (for example depacking thirds into SPBgThird(n)).
;
; enter: A = attribute
; uses : af, bc, de, hl

INCLUDE "SPconfig.def"

XLIB SPBgAttrFill
LIB SPDisplayList, SPDirtyChars

.SPBgAttrFill
   ld hl,SPDisplayList
   ld de,4
   ld bc,32*(SP_ROWEND-SP_ROWSTART)
.afloop
   ld (hl),a
   add hl,de
   dec bc
   ex af,af'
   ld a,b
   or c
   jr z, afdone
   ex af,af'
   jr afloop
.afdone
   ld hl,SPDirtyChars
   ld de,SPDirtyChars+1
   ld (hl),$ff
   ld bc,4*(SP_ROWEND-SP_ROWSTART)-1
   ldir
   ret
