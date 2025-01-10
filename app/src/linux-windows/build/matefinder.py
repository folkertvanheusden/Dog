#! /usr/bin/python3

import chess
import chess.engine
from multiprocessing import Process, Queue
import time


num_procs = 12
epd_file = 'matetrack/matetrack.epd'
proc = './Dog'
time_limit = 0.5

def do_it(q_in, q_out):
    engine = chess.engine.SimpleEngine.popen_uci(proc)

    while True:
        msg = q_in.get()
        if msg == None:
            break

        b = chess.Board(msg)
        result = engine.play(b, chess.engine.Limit(time=time_limit), info=chess.engine.INFO_SCORE)
        try:
            score = result.info['score'].white().score()
            if abs(score) > 9800:
                q_out.put(1)
            else:
                q_out.put(0)
        except Exception as e:
            print(e)
            q_out.put(-1)

    q_out.put(None)

    engine.quit()

q_in = Queue()
q_out = Queue()
processes =  []
for i in range(num_procs):
    p = Process(target=do_it, args=(q_out, q_in,))
    p.start()
    processes.append(p)

n_queued = 0
for line in open(epd_file, 'r').readlines():
    line = line.rstrip('\n')
    # only using the fen here
    parts = line.split()
    q_out.put(' '.join(parts[0:4]))
    n_queued += 1

for i in range(num_procs):
    q_out.put(None)

ok = 0
nok = 0
errors = 0
finished = 0

while True:
    msg = q_in.get()
    if msg == None:
        finished += 1
        if finished == num_procs:
            break
    elif msg == 1:
        ok += 1
    elif msg == 0:
        nok += 1
    else:
        errors += 1

total_n = ok + nok
print(f'% ok: {ok * 100 / total_n}, total processed: {total_n}, total queued: {n_queued}, errors: {errors}')

for p in processes:
    p.join()
