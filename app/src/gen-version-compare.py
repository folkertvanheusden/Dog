#! /usr/bin/env python3

# written by folkert van heusden
# mit license

import chess
import chess.engine
import getopt
import json
import os
import random
import socket
import sys
import threading
import time

proc = None
aproc = None
time_ = 0.2
count = 50
file = None
nth = 1

def help():
    print('-e x  chess-program (UCI) to test')
    print('-a x  chess-program (UCI) to use as arbiter')
    print(f'-T x  how much thinktime (seconds, default: {time_})')
    print(f'-c x  how many games to play (default: {count})')
    print('-f x  file to write to')
    print('-t x  # threads')
    print('-h    this help')

try:
    opts, args = getopt.getopt(sys.argv[1:], 'a:e:T:f:t:C:h')

except getopt.GetoptError as err:
    print(err)
    help()
    sys.exit(2)

for o, a in opts:
    if o == '-e':
        proc = a
    elif o == '-a':
        aproc = a
    elif o == '-c':
        count = int(a)
    elif o == '-T':
        time_ = float(a)
    elif o == '-f':
        file = a
    elif o == '-t':
        nth = int(a)
    elif o == '-h':
        help()
        sys.exit(0)

if proc == None or file == None:
    help()
    sys.exit(1)

lock = threading.Lock()
total_n_played = 0
total_n_mistakes = 0
total_n_moves = 0

def thread(proc):
    global lock
    global total_n_mistakes
    global total_n_moves
    global total_n_played

    end = False
    while end == False:
        n_moves = 0
        n_mistakes = 0

        engineA = chess.engine.SimpleEngine.popen_uci(aproc)
        engine1 = chess.engine.SimpleEngine.popen_uci(proc)
        engine2 = chess.engine.SimpleEngine.popen_uci(proc)

        b = chess.Board()

        for i in range(random.choice((8, 9))):
            moves = [m for m in b.legal_moves]
            b.push(random.choice(moves))
            if b.outcome() != None:
                break

        while b.outcome() == None:
            fen = b.fen()

            aresult = engineA.play(b, chess.engine.Limit(time=time_), info=chess.engine.INFO_BASIC | chess.engine.INFO_SCORE)
            if b.turn == chess.WHITE:
                result = engine1.play(b, chess.engine.Limit(time=time_), info=chess.engine.INFO_BASIC | chess.engine.INFO_SCORE)
                b.push(result.move)
            else:
                result = engine2.play(b, chess.engine.Limit(time=time_), info=chess.engine.INFO_BASIC | chess.engine.INFO_SCORE)
                b.push(result.move)

            n_moves += 1
            if aresult.move != result.move:
                n_mistakes += 1

                with open(file, 'a+') as fh:
                    fh.write(f'{fen}|{result.move}|{aresult.move}\n')

        engine2.quit()
        engine1.quit()
        engineA.quit()

        lock.acquire()
        total_n_mistakes += n_mistakes
        total_n_moves += n_moves
        total_n_played += 1
        end = total_n_played >= count
        print(f'Games played: {total_n_played}, average error percentage: {total_n_mistakes * 100 / total_n_moves:2f}')
        lock.release()

threads = []
for i in range(0, nth):
    t = threading.Thread(target=thread, args=(proc,))
    threads.append(t)
    t.start()

for t in threads:
    t.join()

with open(file, 'a+') as fh:
    fh.write(f'Games played: {total_n_played}, average error percentage: {total_n_mistakes * 100 / total_n_moves:2f}\n')
