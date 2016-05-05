@echo off

set base=%~dp0

set cc=cl
set cflags=/Od /nologo /DHM_DEBUG
set src=%base%\src\grindea.c

if not exist %base%\build mkdir build

pushd %base%\build

REM Change /subsystem:console to /subsystem:windows to disable console
%cc% %cflags% %src% /link hammer.lib /subsystem:console

popd
