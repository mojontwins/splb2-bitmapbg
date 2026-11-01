/*
 *      Sprite Pack V2.2 - BITMAPBG extension
 *      See bitmapbg/SPBgShow.asm
 */

#define _SPLIB
#include "spritepack.h"

void sp_BgShow(void)
{
#asm
   LIB SPBgShow

   call SPBgShow
#endasm
}
