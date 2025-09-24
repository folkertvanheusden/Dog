#! /usr/bin/python3

import chess
import chess.pgn
import sys


pgn = open(sys.argv[1])
n_games = 0
players = dict()
m_counts = dict()
nm_counts = dict()
latencies = dict()

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
        latencies[cw] = dict()
    cb = game.headers['Black']
    if not cb in players:
        players[cb] = []
        nm_counts[cb] = 0
        m_counts[cb] = 0
        latencies[cb] = dict()
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
        latency = parts[3][8:].rstrip('s,')
        if not latency in latencies[p[b.turn]]:
            latencies[p[b.turn]][latency] = 0
        latencies[p[b.turn]][latency] += 1
    nm_counts[cw] += 1
    nm_counts[cb] += 1
    n_games += 1

fhs = open('tijd-test1.sh', 'w')
fhs.write('/usr/bin/gnuplot << EOF > output.png\n')
fhs.write('set term png size 1920,1080\n')
fhs.write('set autoscale\n')
fhs.write(f'set title "average time used per move, over {n_games} games, {time_control} (tc)"\n')
fhs.write('set boxwidth 0.4\n')
fhs.write('set grid\n')
fhs.write('set xlabel "move nr"\n')
fhs.write('set ylabel "avg time (seconds)"\n')

first = True
for p in players:
    print(latencies[p])
    fname = p + '.dat'
    fhd = open(fname, 'w')
    nr = 1
    for pair in players[p]:
        fhd.write(f'{nr} {pair[0] / pair[1] if pair[1] else 0}\n')
        nr += 1
    fhd.close()
    if first:
        first = False
        fhs.write(f'plot "{fname}" using 1:2 axes x1y1 with lines title "{p} ({nm_counts[p]} games)" \\\n')
    else:
        fhs.write(f',    "{fname}" using 1:2 axes x1y1 with lines title "{p} ({nm_counts[p]} games)" \\\n')
fhs.write('')
fhs.write('EOF\n')
fhs.close()
