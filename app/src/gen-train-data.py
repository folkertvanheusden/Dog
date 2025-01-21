#! /usr/bin/python3

# written by folkert van heusden
# mit license

import chess
import chess.engine
import getopt
import os
import random
import sys
import time

    
proc = None
duration = 0.1
file = None

def help():
    print('-e x  chess-program (UCI) to use')
    print('-d x  how long to think per move')
    print('-f x  file to write to, pid will be added to the filename')
    print('-h    this help')

try:
    opts, args = getopt.getopt(sys.argv[1:], 'e:d:f:h')

except getopt.GetoptError as err:
    print(err)
    help()
    sys.exit(2)

for o, a in opts:
    if o == '-e':
        proc = a
    elif o == '-d':
        duration = float(a)
    elif o == '-f':
        file = a
    elif o == '-h':
        help()
        sys.exit(0)

if proc == None or file == None:
    help()
    sys.exit(1)

file = f'{file}.{os.getpid()}'

engine1 = chess.engine.SimpleEngine.popen_uci(proc)
engine2 = chess.engine.SimpleEngine.popen_uci(proc)

while True:
    b = chess.Board()

    for i in range(random.randint(1, 32)):
        moves = [m for m in b.legal_moves]
        b.push(random.choice(moves))
        if b.outcome() != None:
            break

    fens = []
    first = True
    while b.outcome() == None:
        if first:
            first = False
        else:
            fen = b.fen()
            fens.append(fen)
            print(fen)
        if b.turn == chess.WHITE:
            result = engine1.play(b, chess.engine.Limit(time=duration), info=chess.engine.INFO_ALL)
            b.push(result.move)
        else:
            result = engine2.play(b, chess.engine.Limit(time=duration), info=chess.engine.INFO_ALL)
            b.push(result.move)
    if len(fens) > 0:
        result = b.outcome().result()
        str_ = ''
        for fen in fens:
            str_ += f'{fen}|0|{result}\n'
        with open(file, 'a+') as fh:
            fh.write(str_)

engine2.quit()
engine1.quit()
