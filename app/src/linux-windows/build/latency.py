#! /usr/bin/python3

# by folkert van heusden, released under mit license

from datetime import datetime
import statistics
import sys

if len(sys.argv) != 2:
    print('Chess program name missing')
    sys.exit(1)

cmds = ('uci', 'isready', 'go')

diffs = dict()
lts = dict()

for line in sys.stdin.readlines():
    parts = line.split()
    if parts[0] != '[Engine]':
        continue
    if parts[3] != sys.argv[1]:
        continue

    # [Engine] [18:24:03.185898] <140060611577536> Dog-6d39e1d-master-LC7569dd6 <--- uci

    ts = datetime.strptime(parts[1][1:-1], '%H:%M:%S.%f')
    name = parts[2] + '|' + parts[3]

    try:
        arrow = parts.index('--->')
        from_engine = True
    except ValueError as v:
        arrow = parts.index('<---')
        from_engine = False

    cmd = parts[arrow + 1]

    if from_engine:
        if name in lts:
            referring_cmd = lts[name][0]
            if not referring_cmd in diffs:
                diffs[referring_cmd] = []
            diffs[referring_cmd].append((ts - lts[name][1]).total_seconds())
            del lts[name]
    else:
        lts[name] = (cmd, ts)

for cmd in diffs:
    if len(diffs[cmd]) == 0:
        continue
    print(f' -=]* {cmd } *[=-')
    print(f'#      : {len(diffs[cmd])}')
    print(f'average: {statistics.mean(diffs[cmd]) * 1000000:.0f} us')
    print(f'median : {statistics.median(diffs[cmd]) * 1000000:.0f} us')
    print(f'stdev  : {statistics.stdev(diffs[cmd]) * 1000000:.0f} us')
    print(f'min    : {min(diffs[cmd]) * 1000000:.0f} us')
    print(f'max    : {max(diffs[cmd]) * 1000000:.0f} us')

    import termplotlib as tpl
    import numpy as np

    counts, bin_edges = np.histogram(diffs[cmd], bins=25)
    fig = tpl.figure()
    fig.hist(counts, bin_edges, grid=[120, 25], force_ascii=False, orientation='horizontal')
    fig.show()

    print()
