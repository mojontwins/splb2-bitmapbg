"""Reads and writes the design/*.map files (see mkrooms.py) so that tools
can change rooms: load(path) -> MapFile, MapFile.save().
Rooms are kept as 22 strings of 32 characters; backdrops (rooms with
layers) are kept as their original text."""
import re

class MapFile:
    def __init__(self, path):
        self.path = path
        self.prelude = []           # lines before the first room
        self.items = []             # ('room', name, header, grid) / ('raw', text)
        lines = open(path).read().split('\n')
        i = 0
        while i < len(lines) and not lines[i].startswith('room '):
            self.prelude.append(lines[i]); i += 1
        while i < len(lines):
            l = lines[i]
            if not l.startswith('room '):
                i += 1; continue
            m = re.match(r'room\s+(.+?)((\s+\d+){0,2})\s*$', l)
            name, header = m.group(1), m.group(2).strip()
            start = i
            if i + 1 < len(lines) and lines[i + 1].strip() == 'rows':
                grid = [' ' * 32 for _ in range(22)]
                layered = False
                i += 2
                while lines[i].strip() != 'endrows':
                    if lines[i].strip() == 'over': layered = True
                    elif lines[i].strip():
                        mm = re.match(r'\s*(\d+)\|(.*)$', lines[i])
                        grid[int(mm.group(1))] = (mm.group(2) + ' ' * 32)[:32]
                    i += 1
                i += 1
            else:
                grid = [((lines[i + 1 + k] if i + 1 + k < len(lines) else '') + ' ' * 32)[:32]
                        for k in range(22)]
                i += 23
                layered = i < len(lines) and lines[i].strip() == 'over'
                while i < len(lines) and lines[i].strip() == 'over':
                    i += 23
            if layered or name.startswith('bg '):
                self.items.append(('raw', '\n'.join(lines[start:i])))
            else:
                grid = [r.replace('.', ' ') for r in grid]
                self.items.append(('room', name, header, grid))

    def rooms(self):
        return dict((it[1], it) for it in self.items if it[0] == 'room')

    def grid(self, name):
        return self.rooms()[name][3]

    def set_grid(self, name, grid):
        for k, it in enumerate(self.items):
            if it[0] == 'room' and it[1] == name:
                self.items[k] = ('room', name, it[2], grid)

    def save(self):
        out = list(self.prelude)
        while out and out[-1] == '': out.pop()
        out.append('')
        for it in self.items:
            if it[0] == 'raw':
                out.append(it[1])
            else:
                out.append('room %s%s' % (it[1], (' ' + it[2]) if it[2] else ''))
                out.append('rows')
                for r, row in enumerate(it[3]):
                    if row.strip():
                        out.append('%02d|%s' % (r, row.rstrip()))
                out.append('endrows')
            out.append('')
        open(self.path, 'w').write('\n'.join(out))
