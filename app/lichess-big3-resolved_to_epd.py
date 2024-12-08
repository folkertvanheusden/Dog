#! /usr/bin/python3

import sys


fh = open('lichess-big3-resolved.book', 'r')
while True:
    line = fh.readline().strip()
    if line == '':
        break

    bracket1 = line.find('[')
    if bracket1 == -1:
        continue

    bracket2 = line.find(']', bracket1)
    if bracket2 == -1:
        continue

    value = line[bracket1 + 1: bracket2]

    if value == '0.0':
        c9 = '0-1'
    elif value == '1.0':
        c9 = '1-0'
    elif value == '0.5':
        c9 = '1/2-1/2'
    else:
        print(line, file=sys.stderr)

    line = line[0:bracket1] + 'c9 "' + c9 + '"'
    print(line)
