#! /usr/bin/python3

lines = [line.rstrip('\n') for line in open('tune.dat', 'r').readlines() if line[0] != '#']

out = [ '#pragma once' ]

for line in lines:
    parts = line.split('=')

    out.append(f'#define {parts[0].upper()}\t{parts[1]}')

out.append('#define TUNE_PP_SCORES_EG_0 0')
out.append('#define TUNE_PP_SCORES_EG_7 0')
out.append('#define TUNE_PP_SCORES_MG_0 0')
out.append('#define TUNE_PP_SCORES_MG_7 0')

with open('src/tune.h', 'w') as fh:
    for line in out:
        fh.write(f'{line}\n')
