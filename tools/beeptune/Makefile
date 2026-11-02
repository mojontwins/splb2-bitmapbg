# Beeptune - Linux / macOS (XQuartz) / BSD build. Needs a C compiler and X11 headers.
#   make            -> beeptune          (GUI + command line)
#   make headless   -> beeptune-headless (scripted test build, no window, no sound)
#   make windows    -> beeptune.exe      (cross compile with mingw-w64 / llvm-mingw)
CC      ?= cc
CFLAGS  ?= -O2 -Wall -Wextra -std=c99 -D_DEFAULT_SOURCE
SRC      = src/main.c src/app.c src/ui.c src/song.c src/bmp.c
X11INC  ?= $(shell pkg-config --cflags x11 2>/dev/null)
X11LIB  ?= $(shell pkg-config --libs x11 2>/dev/null || echo -lX11)
WINCC   ?= x86_64-w64-mingw32-gcc

all: beeptune

beeptune: $(SRC) src/audio.c src/plat_x11.c src/*.h
	$(CC) $(CFLAGS) $(X11INC) -o $@ $(SRC) src/audio.c src/plat_x11.c $(X11LIB) -lm

headless: beeptune-headless
beeptune-headless: $(SRC) src/audio.c src/plat_headless.c src/*.h
	$(CC) $(CFLAGS) -DBEEPTUNE_NOAUDIO -o $@ $(SRC) src/audio.c src/plat_headless.c -lm

windows: beeptune.exe
beeptune.exe: $(SRC) src/audio.c src/plat_win32.c src/*.h
	$(WINCC) -O2 -Wall -std=c99 -o $@ $(SRC) src/audio.c src/plat_win32.c -mwindows -static -lgdi32 -lcomdlg32 -lshell32 -lwinmm

clean:
	rm -f beeptune beeptune-headless beeptune.exe

.PHONY: all headless windows clean
