#! /usr/bin/python3

fh_members = open('eval_par_psq.h', 'w')

fh_get     = open('eval_par_get.h', 'w')

fh_set     = open('eval_par_set.h', 'w')

fh_tune    = open('tune_psq.h', 'w')

targets = [ 'PawnPSTMG', 'KnightPSTMG', 'BishopPSTMG', 'RookPSTMG', 'QueenPSTMG', 'KingPSTMG', 'PawnPSTEG', 'KnightPSTEG', 'BishopPSTEG', 'RookPSTEG', 'QueenPSTEG', 'KingPSTEG' ]

for target in targets:
    for i in range(0, 64):
        name = f'tune_{target}_{i:02d}'

        fh_members.write('\tlibchess::TunableParameter %s {"%s", %s };\n' % (name, name, name.upper()))

        fh_tune.write(f'#define {name.upper()} 0\n')

        fh_set.write(f'\telse if (name == {name}.name())\n')
        fh_set.write(f'\t\t{name}.set_value(value);\n')

        fh_get.write(f'\tlist.push_back({name});')

    fh_members.write('\n')
