import re
import sys

with open(sys.argv[1]) as f:
    for line in f:
        m = re.match(r'^(\w+).*?(A:\w+ X:\w+ Y:\w+ P:\w+ SP:\w+).*?CYC:(\d+)', line)
        if m:
            print(f"{m.group(1)}  {m.group(2)} CYC:{m.group(3)}")
