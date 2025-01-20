#! /usr/bin/python3

# written by folkert van heusden
# mit license

import chess
import chess.engine
import getopt
import sqlite3
import sys
import time

    
fen_file = None
db_file = None
proc = None
node_limit = 5000

def help():
    print('-f x  fen file to process (skip to only generate with existing data in the database)')
    print('-s x  database to store results & fens in')
    print('-e x  chess-program (UCI) to use')
    print(f'-d x  node limit per move (default: {node_limit})')
    print('-h    this help')

try:
    opts, args = getopt.getopt(sys.argv[1:], 'f:s:e:d:h')

except getopt.GetoptError as err:
    print(err)
    help()
    sys.exit(2)

for o, a in opts:
    if o == '-f':
        fen_file = a
    elif o == '-s':
        db_file = a
    elif o == '-e':
        proc = a
    elif o == '-d':
        node_limit = int(a)
    elif o == '-h':
        help()
        sys.exit(0)

if db_file == None or proc == None:
    help()
    sys.exit(1)

def init_db(db_file):
    con = sqlite3.connect(db_file)

    cur = con.cursor()
    cur.execute('PRAGMA journal_mode=wal')
    cur.execute('PRAGMA encoding="UTF-8"')
    cur.close()

    try:
        cur = con.cursor()

        query = 'CREATE TABLE fens(fen TEXT NOT NULL, result TEXT NOT NULL, score INTEGER, nodes INTEGER, set_by TEXT, PRIMARY KEY(fen))'
        cur.execute(query)
        cur.close()

        con.commit()

    except sqlite3.OperationalError as oe:
        # table already exists (hopefully)
        pass

    return con

con = init_db(db_file)

if fen_file != None:
    err = 0
    ok = 0
    cur = con.cursor()
    for line in open(fen_file, 'r').readlines():
        line = line.rstrip('\n')
        parts = line.split()
        c9 = parts.index('c9')
        if c9 == '1-0':
            result = '1'
        elif c9 == '0-1':
            result = '0'
        else:
            result = '0.5'
        fen = ' '.join(parts[0:4])

        try:
            cur.execute('INSERT INTO fens(fen, result) VALUES(?, ?)', (fen, result))
            ok += 1
        except sqlite3.IntegrityError as e:
            print(ok, err, line, fen, e)
            err += 1

    cur.close()
    con.commit()

    print(f'# ok: {ok}, errors: {err}')

engine = chess.engine.SimpleEngine.popen_uci(proc)
proc_name = engine.id['name']

t = time.time()

while True:
    cur = con.cursor()
    cur.execute('SELECT fen FROM fens WHERE score IS NULL ORDER BY RANDOM() LIMIT 1')
    fen = cur.fetchone()
    cur.close()
    if fen == None:
        break
    fen = fen[0]

    try:
        print(fen)
        b = chess.Board(fen)
        result = engine.play(b, chess.engine.Limit(nodes=node_limit), info=chess.engine.INFO_ALL)
        score = result.info['score'].white().score()
        node_count = result.info['nodes']
        print(f'\t{node_count} -> {score} ({proc_name})')

        cur = con.cursor()
        cur.execute('UPDATE fens SET set_by=?, score=?, nodes=? WHERE fen=?', (proc_name, score, node_count, fen))
        cur.close()
        print(fen, result)
    except Exception as e:
        print(result.info)
        print(e)
        break

    now = time.time()
    if now - t >= 30:
        con.commit()
        t = now

con.commit()
con.close()

engine.quit()
