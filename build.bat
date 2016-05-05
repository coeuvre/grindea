@echo off

set base=%~dp0

set cc=cl
set cflags=/Od /DHM_DEBUG
set src=%base%\src\grindea.c

if not exist %base%\build mkdir build

pushd %base%\build

%cc% %cflags% %src% /link hammer.lib

popd
