#! /bin/sh

(cd src ; g++ -Ofast -ggdb3 main.cpp psq.cpp tt.cpp -I../include -pthread)
