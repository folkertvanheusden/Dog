#! /bin/sh

(cd src ; g++ -Ofast -ggdb3 main.cpp psq.cpp tt.cpp eval.cpp eval_par.cpp -I../include -pthread)
