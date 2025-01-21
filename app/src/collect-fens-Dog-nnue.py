#! /usr/bin/python3

import json
import socket
import sqlite3
import time


host = '0.0.0.0'
port = 31250
db_file = '/home/folkert/bin/data/Dog-nnue-fens.db'

def init_db(db_file):
    con = sqlite3.connect(db_file)

    cur = con.cursor()
    cur.execute('PRAGMA journal_mode=wal')
    cur.execute('PRAGMA encoding="UTF-8"')
    cur.close()

    try:
        cur = con.cursor()

        query = 'CREATE TABLE fens(timestamp DATETIME DEFAULT CURRENT_TIMESTAMP, fen TEXT NOT NULL, result TEXT NOT NULL, score INTEGER, nodes INTEGER, name1 TEXT, name2 TEXT, host TEXT, addr TEXT)'
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

while True:
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind((host, port))
    s.listen()

    while True:
        c, addr = s.accept()

        con = init_db(db_file)

        try:
            data = ''
            while True:
                in_ = c.recv(4096)
                if not in_:
                    break
                data += in_.decode()
            j = json.loads(data)

            cur = con.cursor()
            for fen in j['data']['fens']:
                cur.execute('INSERT INTO fens(fen, result, nodes, name1, name2, host, addr) VALUES(?, ?, ?, ?, ?, ?, ?)',
                            (fen, j['data']['result'], j['node-count'], j['name1'], j['name2'], j['host'], str(addr)))
            cur.close()

            gcount += 1
            count += len(j['data']['fens'])

        except Exception as e:
            print(e)

        con.commit()
        c.close()
        con.close()

        t_diff = time.time() - start

        print(addr, gcount, gcount / t_diff, count, count / t_diff)
