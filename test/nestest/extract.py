import re
import sys

with open(sys.argv[1]) as f:
    for line in f:
        m = re.match(r'^(\w+).*?(A:\w+ X:\w+ Y:\w+ P:\w+ SP:\w+)\s+PPU:\s*(\d+),\s*(\d+)\s+CYC:(\d+)', line)
        if m:
            print(f"{m.group(1)}  {m.group(2)} PPU:{int(m.group(3)):3d},{int(m.group(4)):3d} CYC:{m.group(5)}")
