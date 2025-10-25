#! /usr/bin/env python3

import json
import socket
import sqlite3
import queue
import threading
import time


host = '0.0.0.0'
port = 31251
db_file = '/home/folkert/dog-fens/Dog-nnue-fens2.db'

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

    while True:
        try:
            (j, addr) = q.get()

            cur = con.cursor()
            for fen in j['data']['fens']:
                cur.execute('INSERT INTO fens(fen, result, nodes, name1, name2, host, addr, port, score) VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?)',
                            (fen['fen'], j['data']['result'], fen['node-count'], j['name1'], j['name2'], j['host'], str(addr[0]), addr[1], fen['score']))
            cur.close()

            gcount += 1
            count += len(j['data']['fens'])
            n_commit += 1

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

        j = json.loads(data)

        q.put((j, addr))

    except Exception as e:
        print(e)

    c.close()

q = queue.Queue()

th = threading.Thread(target=pusher, args=(q,))
th.daemon = True
th.start()

while True:
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind((host, port))
    s.listen()

    while True:
        c, addr = s.accept()

        th = threading.Thread(target=handle_client, args=(c, addr, q))
        th.daemon = True
        th.start()

        now = time.time()
        t_diff = now - start

        if commit_total_n:
            print(f'{time.ctime()}, {addr}, total games {gcount}, g/s {gcount / t_diff:.2f}, fens {count}, f/s: {count / t_diff:.2f}, per commit: {commit_total / commit_total_n:.2f}')
        else:
            print(f'{time.ctime()}, {addr}, total games {gcount}, g/s {gcount / t_diff:.2f}, fens {count}, f/s: {count / t_diff:.2f}')
