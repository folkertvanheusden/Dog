#! /usr/bin/python3

# written by folkert van heusden
# mit license

import getopt
import sqlite3
import sys
 

def help():
    print('-s x  database to store results & fens in')
    print('-h    this help')

try:
    opts, args = getopt.getopt(sys.argv[1:], 's:h')

except getopt.GetoptError as err:
    print(err)
    help()
    sys.exit(2)

db_file = None

for o, a in opts:
    if o == '-s':
        db_file = a
    elif o == '-h':
        help()
        sys.exit(0)

if db_file == None:
    help()
    sys.exit(1)

con = sqlite3.connect(db_file)

cur = con.cursor()
cur.execute('SELECT fen, score, result FROM fens WHERE NOT score IS NULL')
for row in cur.fetchall():
    print(f'{row[0]}|{row[1]}|{row[2]}')
cur.close()
con.close()
