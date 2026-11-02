/*
 * beeptune.h - a two channel beeper music player for the ZX Spectrum,
 * for z88dk (classic, sccz80). Plays songs made with the Beeptune
 * tracker (../README.md). #include it in ONE C file of your program.
 *
 * Three ways to play:
 *
 *   bt_play (song);   play the song once. A key stops it. Returns 1 if a
 *                     key stopped it, 0 if it reached the end.
 *   bt_loop (song);   play the song forever (from its loop point at the
 *                     end) until a key is pressed. Returns 1.
 *
 *   bt_init (song);   get ready to play in-game, then call
 *   bt_step ();       once per game frame. Each call sounds the current
 *                     notes for bt_steplen "turns" of the player (see
 *                     below) and returns. The song moves on one row every
 *                     `speed` calls (the song's speed, in frames per row),
 *                     so if you call it once per 50 Hz frame the song
 *                     keeps its tempo; at 25 fps it plays at half speed
 *                     (make the song twice as fast, or call it twice).
 *                     In this mode the song always loops.
 *
 *   bt_steplen        how long each bt_step sounds, in turns of 130
 *                     T-states. 538 turns are a whole 50 Hz frame, so:
 *                       BT_FRAME / 8  =  67   very short blips, 1/8 of the CPU
 *                       BT_FRAME / 4  = 134   (the default), 1/4 of the CPU
 *                       BT_FRAME / 2  = 269   half the CPU
 *                     The rest of the frame is the game's. The longer, the
 *                     more the notes sound like notes and less like clicks.
 *   bt_border         border colour (0-7) while playing. Default 0.
 *
 * While a tone sounds, interrupts are off (they would make the pitch
 * wobble). They are turned back on afterwards only if they were on.
 * Interrupts that fall in that time are lost: in bt_step mode a game that
 * counts frames with its ISR sees fewer of them.
 *
 * Registers: uses AF BC DE HL and BC' DE' HL' (EXX); IX and IY are not
 * touched. About 600 bytes of code and tables.
 *
 * ---------------------------------------------------------------------------
 * HOW IT SOUNDS
 * ---------------------------------------------------------------------------
 * Each channel is a square wave made with a 16-bit phase accumulator (an
 * "add, look at the top bit" oscillator). The loop adds channel A's step,
 * puts its level on the speaker, waits, adds channel B's step and puts B's
 * level, and so on: the speaker spends half the time on each channel, so
 * both are heard at once (time-division mixing, as in many beeper engines
 * since the 80s). One turn of the loop is 130 T-states (26.9 kHz). A step
 * is 65536 * f * 130 / 3500000 for a note of f Hz.
 *
 * ---------------------------------------------------------------------------
 * THE SONG FORMAT (all offsets are from the start of the song, so the data
 * can be put anywhere)
 * ---------------------------------------------------------------------------
 *   +0          speed: frames (1/50 s) per row, 1-255
 *   +1          L: length of the order list, 1-255
 *   +2          loop point: order list entry to go back to, 0..L-1
 *   +3          P: number of patterns, 1-255
 *   +4          the order list: L pattern numbers, 0..P-1
 *   +4+L        P words (little endian): offset of each pattern
 *   patterns    rows (1-255), then two bytes per row: channel A, channel B
 *               0 = keep playing what was playing, 1 = silence,
 *               2..85 = notes C-1 .. B-7 (2 + 12 * (octave - 1) + semitone)
 *
 * The tracker exports this as a .bin to incbin, z80asm source or a C array.
 */

#ifndef BEEPTUNE_H
#define BEEPTUNE_H

#define BT_FRAME  538                   /* turns in a 50 Hz frame          */

extern unsigned char bt_border;
extern unsigned int  bt_steplen;

/* ------------------------------------------------------------------------ */

unsigned char __FASTCALL__ bt_play (void *song) {
	#asm
		xor a
		jp  bt_play_go
	#endasm
}

