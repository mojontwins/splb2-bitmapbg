;
; SPBgShow
; BITMAPBG extension for Sprite Pack 2.2
;
; The opposite of SPBgCapture: copies the shadow screen to the display
; and the display list attributes to attribute memory. Afterwards the
; screen shows the pure background with no sprites on it, which is the
; state you want before drawing something new over the background with
; screen-based routines and calling SPBgCapture again.
;
; Sprites are not removed from splib2's lists: they come back on the next
; SPUpdateNow for any cell that is dirty (SPBgCapture makes all of them dirty).
;
; enter: -
; exit : -
; uses : af, bc, de, hl

INCLUDE "SPconfig.def"

XLIB SPBgShow
LIB SPDisplayList

.SPBgShow
IF BITMAPBG
   ld hl,BGSHADOW*256
   ld de,$4000
   ld bc,6144
   ldir                          ; shadow screen -> bitmap

   ld hl,SPDisplayList
   ld de,$5800 + 32*SP_ROWSTART
   ld bc,32*(SP_ROWEND-SP_ROWSTART)

.attrloop
   ld a,(hl)
IF FLASHASFG
   and 127                       ; bit 7 is the "no sprites here" flag
ENDIF
   ld (de),a
   inc de
   inc hl
   inc hl
   inc hl
   inc hl
   dec bc
   ld a,b
   or c
   jp nz, attrloop
   ret
ELSE
   ret                           ; library built without BITMAPBG: no-op
ENDIF
