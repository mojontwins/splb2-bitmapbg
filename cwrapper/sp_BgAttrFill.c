/*
 *      Sprite Pack V2.2 - BITMAPBG extension
 *      See bitmapbg/SPBgAttrFill.asm
 */

#define _SPLIB
#include "spritepack.h"

void sp_BgAttrFill(uchar attr)
{
#asm
   LIB SPBgAttrFill

   ld hl,2
   add hl,sp
   ld a,(hl)
   jp SPBgAttrFill
#endasm
}
