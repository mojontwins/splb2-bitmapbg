# roomed - Linux / BSD / macOS (XQuartz) build. Needs a C compiler and X11 headers.
#   make            -> roomed            (GUI + command line)
#   make headless   -> roomed-headless   (scripted test build, no window)
#   make windows    -> roomed.exe        (cross compile with mingw-w64 / llvm-mingw)
CC      ?= cc
CFLAGS  ?= -O2 -Wall -Wextra -std=c99 -D_DEFAULT_SOURCE
SRC      = src/main.c src/app.c src/rooms.c src/ui.c src/img.c
X11INC  ?= $(shell pkg-config --cflags x11 2>/dev/null)
X11LIB  ?= $(shell pkg-config --libs x11 2>/dev/null || echo -lX11)
WINCC   ?= x86_64-w64-mingw32-gcc

all: roomed

roomed: $(SRC) src/plat_x11.c src/*.h
	$(CC) $(CFLAGS) $(X11INC) -o $@ $(SRC) src/plat_x11.c $(X11LIB) -lm

headless: roomed-headless
roomed-headless: $(SRC) src/plat_headless.c src/*.h
	$(CC) $(CFLAGS) -o $@ $(SRC) src/plat_headless.c -lm

windows: roomed.exe
roomed.exe: $(SRC) src/plat_win32.c src/*.h
	$(WINCC) -O2 -Wall -std=c99 -o $@ $(SRC) src/plat_win32.c -mwindows -static -lgdi32 -lcomdlg32 -lshell32

clean:
	rm -f roomed roomed-headless roomed.exe

.PHONY: all headless windows clean
