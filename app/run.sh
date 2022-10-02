#! /bin/sh

cd /home/folkert/Projects/esp32chesstest/app

strace -s 1500 -f /usr/games/polyglot -noini -log true -ec ./wrapper.sh 2> bla.txt

>&2 echo exit polyglot
