#!/bin/sh -e

autoreconf -i
./configure --prefix=/home/jurkiew/click/
make
cp *.{uo,ko} ../../packages/$(uname -m)/
