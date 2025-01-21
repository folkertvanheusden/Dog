#! /usr/bin/python3

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


host = 'dog.vanheusden.com'
port = 31250
proc = None
node_count = 5000
file = None
nth = 1

def help():
    print(f'-H x  host to send data to (default: {host})')
    print('-e x  chess-program (UCI) to use')
    print(f'-d x  how many nodes to visit per move (default: {node_count})')
    print('-f x  file to write to, pid will be added to the filename')
    print('-t x  # threads')
    print('-h    this help')

try:
    opts, args = getopt.getopt(sys.argv[1:], 'e:d:f:t:h')

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

file = f'{file}.{os.getpid()}.{socket.gethostname()}'

lock = threading.Lock()

count = 0
gcount = 0

def thread(proc):
    global count
    global gcount
    global lock

    engine1 = chess.engine.SimpleEngine.popen_uci(proc)
    engine2 = chess.engine.SimpleEngine.popen_uci(proc)

    name1 = engine1.id['name']
    name2 = engine2.id['name']

    print(name1, name2)

    while True:
        b = chess.Board()

        for i in range(random.randint(1, 32)):
            moves = [m for m in b.legal_moves]
            b.push(random.choice(moves))
            if b.outcome() != None:
                break

        fens = []
        first = True
        was_capture = False
        while b.outcome() == None:
            if first:
                first = False
            elif b.is_check() == False and was_capture == False:
                fen = b.fen()
                fens.append(fen)
                #print(fen)
            if b.turn == chess.WHITE:
                result = engine1.play(b, chess.engine.Limit(nodes=node_count), info=chess.engine.INFO_ALL)
                was_capture = b.is_capture(result.move)
                b.push(result.move)
            else:
                result = engine2.play(b, chess.engine.Limit(nodes=node_count), info=chess.engine.INFO_ALL)
                was_capture = b.is_capture(result.move)
                b.push(result.move)
        if len(fens) > 0:
            result = b.outcome().result()
            str_ = ''
            for fen in fens:
                str_ += f'{fen}|0|{result}\n'
            lock.acquire()
            with open(file, 'a+') as fh:
                fh.write(str_)
            count += len(fens)
            gcount += 1
            lock.release()

            if host != None:
                j = { 'name1': name1, 'name2': name2, 'node-count': node_count, 'host': socket.gethostname() }
                j['data'] = {'result': result, 'fens': fens }

                while True:
                    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                    try:
                        s.connect((host, port))
                        s.send(json.dumps(j).encode('ascii'))
                        s.close()
                        break
                    except Exception as e:
                        print(f'failure: {e}')
                        time.sleep(1)
                    s.close()

    engine2.quit()
    engine1.quit()

threads = []
for i in range(0, nth):
    t = threading.Thread(target=thread, args=(proc,))
    threads.append(t)
    t.start()

start = time.time()
while True:
    time.sleep(1)
    lock.acquire()
    c = count
    lock.release()
    t_diff = time.time() - start
    print(c / t_diff, c, gcount * 60 / t_diff)

for t in threads:
    t.join()
