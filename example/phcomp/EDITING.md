# Editing the games

Three text files make every game: `gfx/rooms.txt` (what each room looks
like), `gfx/games.txt` (how the rooms connect and what moves in them) and
`gfx/texts.txt` (words printed in rooms). Run `./build.sh` after editing:
it converts them, checks them and makes `phcomp.tap`.

## Coordinates

The play area is 32 columns x 22 rows of characters (rows 0-21; rows 22
and 23 are the score line).

* **Rooms** (`rooms.txt`) use **characters**: X 0-31, Y 0-21.
* **Enemies and start positions** (`games.txt`) use **pixels** for the
  top-left corner of a 16x16 figure: x 0-240, y 0-160. Pixel = character
  x 8. A figure standing on a floor whose top row is R has
  **y = R x 8 - 16**: on row 20, y = 144; on row 16, y = 112.

## 1. Drawing a room (`rooms.txt`, with the room editor)

Load `gfx/tiles.png` as the editor's tileset. Each room is

```
room bru_03
header 1 0          backdrop, text set
rect 0 18 64 80 31 21
...
end
```

* **Colours are behaviour**: BRIGHT = solid, FLASH = deadly (water, fire,
  spikes; FLASH isn't shown), BRIGHT + FLASH = a platform you can jump up
  through and stand on. Anything else is scenery the player walks past.
  Tile 255 is an invisible wall.
* **zigzag** `X Y CODE ATTR X2 Y2` (tool 5 in roomed): the rectangle
  filled with tiles CODE and CODE+1 taking turns, each row starting one
  cell further on: staggered bricks made of a left half and a right half,
  like Infinito's floors (tiles 176 and 177). The top-left cell is CODE.
* **Header, first number: the backdrop.** Backdrops are rooms whose name
  starts with `bg `, counted in file order: 1 bg brujas, 2 bg earth, 3 bg
  cave, 4 bg moon, 5 bg storm, 6 bg sky, 7 bg mountains, 8 bg andromeda.
  The backdrop is drawn first, then the room on top. 0 = plain black.
* **Header, second number: the texts**, a set of `texts.txt` (0 none).
* The player and the enemies paint their own ink on blank cells, so leave
  the background blank (tile 0) where things move; drawn tiles keep their
  own colours under a sprite.

**Neighbouring rooms must meet.** Where a room exits right, its columns
30-31 must hold the same kind of cell (free, solid, deadly, platform), row
by row, as columns 0-1 of the room it leads to; for exits up and down,
rows 20-21 of the upper room against rows 0-1 of the lower one. Otherwise
you can come into a room inside a wall. `tools/edges.py` lists every
place where they don't meet; the build prints the count.

**Can you get through?** `tools/reach.py` simulates the player's jumps in
every room, from every exit it can enter by, and lists exits it can't get
to (`tools/reach.py bru_03` for one room). It ignores enemies and moving
platforms, so a room crossed on a raft always shows up (bru_06). It
doesn't know that some ways are one-way (dropping down a hole): "from D
cannot reach..." for a room you can only fall out of is fine. Jumps, to
design by: the high jump climbs 44 pixels (5 rows: a floor 5 rows higher
is out of reach, 3-4 rows is fine), the long jump climbs 20 and crosses
about 8 columns.

## 2. Linking rooms and placing enemies (`games.txt`)

```
game brujas
	menu "EN EL PAIS BRUJIL"        menu line and score line
	title "PHANTOMAS EN EL PAIS BRUJIL"
	ink 1                           the player's colour
	lives 5
	start bru_01 24 112             first room, x, y (pixels)
	ending "GRACIAS, GRAN HOMBRE..."
	room bru_01 - bru_02 - -        exits: left right up down
		bouncer 40 40 200 96 1 bubble 5
	room bru_02 bru_01 bru_03 - -
		hopper 64 112 224 24 2 witch 0
	...
end
```

* **The order of the rooms doesn't matter**: the map is made only by the
  exits. `room A L R U D` names, for room A, the rooms reached by leaving
  it through each edge (`-`: none). An edge with no exit is a wall, except
  the bottom: falling off it kills.
* Make links both ways (if A's right exit is B, B's left exit should be
  A) unless you want a one-way passage, like the holes you drop through.
* When you change rooms you keep your height (left/right) or your column
  (up/down): that is why the edges must meet.
* Add `fly` after the exits to fly in that room (Q/A up and down).
* After dying you come back where you entered the room, and the enemies
  start over.

**Enemies**: up to 4 per room, one line each after the room line:

```
TYPE X1 Y1 X2 Y2 SPEED FIGURE INK
```

FIGURE is a name of `gfx/sprites.txt` (witch, bat, bubble, blob, alien,
astronaut, android, cop, drop, bird, broom, ball, hopper, devil, knight,
rocket, computer, fire, platform, heart, key...). INK 0-7 (add 64 for
BRIGHT).

| TYPE | moves | X1 Y1 X2 Y2 | SPEED |
|---|---|---|---|
| linear | back and forth between two points (horizontal, vertical or diagonal) | the two points | pixels a frame |
| chaser | walks along row Y1 towards you | X1..X2 its limits, Y2 unused (0) | pixels (moves every other frame) |
| random | walks along row Y1, changing direction at random | X1..X2 limits | pixels |
| drop | falls from (X1,Y1) to Y2, splashes, waits X2 frames, again | | unused |
| ghost | appears at a random place on row Y1 within X1..X2, vanishes | | time shown, in 8-frame units |
| bouncer | bounces diagonally inside the box | the box corners | pixels |
| hopper | walks X1..X2 on row Y1 hopping | Y2 = hop height (below 16: half hops) | pixels |
| platform | like linear, harmless, carries you | the two points | pixels |
| goal | stands at (X1,Y1); touching it ends the game | Y2 = 1: only if you carry the item | unused |
| item | stands at (X1,Y1); touching it picks it up (once per game) | | unused |
| faller | drops from the sky at a random column of X1..X2, from Y1 to Y2, then walks there at random | | walking pixels |
| toggle | not a sprite: cells (X1,Y1)-(X2,Y2) **in characters** switch every SPEED x 8 frames between deadly and empty. FIGURE = tile number, INK = attribute | | |

Speeds: keep the distance X2-X1 (and Y2-Y1) a multiple of the speed so
the turns are exact (the engine clamps anyway). Enemies ignore walls:
give them a path that looks right.

## 3. Texts (`texts.txt`)

```
text 2
4 8 56 "TRINCA LA ESCOBA"           row, column, PAPER*8+INK (+64 BRIGHT)
```

Numbered from 1 without gaps; capitals, digits and punctuation only.

## 4. New graphics

* `gfx/tiles.png`: 256x64, 32 tiles a row, white = ink. Tile number = row
  x 32 + column. 178 are used; add new ones after the last.
* `gfx/sprites.png`: 16x16 frames, 16 a row; name them in
  `gfx/sprites.txt` (4 frames: left, left, right, right; 2: animation;
  1: still). Red pixels = a mask you draw yourself; otherwise it is made
  automatically.

The binary ends near $DA80 and must stay under $EB80: about 4K left for
rooms and enemies (a room is 20-80 bytes).
