#! /usr/bin/env python3

import json
import socket
import sqlite3
import queue
import threading
import time


host = '0.0.0.0'
port = 31250
db_file = '/home/folkert/dog-fens/storage/Dog-nnue-fens1b.db'

def init_db(db_file):
    con = sqlite3.connect(db_file)

    cur = con.cursor()
    cur.execute('PRAGMA journal_mode=wal')
    cur.execute('PRAGMA encoding="UTF-8"')
    cur.close()

    try:
        cur = con.cursor()

        query = 'CREATE TABLE fens(timestamp DATETIME DEFAULT CURRENT_TIMESTAMP, fen TEXT NOT NULL, result TEXT NOT NULL, score INTEGER, nodes INTEGER, name1 TEXT, name2 TEXT, host TEXT, addr TEXT, port int)'
        cur.execute(query)
        cur.close()

        con.commit()

    except sqlite3.OperationalError as oe:
        # table already exists (hopefully)
        pass

    return con

count = 0
gcount = 0

start = time.time()
n_commit = 0
commit_total = 0
commit_total_n = 0

def pusher(q):
    global gcount
    global n_commit
    global count
    global commit_total
    global commit_total_n

    con = init_db(db_file)
    prev_commit = time.time()

    hosts = dict()
    interval_fen_count = 0

    while True:
        try:
            (j, addr) = q.get()
            h = str(addr[0])

            cur = con.cursor()
            for fen in j['data']['fens']:
                cur.execute('INSERT INTO fens(fen, result, nodes, name1, name2, host, addr, port, score) VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?)',
                            (fen['fen'], j['data']['result'], fen['node-count'], j['name1'], j['name2'], j['host'], h, addr[1], fen['score']))
            cur.close()

            gcount += 1
            n_fens = len(j['data']['fens'])
            count += n_fens
            interval_fen_count += n_fens
            n_commit += 1

            if h in hosts:
                hosts[h] += n_fens
            else:
                hosts[h] = n_fens

        except Exception as e:
            print(e)

        now = time.time()
        if now - prev_commit >= 60:
            print(f'commit {n_commit}')
            commit_total += n_commit
            commit_total_n += 1
            n_commit = 0
            prev_commit = now
            con.commit()

            t_diff = now - start
            if commit_total_n:
                print(f'{time.ctime()}, total games {gcount}, g/s {gcount / t_diff:.2f}, fens {count}, f/s: {count / t_diff:.2f}, per commit: {commit_total / commit_total_n:.2f}, interval: {interval_fen_count}')
            else:
                print(f'{time.ctime()}, total games {gcount}, g/s {gcount / t_diff:.2f}, fens {count}, f/s: {count / t_diff:.2f}')
            interval_fen_count = 0

            for h in hosts:
                print(f'\t{h}: {hosts[h] / 60:.2f} fens/s')
            hosts = dict()

    con.close()

def handle_client(c, addr, q):
    global gcount
    global count
    global n_commit

    try:
        data = ''
        while True:
            in_ = c.recv(4096)
            if not in_:
                break
            data += in_.decode()

            lf = data.find('\n')
            if lf != -1:
                j = json.loads(data[0:lf])
                q.put((j, addr))
                data = data[lf + 1:]

    except Exception as e:
        print(e)

    c.close()

print(time.ctime())

q = queue.Queue()

th = threading.Thread(target=pusher, args=(q,))
th.daemon = True
th.start()

while True:
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    TCP_FASTOPEN = 23
    s.setsockopt(socket.SOL_TCP, TCP_FASTOPEN, 5)
    s.bind((host, port))
    s.listen(socket.SOMAXCONN)

    while True:
        c, addr = s.accept()

        th = threading.Thread(target=handle_client, args=(c, addr, q))
        th.daemon = True
        th.start()
