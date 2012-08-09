@ECHO OFF
set PATH=C:\mingw64\bin;C:\mingw64\lib;%PATH%

del png.exe
gcc -std=c99 -L. -lzlibwapi debug.c png.c -o png.exe

pause

png.exe