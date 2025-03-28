#! /usr/bin/env python3

import chess
import chess.pgn
import chess.polyglot
import random


depth = 7
duration = 10
pgn_out = 'test.pgn'
program = '/usr/games/stockfish'
thread_count = 12
hash_amount = 256

def do_board(initial_move, depth, duration):
    game = chess.pgn.Game()
    game.headers['Event'] = 'example'
    add_to = None
    b = chess.Board()
    b.push(initial_move)
    program_i = chess.engine.SimpleEngine.popen_uci(program)
    program_i.configure({"Threads": thread_count})
    program_i.configure({"Hash": hash_amount})
    result_moves = [ initial_move ]
    for d in range(depth):
        if b.is_game_over():
            break
        result = program_i.play(b, chess.engine.Limit(time=duration))
        b.push(result.move)
        print(f'{result.move}, ', end='')
        result_moves.append(result.move)
    game.add_line(result_moves)
    program_i.quit()
    print()
    return game


fh = open(pgn_out, 'w')
exporter = chess.pgn.FileExporter(fh)

n = 0
with chess.polyglot.open_reader('../data/dog-book.bin') as reader:  # use step 3
    moves = [move for move in chess.Board().legal_moves]
    for m in moves:
        b = chess.Board()
        b.push(m)
        book_moves = [move.move for move in reader.find_all(b)]
        if len(book_moves) > 0:
            continue
        print(f'{m}: ', end='')
        pgn = do_board(m, depth, duration)
        pgn.accept(exporter)

fh.close()
