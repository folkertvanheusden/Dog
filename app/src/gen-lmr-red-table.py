#! /usr/bin/python3

import math


n_lmr_depth=64
n_lmr_moves=64
lmr_mul= 0.5
lmr_base = 1.0
print(f'#define N_LMR_DEPTH {n_lmr_depth}')
print(f'#define N_LMR_MOVES {n_lmr_moves}')
print('constexpr const uint8_t lmr_reductions[N_LMR_DEPTH][N_LMR_MOVES] = {')
for depth in range(n_lmr_depth):
    print('\t{', end='')
    if depth == 0:
        print(' 0, ' * n_lmr_moves, end='')
    else:
        for n_played in range(n_lmr_moves):
            print(f' {int(math.log(depth) * math.log(n_played + 1) * lmr_mul + lmr_base)}, ', end='')
    print('},')
print('};\n')
