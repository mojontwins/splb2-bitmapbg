/*
 *      Sprite Pack V2.2 - Suvleir 4 vector screens
 *      See vector/SPVecDraw.asm
 */

#define _SPLIB
#include "spritepack.h"

void sp_VecDraw(void *svl, uchar *hardness, uchar stride)
{
#asm
   LIB SPVecDraw

   ld hl,2
   add hl,sp
   ld a,(hl)       ; stride
   inc hl
   inc hl
   ld e,(hl)
   inc hl
   ld d,(hl)       ; de = hardness destination
   inc hl
   ld c,(hl)
   inc hl
   ld b,(hl)       ; bc = svl
   ld l,c
   ld h,b
   jp SPVecDraw
#endasm
}
