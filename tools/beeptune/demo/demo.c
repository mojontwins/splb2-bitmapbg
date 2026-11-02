/*
 * Beeptune player demo: the three ways of playing a song with beeptune.h.
 * z88dk classic, sccz80:  see build.sh
 *
 *   1  bt_play: the song once (a key stops it)
 *   2  bt_loop: the song forever (a key stops it)
 *   3  bt_step: in-game. A square bounces round the screen at 50 fps while
 *      the music plays; O / P change bt_steplen (how much of each frame
 *      goes to the music), SPACE goes back to the menu.
 */
#include <stdio.h>
#include "../player/beeptune.h"
#include "demo_song.h"

unsigned char k, x, y, dx, dy;
unsigned char *a;

unsigned char __FASTCALL__ inport (unsigned int port) {
	#asm
		ld  b, h
		ld  c, l
		in  a, (c)
		ld  l, a
		ld  h, 0
	#endasm
}

void halt (void) {
	#asm
		ei
		halt
	#endasm
}

/* a key 1-3 on the menu */
unsigned char menu_key (void) {
	while (1) {
		k = inport (0xF7FE) & 7;              /* 1 2 3 */
		if (!(k & 1)) return 1;
		if (!(k & 2)) return 2;
		if (!(k & 4)) return 3;
	}
}

void menu (void) {
	printf ("%c", 12);
	printf ("BEEPTUNE player demo\n\n");
	printf ("1  bt_play  once, a key stops\n");
	printf ("2  bt_loop  forever, a key stops\n");
	printf ("3  bt_step  in-game: one step\n");
	printf ("            per frame\n");
}

void ingame (void) {
	printf ("%c", 12);
	printf ("bt_step: O P change bt_steplen\nSPACE: back to the menu\n");
	bt_init (demo_song);
	x = 10; y = 10; dx = 1; dy = 1;
	while (inport (0x7FFE) & 1) {
		halt ();
		/* the bouncing square: an attribute moving round the screen */
		a = (unsigned char *) (0x5800 + (y << 5) + x);
		*a = 0x38;
		x += dx; y += dy;
		if (x == 0 || x == 31) dx = -dx;
		if (y == 3 || y == 23) dy = -dy;
		a = (unsigned char *) (0x5800 + (y << 5) + x);
		*a = 0x10;
		/* the music: a slice of this frame */
		bt_step ();
		k = inport (0xDFFE);
		if (!(k & 2) && bt_steplen > 20) bt_steplen -= 4;      /* O */
		if (!(k & 1) && bt_steplen < 500) bt_steplen += 4;     /* P */
		printf ("\x16\x03\x01steplen %u = %u%% of a frame   ", bt_steplen, bt_steplen * 100 / BT_FRAME);
	}
}

main () {
	while (1) {
		menu ();
		k = menu_key ();
		if (k == 1) { bt_play (demo_song); }
		else if (k == 2) { bt_loop (demo_song); }
		else ingame ();
	}
}
