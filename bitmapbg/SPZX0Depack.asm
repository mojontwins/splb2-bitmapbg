; SPZX0Depack
; BITMAPBG extension for Sprite Pack 2.2
;
; ZX0 decompressor ("turbo" version) by Einar Saukas & introspec,
; BSD 3-Clause licence, see NOTICE.md. Data is compressed with the zx0 tool
; (https://github.com/einar-saukas/ZX0), forwards (no -b option).
;
; Typical BITMAPBG use: depack a picture or a screen third straight into the
; shadow screen (SP_BGSHADOW + n*2048 is third n, in display-file layout), so
; nothing has to be drawn on the real screen and captured.
;
; Self-modifying: the library must be in RAM (it always is on a Spectrum).
;
; enter: HL = compressed data, DE = destination
; exit : DE = one past the last byte written
; uses : af, bc, de, hl

XLIB SPZX0Depack

.SPZX0Depack
        ld      bc, $ffff               ; preserve default offset 1
        ld      (zx0_last_offset+1), bc
        inc     bc
        ld      a, $80
        jr      zx0_literals
.zx0_new_offset
        ld      c, $fe                  ; prepare negative offset
        add     a, a
        jp      nz, zx0_new_offset_skip
        ld      a, (hl)                 ; load another group of 8 bits
        inc     hl
        rla
.zx0_new_offset_skip
        call    nc, zx0_elias         ; obtain offset MSB
        inc     c
        ret     z                       ; check end marker
        ld      b, c
        ld      c, (hl)                 ; obtain offset LSB
        inc     hl
        rr      b                       ; last offset bit becomes first length bit
        rr      c
        ld      (zx0_last_offset+1), bc ; preserve new offset
        ld      bc, 1                   ; obtain length
        call    nc, zx0_elias
        inc     bc
.zx0_copy
        push    hl                      ; preserve source
.zx0_last_offset
        ld      hl, 0                   ; restore offset
        add     hl, de                  ; calculate destination - offset
        ldir                            ; copy from offset
        pop     hl                      ; restore source
        add     a, a                    ; copy from literals or new offset?
        jr      c, zx0_new_offset
.zx0_literals
        inc     c                       ; obtain length
        add     a, a
        jp      nz, zx0_literals_skip
        ld      a, (hl)                 ; load another group of 8 bits
        inc     hl
        rla
.zx0_literals_skip
        call    nc, zx0_elias
        ldir                            ; copy literals
        add     a, a                    ; copy from last offset or new offset?
        jr      c, zx0_new_offset
        inc     c                       ; obtain length
        add     a, a
        jp      nz, zx0_last_offset_skip
        ld      a, (hl)                 ; load another group of 8 bits
        inc     hl
        rla
.zx0_last_offset_skip
        call    nc, zx0_elias
        jp      zx0_copy
.zx0_elias
        add     a, a                    ; interlaced Elias gamma coding
        rl      c
        add     a, a
        jr      nc, zx0_elias
        ret     nz
        ld      a, (hl)                 ; load another group of 8 bits
        inc     hl
        rla
        ret     c
        add     a, a
        rl      c
        add     a, a
        ret     c
        add     a, a
        rl      c
        add     a, a
        ret     c
        add     a, a
        rl      c
        add     a, a
        ret     c
.zx0_elias_loop
        add     a, a
        rl      c
        rl      b
        add     a, a
        jr      nc, zx0_elias_loop
        ret     nz
        ld      a, (hl)                 ; load another group of 8 bits
        inc     hl
        rla
        jr      nc, zx0_elias_loop
        ret
; -----------------------------------------------------------------------------
