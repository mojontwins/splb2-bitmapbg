/*
 *      Sprite Pack V2.2 - Suvleir 4 vector screens
 *      See vector/SPVecDraw.asm
 */

#define _SPLIB
#include "spritepack.h"

void sp_VecConfig(void *patterns, void *fillstack, uint filldepth)
{
#asm
   LIB SPVecDraw
   XREF SPVecPatterns, SPVecFillSP, SPVecFillDepth, SPVecDefPatterns

   ld hl,2
   add hl,sp
   ld e,(hl)
   inc hl
   ld d,(hl)
   inc hl
   ld (SPVecFillDepth),de
   ld e,(hl)
   inc hl
   ld d,(hl)
   inc hl
   ld (SPVecFillSP),de
   ld e,(hl)
   inc hl
   ld d,(hl)
   ld a,d
   or e
   jr nz, vc_pat
   ld de,SPVecDefPatterns          ; NULL = built-in patterns
.vc_pat
   ld (SPVecPatterns),de
#endasm
}
