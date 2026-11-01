/*
 *      Sprite Pack V2.2 - BITMAPBG extension
 *      See bitmapbg/SPBgAddr.asm
 */

#define _SPLIB
#include "spritepack.h"

void *sp_BgAddr(uchar row, uchar col)
{
#asm
   LIB SPBgAddr

   ld hl,2
   add hl,sp
   ld c,(hl)
   inc hl
   inc hl
   ld a,(hl)
   call SPBgAddr
#endasm
}
