@echo off

set base=%~dp0

pushd %base%

build && hammer_player build\grindea.dll

popd
