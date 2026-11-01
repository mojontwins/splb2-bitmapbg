;
; SPBgCapture
; BITMAPBG extension for Sprite Pack 2.2
;
; Copies the current Spectrum display (bitmap + attributes) into the
; background: the bitmap goes to the shadow screen at BGSHADOW*256, the
; attributes go to byte 0 of every display list cell. Then marks every
; managed cell dirty so the next SPUpdateNow redraws the whole screen
; (background + sprites on top).
;
; Typical use: draw a background directly on the screen with any routine
; you like (ROM, lines, fills, decompressors...) and then call this.
; The screen must not contain sprite pixels at that moment (see SPBgShow).
;
; enter: -
; exit : -
; uses : af, bc, de, hl

INCLUDE "SPconfig.def"

XLIB SPBgCapture
LIB SPDisplayList, SPDirtyChars

.SPBgCapture
IF BITMAPBG
   ld hl,$4000
   ld de,BGSHADOW*256
   ld bc,6144
   ldir                          ; bitmap -> shadow screen

   ld hl,$5800 + 32*SP_ROWSTART
   ld de,SPDisplayList
   ld bc,32*(SP_ROWEND-SP_ROWSTART)

.attrloop
   ldi                           ; attribute -> display list byte 0
   inc de
   inc de
   inc de                        ; next 4-byte cell
   jp pe, attrloop               ; P/V set while bc != 0

   ld hl,SPDirtyChars
   ld de,SPDirtyChars+1
   ld (hl),$ff
   ld bc,4*(SP_ROWEND-SP_ROWSTART)-1
   ldir                          ; invalidate everything
   ret
ELSE
   ret                           ; library built without BITMAPBG: no-op
ENDIF
