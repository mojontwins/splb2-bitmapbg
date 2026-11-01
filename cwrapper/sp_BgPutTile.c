/*
 *      Sprite Pack V2.2 - BITMAPBG extension
 *      See bitmapbg/SPBgPutTile.asm
 */

#define _SPLIB
#include "spritepack.h"

void sp_BgPutTile(uchar row, uchar col, uchar attr, void *gfx)
{
#asm
   LIB SPBgPutTile

   ld hl,2
   add hl,sp
   ld e,(hl)
   inc hl
   ld d,(hl)       ; de = graphic
   inc hl
   ld b,(hl)       ; b = attr
   inc hl
   inc hl
   ld c,(hl)       ; c = col
   inc hl
   inc hl
   ld a,(hl)       ; a = row
   jp SPBgPutTile
#endasm
}
