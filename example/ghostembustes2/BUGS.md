# Ghost Embustes 2: the bugs of the original

Found while porting `orig/chem2.bas.txt` (the BASIC listed from
`orig/CHEM2.Z80`), most of them confirmed by running the original in an
emulator. Line numbers are the BASIC's. "Port" says what
`src/ghostembustes2.c` does; the comments marked `BUG:` there point at the
same places.

## Crashes

| Line | What happens | Port |
|---|---|---|
| 1050 | The shop's cursor can go down to row 21. Its lower half is then printed on row 22, and the program stops with *5 Out of screen*. | The cursor stops at row 20. |
| 9200 (via 8041) | FIRE ("0") makes the little man jump 6 columns right and 3 rows up, with no bounds check. Jumping from column 26 or more, or from rows 0-2, stops the program: *Out of screen* / *Integer out of range*. Every part that uses the movement routine at 8000 has this jump, not just Gregory. | The jump only exists in Gregory, and it collides with walls and the screen edges. |
| 3980 | Climbing the pole in Gregory 2 does `w=w-5` without checking. From rows 0-4 the next `PRINT AT w,a` gets a negative row and stops the program. | The pole is a ladder. |
| 6010-6110 | The mine detector reads `SCREEN$ (w,a-1)`, `(w,a+1)` and `(w-1,a)` with no checks. In column 0 or 31, or on row 0, the program stops with *B Integer out of range*. The walls don't stop you getting there (see "walls" below). | The walls are solid, and the detector only looks at cells inside the room. |
| 8050, 9000 | Dying jumps out of a `GO SUB` with `GO TO`, and the platform ride (9000) leaves its `GO SUB` the same way. Each time, the GOSUB stack grows by one entry. In a 38K program, enough deaths end in *4 Out of memory*. | Normal function returns. |

## The game doesn't work as written

| Line | What happens | Port |
|---|---|---|
| 8060 and everywhere | `SCREEN$` never recognises UDGs. So `IF SCREEN$ (w,a)="T"` is never true, and no wall is ever solid: you can walk through the bricks and room walls in every part. The message "¡no toques las paredes! o si no, tendrás que empezar de nuevo todo" is a lie, because touching the walls does nothing. | Walls (T) are solid. Only the spikes kill, which is what the code actually tested. |
| 3810-4130 | Gregory has no gravity. You fly anywhere, which makes the three screens trivial. The code shows what was meant: a jump (9200), a moving platform over spikes, poles to climb (the F characters), and death by falling ("Caes. Mueres.", 4120). | A small platform engine: gravity, ladders on the F characters, a FIRE jump of 3 up and 6 across like 9200, spikes and long falls kill. |
| 9500-9530 | In Gregory 3, pressing up or down anywhere leaves an F pillar behind. Since F isn't empty, `SCREEN$ (w+1,a)=" "` (the falling test) is false over it, so you can climb anywhere. The special climbing spots at 4050-4100 are never needed. | F cells are the only ladders. |
| 4070 | One of those spots expects you at row 12, but 4060 leaves you at row 11. | Same as above. |
| 3850-3920 | The platform tests never look at the row. Standing anywhere in column 16, at any height, makes you ride the platform. Standing at column 16 or more while it's elsewhere kills you, again at any height. On the way back the test is `a=16` instead of `a>=16`, so columns 17-28 are safe then. | The platform is a sprite you stand on. You fall into the pit only if you walk off it. |
| 4120 | Falling in Gregory 3 goes to line 4000, which is Gregory 2's loop. Screen 3 stays on view, and you play screen 2's rules on it. | Dying repeats the current screen. |
| 9100, 8050 | Dying in Gregory either `RUN`s the whole game from scratch (9100), or does `GO TO sigue` with `sigue` still 0 (8050). `GO TO 0` restarts at line 10 but keeps the variables. | Dying repeats the current screen. |
| 3260, 3280 | The Prueba's ghost is printed as `" R"` at column n, so it really is at column n+1. The hit test uses n: firing when the ghost is right above Tracy (column 28) misses, and firing when it has already passed (columns 29-31) hits. At n=31 the ghost is printed on the next row, at (11,0). | You hit when the ghost is over Tracy's column. |
| 3270 | After a miss, the ghost on row 10 isn't erased: the code erases column n, but the ghost is at n+1. | Sprites. |
| 4850-4860 | The come-come is won when `LAP = 22*32`. LAP counts loops, not eaten characters, and the clock starts at 22*32+120. Standing still wins with 120 to spare. | You win when no character is left, including the spaces of the inverse "PULSA UNA TECLA". |
| 6080-6110 | The mine detector prints "nada cerca" if ANY of the four cells around you is empty. That's nearly always, even with a mine next to you. Lines 6050-6070 `PRINT` without `AT`, at wherever the cursor happens to be, and 6060 tests a diagonal. | "Una mina cerca" if a mine is up, down, left or right of you, "nada cerca" if not. |
| 4680 | `IF w=1 OR e=3`, a typo for `e=1` (compare 4460). The second electric fence is live half as often as the first. | `e=1 OR e=3`. |
| 1070, 1080 | Buying repeats on every loop while 0 is held, so one press can buy several things and put you in the red (which restarts the game, 1112). | One press, one purchase. |
| 60, 380 | The intro says to drive to the flashing buildings and press FIRE when you get there. Nothing flashes, and you go in automatically. | Your house flashes, and FIRE takes you in. |
| 8000 | Moving prints a space where you were, so the little man erases whatever he walks over: scenery, walls, mines. The come-come is the only part that relies on this. | Sprites; only the come-come eats. |
| 9200 | The jump also prints spaces along its arc, so it can dig holes through the jefe's rows of spikes. | (no jump there) |
| 5080 | The 51 spikes of the last field can land on the walls (row 21, column 31), making holes in them, or on you. | Inside the room, not next to you. |
| 4400-4410 | A rock only hits you if it's exactly on your cell after you move. If you and the rock swap cells, you pass through each other. | Box overlap between two sprites. |

