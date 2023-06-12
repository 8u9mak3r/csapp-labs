#! /bin/bash

cd ~/Desktop/labs/csapp-labs/malloclab

./mdriver -f ./traces/short1-bal.rep -a -V
./mdriver -f ./traces/short2-bal.rep -a -V
./mdriver -f ./traces/amptjp-bal.rep -a -V
./mdriver -f ./traces/binary-bal.rep -a -V
./mdriver -f ./traces/binary2-bal.rep -a -V
./mdriver -f ./traces/cccp-bal.rep -a -V
./mdriver -f ./traces/coalescing-bal.rep -a -V
./mdriver -f ./traces/cp-decl-bal.rep -a -V
./mdriver -f ./traces/expr-bal.rep -a -V
./mdriver -f ./traces/random-bal.rep -a -V
./mdriver -f ./traces/random2-bal.rep -a -V
./mdriver -f ./traces/realloc-bal.rep -a -V
./mdriver -f ./traces/realloc2-bal.rep -a -V
