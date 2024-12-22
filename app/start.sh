#! /bin/sh

/usr/local/bin/icsdroneng \
        -icsPort 5000 -icsHost nightmare-chess.nl -handle DogPC -password uemw \
        -dontReuseEngine off \
        -program ./run.sh \
        -resign off \
        -logging on
