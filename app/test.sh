#! /bin/sh

cutechess-cli \
        -engine cmd=/usr/local/bin/Saruman proto=uci \
        -engine cmd=/home/folkert/Projects/esp32chesstest/app/src/a.out proto=uci name=Dog_v0.2_i5 \
        -concurrency 5 \
        -each dir=/home/folkert/Projects/esp32chesstest/app tc=40/10+0.25 book=dc-3200.bin -bookmode disk -rounds 500 -games 5 -tournament gauntlet -recover -repeat -pgnout $HOST.pgn -site "$HOST"

exit 0
        -engine cmd=/home/folkert/Projects/esp32chesstest/app/wrapper.sh proto=uci name=Dog_v0.2 \
        -engine cmd=/usr/local/bin/tscp proto=xboard name=tscp \
        -engine cmd=/usr/games/fairymax proto=xboard name=fairymax \
        -engine cmd=/usr/local/bin/mscp proto=xboard name=mscp \
