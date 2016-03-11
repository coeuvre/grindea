#!/bin/sh

cc=clang
src=`pwd`/src/grindea.c

clear

[ ! -d "build" ] && mkdir build

pushd build

$cc -W -Wall -g -std=c11 $src -c -o grindea.o
$cc -lSDL2 -lhammer grindea.o -o grindea

success=$?

popd

exit $success

