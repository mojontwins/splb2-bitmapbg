# roomed -export and tools/conv.py must produce the same rooms_data/rooms_off
import re, sys
def grab(s):
    m = re.search(r'unsigned char rooms_data \[\] = \{(.*?)\};', s, re.S)
    nums = [int(x) for x in re.findall(r'\d+', re.sub(r'/\*.*?\*/', '', m.group(1)))]
    o = re.search(r'rooms_off \[\] = \{(.*?)\}', s).group(1)
    return nums, [int(x) for x in re.findall(r'\d+', o)]
if grab(open(sys.argv[1]).read()) != grab(open(sys.argv[2]).read()):
    print('FAIL: roomed -export differs from tools/conv.py')
    sys.exit(1)
print('export matches conv.py: ok')
