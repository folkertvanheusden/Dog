#! /bin/sh

icsdrone \
        -icsPort 5000 -icsHost nightmare-chess.nl -handle Dog -password pahk \
        -dontReuseEngine off \
        -program ./run.sh \
        -resign off \
        -logging on
