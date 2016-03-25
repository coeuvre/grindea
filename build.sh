#!/bin/sh

cd `dirname $0`
base=`pwd`

cc=clang
cflags="-W -Wall -g -std=c99 -DHM_DEBUG"
src=$base/src/grindea.c

[ ! -d "build" ] && mkdir build

cd build

$cc $cflags $src -lhammer -lm -lSDL2 -o grindea

