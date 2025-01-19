#! /bin/sh

/usr/local/bin/icsdroneng \
        -icsPort 5000 -icsHost nightmare-chess.nl -handle DogPC -password uemw \
        -dontReuseEngine off \
        -program ./run.sh \
        -resign off \
	-book /home/folkert/bin/data/dc-3200.bin \
        -logging on
