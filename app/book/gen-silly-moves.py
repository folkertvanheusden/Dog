#! /usr/bin/env python3

import chess
import chess.pgn
import chess.polyglot
import random
import sys


depth = 10
duration = 2
pgn_out = sys.argv[2]
program = '/usr/games/stockfish'
thread_count = 32
hash_amount = 256

def do_board(initial_moves, depth, duration):
    game = chess.pgn.Game()
    game.headers['Event'] = 'example'
    add_to = None
    b = chess.Board()
    for m in initial_moves:
        b.push(m)
    program_i = chess.engine.SimpleEngine.popen_uci(program)
    program_i.configure({"Threads": thread_count})
    program_i.configure({"Hash": hash_amount})
    result_moves = initial_moves
    for d in range(depth):
        if b.is_game_over():
            break
        result = program_i.play(b, chess.engine.Limit(time=duration))
        b.push(result.move)
        print(f'{result.move}, ', end='', flush=True)
        result_moves.append(result.move)
    game.add_line(result_moves)
    program_i.quit()
    print()
    return game


fh = open(pgn_out, 'w')
exporter = chess.pgn.FileExporter(fh)

n = 0
with chess.polyglot.open_reader(sys.argv[1]) as reader:  # use step 3
    moves1 = [move for move in chess.Board().legal_moves]
    for m1 in moves1:
        b = chess.Board()
        b.push(m1)
        moves2 = [move for move in b.legal_moves]
        for m2 in moves2:
            b.push(m2)
            book_moves = [move.move for move in reader.find_all(b)]
            b.pop()
            if len(book_moves) > 0:
                continue
            print(f'{m1} {m2}: ', end='', flush=True)
            pgn = do_board([m1, m2], depth, duration)
            pgn.accept(exporter)

fh.close()
