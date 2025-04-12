#! /bin/sh

polyglot make-book -pgn eco.pgn -bin dog-book1.bin -min-game 1
polyglot make-book -pgn noomen-pohl-select-books_x_4-combined-50.pgn -bin dog-book2.bin -min-game 1
polyglot merge-book -in1 dog-book1.bin -in2 dog-book2.bin -out dog-bookT.bin

TFILE=/tmp/pgn-dog-book.$$
chess-openings/bin/gen.py chess-openings/*tsv | cut -d '	' -f 3 | tail -n +2 | ./gen-lichess.py > $TFILE
polyglot make-book -pgn $TFILE -bin dog-book3.bin -min-game 1
polyglot merge-book -in1 dog-bookT.bin -in2 dog-book3.bin -out dog-bookT2.bin

TFILE2=/tmp/pgn-dog-book2.$$
./gen-silly-moves.py dog-bookT2.bin $TFILE2
polyglot make-book -pgn $TFILE2 -bin dog-book4.bin -min-game 1
polyglot merge-book -in1 dog-bookT2.bin -in2 dog-book4.bin -out dog-book.bin

cp dog-book.bin ../data

rm -f $TFILE dog-book1.bin dog-book2.bin dog-book3.bin dog-bookT.bin dog-bookT2.bin $TFILE2 dog-book4.bin
