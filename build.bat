@echo off

IF NOT EXIST .\build mkdir .\build
pushd .\build
cl -FC -Zi /nologo ..\client.cpp user32.lib gdi32.lib ws2_32.lib
popd

move build\client.exe client.exe 2> nul
