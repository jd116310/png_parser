@ECHO OFF
set PATH=C:\mingw64\bin;C:\mingw64\lib;%PATH%

del a.exe
gcc -std=c99 -L. -lzlib1 png.c

pause

a.exe