unsigned char __FASTCALL__ bt_loop (void *song) {
	#asm
		ld  a, 1
		jp  bt_play_go
	#endasm
}

void __FASTCALL__ bt_init (void *song) {
	#asm
		ld  a, 1
		ld  (bt_loopf), a
		jp  bt_start
	#endasm
}

void bt_step (void) {
	#asm
		ld  a, (bt_frame)
		or  a
		call z, bt_row                  ; first frame of a row: read it
		ld  bc, (_bt_steplen)
		call bt_tone
		ld  hl, bt_frame
		inc (hl)
		ld  a, (bt_speed)
		cp  (hl)
		ret nz
		ld  (hl), 0                     ; the row is over
	#endasm
}

#asm
._bt_border     defb 0
._bt_steplen    defw 134
.bt_base        defw 0                  ; the song
.bt_speed       defb 1                  ; header
.bt_ordlen      defb 1
.bt_loopto      defb 0
.bt_ord         defb 0                  ; order list entry playing
.bt_ptr         defw 0                  ; next row of its pattern
.bt_left        defb 0                  ; rows left in the pattern
.bt_stepa       defw 0                  ; steps of the notes playing
.bt_stepb       defw 0                  ;   (0 = silence)
.bt_acca        defw 0                  ; phase accumulators
.bt_accb        defw 0
.bt_frame       defb 0                  ; frames into the row
.bt_loopf       defb 0                  ; 1: loop at the end

; bt_play / bt_loop: A = 0 once, 1 forever. HL = song.
.bt_play_go
		ld  (bt_loopf), a
		call bt_start
.bt_play_release                        ; wait for the keys to be released
		xor a
		in  a, ($fe)
		cpl
		and $1f
		jr  nz, bt_play_release
.bt_play_row
		call bt_row
		jr  c, bt_play_end              ; the end (when not looping)
		ld  a, (bt_speed)
		ld  (bt_frame), a
.bt_play_frame                          ; a frame of sound...
		ld  bc, 538
		call bt_tone
		xor a                           ; ...then a look at the keyboard
		in  a, ($fe)
		cpl
		and $1f
		jr  nz, bt_play_key
		ld  hl, bt_frame
		dec (hl)
		jr  nz, bt_play_frame
		jr  bt_play_row
.bt_play_key
		ld  hl, 1
		jr  bt_play_out
.bt_play_end
		ld  hl, 0
.bt_play_out
		xor a
		ld  (bt_frame), a
		ld  a, (_bt_border)
		and 7
		out ($fe), a                    ; speaker off
		ret

; bt_start: HL = song. Reads the header, starts at the first order entry.
.bt_start
		ld  (bt_base), hl
		ld  a, (hl)
		ld  (bt_speed), a
		inc hl
		ld  a, (hl)
		ld  (bt_ordlen), a
		inc hl
		ld  a, (hl)
		ld  (bt_loopto), a
		xor a
		ld  (bt_ord), a
		ld  (bt_frame), a
		ld  h, a
		ld  l, a
		ld  (bt_stepa), hl
		ld  (bt_stepb), hl

; bt_loadpat: the pattern of order entry bt_ord -> bt_ptr, bt_left
.bt_loadpat
		ld  hl, (bt_base)
		ld  de, 4
		add hl, de                      ; the order list
		ld  a, (bt_ord)
		ld  e, a
		add hl, de
		ld  c, (hl)                     ; C = pattern number
		ld  hl, (bt_base)
		ld  e, 4
		add hl, de
		ld  a, (bt_ordlen)
		ld  e, a
		add hl, de                      ; the pattern offsets
		ld  e, c
		add hl, de
		add hl, de
		ld  e, (hl)
		inc hl
		ld  d, (hl)
		ld  hl, (bt_base)
		add hl, de                      ; the pattern
		ld  a, (hl)
		ld  (bt_left), a
		inc hl
		ld  (bt_ptr), hl
		ret

