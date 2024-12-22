#! /bin/sh

#/usr/bin/socat -v /dev/ttyACM0,b115200,echo=0,raw  PTY,link=myserial,raw,echo=0

/usr/bin/socat -s -v STDIO,echo=0,raw /dev/ttyUSB0,raw,echo=0,b115200 2> err.txt

>&2 echo exit socat
