#! /usr/bin/python3

fh = open('tune.dat', 'r')
for line in fh.readlines():
    line = line.rstrip()
    parts = line.split('=')
    if len(parts) != 2:
        print(line, file=sys.stderr)
        continue
    print(f'#define {parts[0].upper()} {parts[1]}')
