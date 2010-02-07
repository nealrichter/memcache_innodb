#!/bin/sh

set -x

#change the paths below to point to your embedded_innodb installation

rm -f *.o ib_kvtest

gcc -c -g -Wall -I/usr/local/src/embedded_innodb/include/embedded_innodb-1.0 -I. test0aux.c
gcc -c -g -Wall -I/usr/local/src/embedded_innodb/include/embedded_innodb-1.0 -I. ib_kvtest.c
gcc -o ib_kvtest ib_kvtest.o test0aux.o -L/usr/local/src/embedded_innodb/lib -linnodb -lpthread -lz -lm

echo "To run the ib_kvtest do this:"
echo "LD_LIBRARY_PATH=/usr/local/src/embedded_innodb/lib ./ib_kvtest"