; bt_row: reads the next row into bt_stepa / bt_stepb. Carry set: the song
; is over (only when not looping).
.bt_row
		ld  a, (bt_left)
		or  a
		jr  nz, bt_row_go
		ld  a, (bt_ord)                 ; pattern over: next order entry
		inc a
		ld  hl, bt_ordlen
		cp  (hl)
		jr  c, bt_row_next
		ld  a, (bt_loopf)
		or  a
		jr  nz, bt_row_wrap
		scf
		ret
.bt_row_wrap
		ld  a, (bt_loopto)
.bt_row_next
		ld  (bt_ord), a
		call bt_loadpat
		jr  bt_row
.bt_row_go
		dec a
		ld  (bt_left), a
		ld  hl, (bt_ptr)
		ld  a, (hl)
		inc hl
		ld  de, bt_stepa
		call bt_note
		ld  a, (hl)
		inc hl
		ld  de, bt_stepb
		call bt_note
		ld  (bt_ptr), hl
		or  a                           ; carry clear
		ret

; bt_note: A = note code, DE = where its step goes. Keeps HL.
.bt_note
		or  a
		ret z                           ; 0: keep the note
		push hl
		ld  hl, 0
		dec a
		jr  z, bt_note_set              ; 1: silence (step 0)
		dec a
		add a, a
		ld  l, a
		ld  bc, bt_notes
		add hl, bc
		ld  a, (hl)
		inc hl
		ld  h, (hl)
		ld  l, a
.bt_note_set
		ex  de, hl
		ld  (hl), e
		inc hl
		ld  (hl), d
		pop hl
		ret

; bt_tone: BC = turns (130 T-states each) of the current notes.
.bt_tone
		ld  a, b
		or  c
		ret z
		ld  a, c                        ; C counts the inner loop, B the
		or  a                           ; outer one
		jr  z, bt_tone_c0
		inc b
.bt_tone_c0
		ld  a, (_bt_border)
		and 7
		ld  (bt_or1 + 1), a
		ld  (bt_or2 + 1), a
		ld  a, i                        ; P/V = were interrupts on?
		push af
		di
		exx
		ld  hl, (bt_accb)
		ld  de, (bt_stepb)
		exx
		ld  hl, (bt_acca)
		ld  de, (bt_stepa)
.bt_turn
		add hl, de                      ; channel A          11
		ld  a, h                        ;                     4
		rla                             ; top bit -> carry    4
		sbc a, a                        ; 0 or $FF            4
		and 16                          ; speaker bit         7
.bt_or1
		or  0                           ; border             7
		out ($fe), a                    ;                    11
		nop                             ; (so that A and B    4
		nop                             ;  sound for about    4
		nop                             ;  the same time)     4
		exx                             ; channel B           4
		add hl, de                      ;                    11
		ld  a, h                        ;                     4
		rla                             ;                     4
		sbc a, a                        ;                     4
		and 16                          ;                     7
.bt_or2
		or  0                           ;                     7
		out ($fe), a                    ;                    11
		exx                             ;                     4
		dec c                           ;                     4
		jp  nz, bt_turn                 ;                    10 = 130
		dec b
		jp  nz, bt_turn
		ld  (bt_acca), hl
		exx
		ld  (bt_accb), hl
		exx
		pop af
		ret po                          ; interrupts were off
		ei
		ret

; steps of the notes C-1 .. B-7: 65536 * f * 130 / 3500000
.bt_notes
	defw 80,84,89,95,100,106,113,119,126,134,142,150
	defw 159,169,179,189,201,213,225,239,253,268,284,301
	defw 318,337,357,379,401,425,450,477,505,536,567,601
	defw 637,675,715,757,802,850,901,954,1011,1071,1135,1202
	defw 1274,1349,1430,1515,1605,1700,1801,1908,2022,2142,2269,2404
	defw 2547,2699,2859,3029,3210,3400,3603,3817,4044,4284,4539,4809
	defw 5095,5398,5719,6059,6419,6801,7205,7634,8087,8568,9078,9618
#endasm

#endif
