@echo off

set base=%~dp0

set cc=clang
set cflags=-W -Wall -g -std=c99 -DHM_DEBUG
set src=%base%\src\grindea.c

if not exist %base%\build mkdir build

pushd %base%\build

%cc% %cflags% %src% -lhammer -o grindea.exe

popd
