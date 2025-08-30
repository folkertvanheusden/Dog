#! /usr/bin/python3

import sys

for line in sys.stdin.readlines():
    print('[Event "?"]')
    print('[Site "?"]')
    print('[Date "2018.04.11"]')
    print('[Round "?"]')
    print('[White ""]')
    print('[Black "?"]')
    print()
    print(line.rstrip('\n') + ' *')
    print()
    print()
