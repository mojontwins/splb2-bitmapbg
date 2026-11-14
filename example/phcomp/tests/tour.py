#!/usr/bin/env python3
"""tests/tour.py GAME N > script: visit rooms 0..N-1 of game GAME (menu
key GAME+1) by poking the right exit, a screenshot of each."""
import sys
g, n = int(sys.argv[1]), int(sys.argv[2])
print('wait 30\ntap %d\nwait 50' % (g + 1))
for r in range(n):
    print('poke flicker 0 250\npoke ex_r 0 %d\npoke py 0 40\npoke exit_dir 0 2\nwait 40\nshot r%02d' % (r, r))
