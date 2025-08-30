#! /usr/bin/env python3

# written by folkert van heusden
# mit license

import chess
import chess.engine
import getopt
import json
import multiprocessing
import os
import random
import socket
import sys
import time

#import logging
#logging.basicConfig(level=logging.DEBUG)

host = 'dog.vanheusden.com'
port = 31251
proc = None
node_count = 10000
nth = os.sysconf('SC_NPROCESSORS_ONLN')

def help():
    print(f'-H x  host to send data to (default: {host})')
    print('-e x  chess-program (UCI) to use')
    print(f'-d x  how many nodes to visit per move (default: {node_count})')
    print(f'-t x  # processes (default: {nth})')
    print('-h    this help')

try:
    opts, args = getopt.getopt(sys.argv[1:], 'e:d:t:h')

except getopt.GetoptError as err:
    print(err)
    help()
    sys.exit(2)

for o, a in opts:
    if o == '-e':
        proc = a
    elif o == '-d':
        node_count = float(a)
    elif o == '-h':
        host = a
    elif o == '-t':
        nth = int(a)
    elif o == '-h':
        help()
        sys.exit(0)

if proc == None:
    help()
    sys.exit(1)

def gen_board():
    b = chess.Board()

    for i in range(random.choice((8, 9))):
        moves = [m for m in b.legal_moves]
        b.push(random.choice(moves))
        if b.outcome() != None:
            break

    return b

def play(b, engine1, engine2, q):
    fens = []
    first = True
    was_capture = False
    while b.outcome() == None:
        store_fen = None
        if first:
            first = False  # was random
        elif b.is_check() == False and was_capture == False:
            store_fen = b.fen()

        # count number of pieces. if 4 or less: stop.
        piece_count = chess.popcount(b.occupied)
        if piece_count < 4:
            q.put(('early_abort', 1))
            break

        if b.turn == chess.WHITE:
            result = engine1.play(b, chess.engine.Limit(nodes=node_count), info=chess.engine.INFO_BASIC | chess.engine.INFO_SCORE)
            was_capture = b.is_capture(result.move)
            b.push(result.move)
        else:
            result = engine2.play(b, chess.engine.Limit(nodes=node_count), info=chess.engine.INFO_BASIC | chess.engine.INFO_SCORE)
            was_capture = b.is_capture(result.move)
            b.push(result.move)

        if store_fen != None and 'score' in result.info and result.info['score'].is_mate() == False and 'nodes' in result.info:
            score = result.info['score'].white().score()
            cur_node_count = result.info['nodes']
            fens.append({ 'score': score, 'node-count': cur_node_count, 'fen': store_fen })

    return fens

def process(proc, q):
    while True:
        try:
            engine1 = engine2 = None
            engine1 = chess.engine.SimpleEngine.popen_uci(proc)
            engine2 = chess.engine.SimpleEngine.popen_uci(proc)

            name1 = engine1.id['name']
            name2 = engine2.id['name']

            print(name1, name2)

            s = None

            while True:
                b = gen_board()
                fens = play(b, engine1, engine2, q)
                if len(fens) > 0 and b.outcome() != None:
                    result = b.outcome().result()

                    q.put(('count', len(fens)))
                    q.put(('gcount', 1))

                    if host != None:
                        j = { 'name1': name1, 'name2': name2, 'host': socket.gethostname() }
                        j['data'] = {'result': result, 'fens': fens }

                        while True:
                            try:
                                if s == None:
                                    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                                    s.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, True)
                                    s.setsockopt(socket.SOL_TCP, 23, 5)  # TCP fastopen (client as well?!)
                                    s.connect((host, port))

                                s.send((json.dumps(j) + '\n').encode('ascii'))
                                break
                            except Exception as e:
                                print(f'Socket error: {e}')
                                s.close()
                                s = None
                                time.sleep(0.5)

        except Exception as e:
            print(f'failure: {e}')

        finally:
            if engine2:
                engine2.quit()
            if engine1:
                engine1.quit()

    print('PROCESS TERMINATING')

q = multiprocessing.Queue()

processes = []
for i in range(0, nth):
    t = multiprocessing.Process(target=process, args=(proc,q,))
    processes.append(t)
    t.start()

count = 0
gcount = 0
early_abort = 0

start = time.time()
while True:
    item = q.get()
    if item[0] == 'count':
        count += item[1]
    elif item[0] == 'gcount':
        gcount += item[1]
    elif item[0] == 'early_abort':
        early_abort += item[1]
    else:
        print('Internal error', item[0])
        continue
    t_diff = time.time() - start
    print(f'{time.ctime()}, fen/s: {count / t_diff:.2f}, total fens: {count}, games/minute: {gcount * 60 / t_diff:.2f}, early aborts: {early_abort}')

for t in processes:
    t.join()