## Wrong on screen

| Line | What happens | Port |
|---|---|---|
| 110 | `OVER 1` is set for the accent of "tú" (`"tu"+CHR$ 8+"'"`), but it's on while the whole caption is typed. "Ese eres tu" is XORed over "Es el Ghost-tipo" and comes out as garbage. | The caption line is cleared first. Real accents. |
| 9900 | The typewriter cuts words at column 32 ("par/padeantes"). The texts were hand-padded with spaces to avoid that, but not all of them were. | Word wrap. |
| 1000 | No `CLS`: the shop is drawn over the street, which shows around it, and its bottom line keeps the street's messages. | Clean screen. |
| 1030 | The cursor is printed over the "---" of "---Caza-busters" and erases it. | The cursor is a sprite. |
| 1111 | "Dinero:" is printed on row 20, over "---Desmaterializador del ayu.". | Row 22. |
| 3230 | Clearing the Prueba's text starts at row 3, so the first line of the text stays. | Rows 2-10. |
| 3281 | `PRINT AT 21,0;n`, a debugging leftover, shows the ghost's column. | Gone. |
| 3410 | No `CLS`: the Trampa's city is drawn over the Prueba (Tracy, the beam, the explosion). | Clean screen. |
| 3460 | "Y los Ghost empiezan sus corridas..." wraps onto row 10 and leaves the end of "Se hace denoche..." after it. | Row 10 cleared, word wrap. |
| 3480 | `PRINT AT 7,0;" "` uses the permanent white paper and leaves a white square in the night. | Gone. |
| 3570 | The flashing "GHOST EMBUSTES" title ends in black on black, invisible. | It stays white. |
| 3950-3960 | "Gregory, Pantalla 2" is printed, then covered at once by the screen. | At the bottom. |
| 4010 | No `CLS`: screen 3 is printed over screen 2, whose grass stays at the top right. | Clean screen. |
| 6010 | The detector's message is printed over the top wall, and the shorter "nada cerca" leaves the end of "Una mina cerca" there. | Kept on the wall, which is redrawn before each message. |
| 5020 | "!!Bien!!" is printed at wherever the cursor is. | Bottom line. |
| 5140 | The choice screen inherits black ink on blue paper and is hard to read. | White ink. |

## Harmless

* **Never used:**
  * `hov` (Caza-busters bought) and `k` (boxes bought), 1070-1080;
  * `pun = money*26` (4280);
  * `rutina de movimiento = 8000` (3650);
  * the chosen character's `m$`, `n$`, `cs` (5150-5170): they were for the next part, which isn't on this tape. The port shows the chosen character at the end.
* **Dead code:**
  * line 1999 (`GO SUB 2000` lands on 2001);
  * the `STOP`s at 7999 and 9998;
  * the dangling `AT 5,2` at the end of line 250;
  * `IF INKEY$="" THEN GO TO 4150` at 4140, which goes to 4150 anyway.
* **Messages:** the red fence (4480) kills you by setting `w`, `n`, `a` and `o` to 0 and jumping into the middle of a `FOR` loop (4410), whose test then "catches" you. It works, but the message is "Por favor, pulsa una tecla", not one about electricity.
* **UDGs:** line 9700 defines only the UDGs S, T and U. A to R come from whatever was already in memory: the snapshot has them, but a plain `LOAD ""` of this BASIC alone wouldn't.
* **Typos** are kept as written ("Prueva" in a REM, "denoche", "por lo tanta", "LO CONSEGUISTES", "lapantalla").
