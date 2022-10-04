#! /bin/sh

cd /home/folkert/Projects/esp32chesstest/app

/usr/games/polyglot -noini -log true -ec ./wrapper.sh 2> bla.txt

>&2 echo exit polyglot
