# CJMC93: bugs in the original, and what the port does

Line numbers are those of `orig/cjmc93.bas.txt`. Section names (**3.1**,
**PHYSICS**...) are those of `GAME.md`.

## Bugs that change the game

### 1. Room 3.1 can't be left
Line 1212 opens the wall in column 4 with two single holes, at rows 10
and 13. The player is two characters tall, and moving left needs both
cells free (`ATTR (y-1, x-1)` and `ATTR (y, x-1)`). No row has both
holes, so there is no way out of the room.
**Port**: the hole at row 13 is now two cells tall (rows 13 and 14;
in `orig/rooms-print.txt`, carried into `gfx/rooms.txt`). The player can walk off the ledge on row 15 through
it.

### 2. Invisible death in rooms 3.1 and 3.2; the balls are harmless
Line 1235 checks for death with `x = e`, `y = f`: the variables of room
1.5's monster, not the balls'. `e` and `f` still hold whatever 1.5 left
in them (a column from 5 to 20, a random number from 1 to 10). So there
is an invisible deadly spot somewhere in the upper half of the room, and
the balls go straight through the player.
**Port**: the balls kill (`BALLS_KILL 1`). There is no invisible spot.
Set `BALLS_KILL 0` for harmless balls.

### 3. The player can't move in 4.1
The drawbridge scene (sub 9240) paints the sky with `INK 1`. Every cell
there has attribute 1, not 7, and the movement code treats everything
that isn't 7 as solid. The player is stuck where he appears (column 26,
row 13). All he can do is shoot, and wait for the guardian to pass his
row.
**Port**: collision comes from the room layout, not from the colours on
screen, so the player walks the bridge. The bridge deck and the chain are
solid, so the chain can be climbed to reach the guardian's height.

### 4. The guardian's ray ends the game
Line 3100: when the ray hits, `PAUSE 0: RUN`. That is game over, however
many lives are left.
**Port**: it costs one life, like every other death. The bridge is
redrawn and the fight starts again.

### 5. Room 1.7 is drawn over the box room
After **1.S**, line 2120 does `GO TO back` (line 660). That skips line
650, the `RANDOMIZE USR 61913` that clears the room. The bricks of the
box room stay under the dragon's head.
**Port**: every room is drawn from scratch (`room_draw ()`).

### 6. Collisions only on exact positions
Every touch test compares cells exactly. In 1.4 it is `x = e`, though the
knight is two characters wide. In 1.5 it is `x = e`, though the monster
is drawn at `e+1`. You die one column to the left of it and walk through
its right half. 3.3 and the balls have the same problem.
**Port**: rectangles that overlap (`player_touch ()`, sizes in `at_*`).

### 7. The jump hangs under ceilings
Sub 8031 counts six jump steps, but rises only when the two cells above
are free. When the head hits something, the player hangs there for the
remaining steps.
**Port**: a head bump ends the rise and the fall starts (**PHYSICS**).

### 8. Platforms teleport the player
Lines 1010-1043: when the player is on a platform's row and within one
column of it, `LET x = p`: he jumps sideways onto it. Platform 2's test
also repeats `x = p2` (line 1042).
**Port**: the player stands on platforms and is carried by them,
pixel by pixel (top of `player_move ()`).

### 9. Room 1.4 has two exits
In the first half of the knight's loop the exit is `x = 4` (line 300). In
the second half it is `x = 5` (line 350).
**Port**: always column 4.

## Small things

* **Fourth platform**: line 952 sets up four platforms (`p4`, `d4`, `c4`),
  but only three are moved and drawn.
* **Lives over the message**: sub 7999 prints the lives at row 21 on
  every turn of every room. In 4.1 that overwrites a letter of the typed
  message. **Port**: no lives on screen in 4.1.
* **Flashing box (1.6)**: `FLASH 1` can't be used with splib2 here (it
  marks cells sprites must not be drawn in). **Port**: `flash_box ()`
  swaps ink and paper every 16/50 s.
* **Random colours** of the platforms (2.x) and the colour of the
  figures: sprites take the colour of the background: yellow in the
  castle's and the cave's game area (`INK 6`, for the pictures), white
  on the bridge.
* **Redrawing every turn**: 3.1/3.2 loop back to line 1216, which redraws
  the room's ledges every turn (lines 1216 and 1221). Harmless, but
  slow.

## Unreachable code

* Lines 1530-1640: an earlier guardian fight, skipped by line 1520's
  `GO TO 2140`.
* Line 9330 (*HAS LLEGADO AL FINAL DE LA PRIMERA CARGA. CLAVE:OJO*): only
  reached from that dead code.
* Line 8034 (key **8** blanks the player) comes after a `RETURN`.
* Subs 9030 and 9100, and the first loop of line 9310 (it prints commas
  with `OVER 1`: nothing visible).
