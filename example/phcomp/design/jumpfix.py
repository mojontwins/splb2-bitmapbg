#!/usr/bin/env python3
"""One-off (kept as a record): rooms changed for the Phantomas jumps
(high: 32 px up, 16 across; long: 16 up, 32 across). Edits design/*.map;
then run mkrooms.py, tools/edges.py and tools/reach.py."""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import maplib
D = os.path.dirname(os.path.abspath(__file__))
maps = dict((f, maplib.MapFile(os.path.join(D, f + '.map'))) for f in ('brujas', 'luna', 'vee', 'infinito'))
where = {}
for f, m in maps.items():
    for n in m.rooms(): where[n] = m

def rows(name, spec):
    """spec: {row: 32 characters} replacing whole rows"""
    g = list(where[name].grid(name))
    for r, line in spec.items():
        g[r] = (line + ' ' * 32)[:32]
    where[name].set_grid(name, g)

# Brujas 05: stepping blocks across the pit
rows('bru_05', {
	13: '               LL',
	14: '               LL',
	15: '               BB',
	16: '               BB',
	17: '          BB   BBBBBBBBBBBBBBBBB',
	18: '          BB   BBBBBBBBBBBBBBBBB',
	19: '     BB   BB   BBBBBBBBBBBBBBBBB',
	20: 'BB   BB   BB   BBBBBBBBBBBBBBBBB',
	21: 'BB   BB   BB   BBBBBBBBBBBBBBBBB'})
# Brujas 07: ledges 4 characters apart up to the top
rows('bru_07', {
	2:  '       BBBB                   BB',
	3:  '                              BB',
	4:  '                              BB',
	5:  '  BBB                         BB',
	6:  '                              BB',
	7:  '            LL                BB',
	8:  '       BBB  LL                BB',
	9:  '            PP                BB',
	10: '            PP                BB',
	11: '            PP                BB',
	12: '  BBB       PP                BB',
	13: '            PP                BB',
	14: '            PP                BB',
	15: '            PP        LL      BB',
	16: '       BB   PP        LL      BB',
	17: '            PP        BB      BB',
	18: '            bb        BB      BB',
	19: '            bb        BB      BB'})
# Brujas 07b: land a row lower, a step to the building
rows('bru_07b', {
	16: '                     BBB   BBBBB',
	17: '                           BBBBB',
	18: '         BBBBBBBB          BBBBB'})
# Brujas 07c: towers closer
rows('bru_07c', {
	11: 'BBBBBBB',
	12: 'BBBBBBB   BBBBBB',
	13: 'BBBBBBB   B    B          BBBBBB',
	14: 'BBBBBBB   B LL B          BBBBBB',
	15: 'BBBBBBB   B LL B          BBBBBB',
	16: 'BBBBBBB   BBBBBB BB  BB   BBBBBB',
	17: 'BBBBBBB   BBBBBB BB  BB   BBBBBB',
	18: 'BBBBBBB   BBBBBB BB  BB   BBBBBB',
	19: 'BBBBBBB   BBBBBB BB  BB   BBBBBB',
	20: 'BBBBBBB   BBBBBB BB  BB   BBBBBB',
	21: 'BBBBBBB   BBBBBB BB  BB   BBBBBB'})
# Brujas 09: the two tall pillars a row lower
rows('bru_09', {
	13: '',
	14: '     LL                   LL',
	15: 'LL   LL         LL        LL',
	16: 'LL   BB    LL   LL        BB'})
# Brujas 11: stones a long jump apart (8 columns)
rows('bru_11', {
	15: '                LL',
	16: '                LL            BB',
	17: 'BB      BB      BB        BB  BB',
	18: 'BB      BB      BB        BB  BB',
	19: 'BB      BB      BB        BB  BB',
	20: 'BB      BB      BB        BB  BB',
	21: 'BB      BB      BB        BB  BB'})
# V1 12: the upper stairs moved right, room to jump onto the lower one
rows('v1_12', {
	11: '                           RRR',
	14: '                  RRRHHHRRR'})
# V2 07: a way up to the top of the left wall
rows('v2_07', {
	14: 'KKKK              JJ      KKKK',
	15: 'JJKK  rrrr',
	17: 'KK          rrrr      rrrr'})
# V2 12: steps up to the treasure
rows('v2_12', {
	6:  '          pppp',
	9:  '                pppppp',
	12: '          pppppp',
	15: '                 pppppp',
	18: 'YYTT      ppppp           TTTT'})
# Infinito: the pit 4 characters wide
for r in ('i1_08', 'i3_08'):
    rows(r, dict((k, 'MMMMMMMMMMMM    MMMMMMMMMMMMMMMM') for k in range(16, 22)))
rows('i3_08', {15: ''})
for m in maps.values(): m.save()
print('jumpfix.py: done')
