#! /bin/sh

./toggle.py /dev/ttyACM1

/usr/bin/socat -s -v STDIO,echo=0,raw /dev/ttyACM1,raw,echo=0,b115200 2> err.txt

>&2 echo exit socat
