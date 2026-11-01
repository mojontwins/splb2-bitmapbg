/*
 *      Sprite Pack V2.2 - BITMAPBG extension
 *      See bitmapbg/SPZX0Depack.asm
 */

#define _SPLIB
#include "spritepack.h"

void sp_ZX0(void *src, void *dst)
{
#asm
   LIB SPZX0Depack

   pop bc
   pop de          ; de = dst
   pop hl          ; hl = src
   push hl
   push de
   push bc
   jp SPZX0Depack
#endasm
}
