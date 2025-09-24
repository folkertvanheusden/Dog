#! /usr/bin/python3

import chess
import chess.pgn
import sys


pgn = open(sys.argv[1])
n_games = 0
players = dict()
m_counts = dict()
nm_counts = dict()

time_control = None

while True:
    game = chess.pgn.read_game(pgn)
    if game == None:
        break
    if time_control == None:
        time_control = game.headers['TimeControl']
    b = chess.Board(game.headers['FEN']) if 'FEN' in game.headers else chess.Board()
    cw = game.headers['White']
    if not cw in players:
        players[cw] = []
        nm_counts[cw] = 0
        m_counts[cw] = 0
    cb = game.headers['Black']
    if not cb in players:
        players[cb] = []
        nm_counts[cb] = 0
        m_counts[cb] = 0
    p = [None, None]
    p[chess.WHITE] = cw
    p[chess.BLACK] = cb
    for move in game.mainline():
        parts = move.comment.split(' ')
        move_t = float(parts[1].rstrip('s,'))
        while len(players[p[b.turn]]) <= b.fullmove_number:
            players[p[b.turn]].append([0, 0])
        players[p[b.turn]][b.fullmove_number][0] += move_t
        players[p[b.turn]][b.fullmove_number][1] += 1
        b.push(move.move)
        m_counts[p[b.turn]] += 1
    nm_counts[cw] += 1
    nm_counts[cb] += 1
    n_games += 1

print('/usr/bin/gnuplot << EOF > output.png')
print('set term png size 1920,1080')
print('set autoscale')
print(f'set title "average time used per move, over {n_games} games, {time_control} (tc)"')
print('set boxwidth 0.4')
print('set grid')
print('set xlabel "move nr"')
print('set ylabel "avg time (seconds)"')

first = True
for p in players:
    fname = p + '.dat'
    fh = open(fname, 'w')
    nr = 1
    for pair in players[p]:
        fh.write(f'{nr} {pair[0] / pair[1] if pair[1] else 0}\n')
        nr += 1
    fh.close()
    if first:
        first = False
        print(f'plot "{fname}" using 1:2 axes x1y1 with lines title "{p} ({nm_counts[p]} games)" \\')
    else:
        print(f',    "{fname}" using 1:2 axes x1y1 with lines title "{p} ({nm_counts[p]} games)" \\')
print('')
print('EOF')
