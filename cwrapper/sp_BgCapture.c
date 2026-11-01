/*
 *      Sprite Pack V2.2 - BITMAPBG extension
 *      See bitmapbg/SPBgCapture.asm
 */

#define _SPLIB
#include "spritepack.h"

void sp_BgCapture(void)
{
#asm
   LIB SPBgCapture

   call SPBgCapture
#endasm
}
