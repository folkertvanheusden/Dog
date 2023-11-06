#! /bin/sh

/usr/local/icsdroneng/bin/icsdrone \
        -icsPort 5000 -icsHost nightmare-chess.nl -handle DogPC -password uemw \
        -dontReuseEngine off \
        -program ./run.sh \
        -resign off \
        -logging on
