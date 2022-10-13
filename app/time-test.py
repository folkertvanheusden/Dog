#! /usr/bin/python3

# experiments with timekeeping

import math
import sys

def eprint(*args, **kwargs):
    print(*args, file=sys.stderr, **kwargs)

start_ms = 12000
time_inc = 200

ms_hgm   = start_ms
ms_fvh   = start_ms

sum_hgm  = 0
sum_fvh  = 0

sum_theory = 50 * time_inc + start_ms

print('$Mydata << EOD')

for cur_n_moves in range(1, 50):
    think_time_hgm = (ms_hgm + (cur_n_moves - 1) * time_inc) / (cur_n_moves + 7)
    if think_time_hgm > ms_hgm / 15:
        think_time_hgm = ms_hgm / 15
    ms_hgm        -= think_time_hgm
    sum_hgm       += think_time_hgm

    think_time_fvh = (ms_fvh + (cur_n_moves - 1) * time_inc) / (cur_n_moves + 7 * math.sqrt(cur_n_moves - math.log(cur_n_moves)))
    if think_time_fvh > ms_fvh / (15 - math.log(cur_n_moves)):
        think_time_fvh = ms_fvh / (15 - math.log(cur_n_moves))
    ms_fvh        -= think_time_fvh
    sum_fvh       += think_time_fvh

    print(cur_n_moves, think_time_hgm, think_time_fvh, think_time_fvh - think_time_hgm)

print('EOD')

eprint('---')
eprint(f'theory: {sum_theory}, hgm: {sum_hgm}, fvh: {sum_fvh}')

print('set term png size 1920,1080 small')
print('set autoscale')
print('set title "time control"')
print('set boxwidth 0.4')
print('set grid')
print('set xlabel "move nr"')
print('set ylabel "time (ms)"')
print('set y2label "time diff (ms)"')
print('plot $Mydata using 1:2 axes x1y1 with lines title "hgm" \\')
print(',    $Mydata using 1:3 axes x1y1 with lines title "fvh" \\')
print(',    $Mydata using 1:4 axes x1y2 with lines title "dt"')
