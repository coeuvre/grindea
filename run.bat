@echo off

set base=%~dp0

pushd %base%

build && build\grindea.exe

popd
