@echo off
cd /d "%~dp0"
setlocal enabledelayedexpansion
set SRC=
for /r "src" %%f in (*.c) do (
    set SRC="%%f" !SRC!
)

set SDL1="bin\SDL-devel-1.2.15-VC\SDL-1.2.15\include"
set SDL2="bin\SDL2-devel-2.30.9-VC\SDL2-2.30.9\include"
set SDL3="bin\SDL3-devel-3.1.6-VC\SDL3-3.1.6\include"
set RELEASE=-Wl,-subsystem=windows
set DEBUG=-Wl,-subsystem=console -g

set CC=bin\tcc\tcc
::set CC=tcc
::set CC=gcc
::set CC=emcc
set SDL=1
set ENTRY=client
set OPT=%DEBUG%

goto :setup
:shift1
shift
:shift2
shift
:setup
if "%1" == "-cc" set CC=%2&& goto :shift1
if "%1" == "-v" set SDL=%2&& goto :shift1
if "%1" == "-e" set ENTRY=%2&& goto :shift1
if "%1" == "-r" set OPT=%RELEASE%&& goto :shift2
if "%1" == "-gl" set GL11=1&& goto :shift2
if "%1" == "" goto build

:usage
echo usage: build.bat [ options ... ]
echo options:
echo   -cc   set compiler (tcc/gcc/emcc)
echo   -v    set sdl version (1/2/3)
echo   -e    set entry (client/playground/midi)
echo   -r    set release build
echo   -gl   use experimental opengl 1.1 renderer
exit /B 1

:build
if "%SDL%" == "3" (
	set SDL=-DSDL=3 -I%SDL3%
	set VER=3
) else if "%SDL%" == "2" (
	set SDL=-DSDL=2 -I%SDL2%
	set VER=2
) else (
	set SDL=-DSDL=1 -I%SDL1%
	set VER=
)

REM need -DSDL_main here since we included sdl in same file as the one with main in it?
if "%ENTRY%" == "midi" (
	set SRC=src/entry/midi.c src/thirdparty/bzip.c -DSDL_main=main
)

if not exist SDL2.dll (
	copy bin\SDL2-devel-2.30.9-VC\SDL2-2.30.9\lib\x86\SDL2.dll SDL2.dll
)

if not exist SDL3.dll (
	REM copy bin\SDL3-devel-3.1.6-VC\SDL3-3.1.6\lib\x86\SDL3.dll SDL3.dll
)

if "%CC%" == "cl" (
	exit /B 1
) else if "%CC%" == "emcc" (
	REM && emrun --no-browser --hostname 0.0.0.0 .
	REM add emscripten debug
	set COMPILE=-fwrapv --use-port=sdl3 --shell-file shell.html -DNDEBUG -s -Oz -ffast-math -flto --closure 1 -std=c99 -DWITH_RSA_LIBTOM -D%ENTRY% -sALLOW_MEMORY_GROWTH -sINITIAL_HEAP=50MB -sSTACK_SIZE=1048576 -o client.html -sJSPI -sDEFAULT_TO_CXX=0 -sENVIRONMENT=web -sSINGLE_FILE -sSINGLE_FILE_BINARY_ENCODE=0
) else if "%CC%" == "gcc" (
	set COMPILE=-fwrapv -s -O3 -ffast-math -std=c99 -DSDL_main=main -DWITH_RSA_LIBTOM -D%ENTRY% %SDL% -lws2_32 -lwsock32 %OPT% -o %ENTRY%.exe SDL%VER%.dll
) else (
	set COMPILE=-v -std=c99 -Wall -Wwrite-strings -DWITH_RSA_LIBTOM -D%ENTRY% %SDL% -lws2_32 -lwsock32 %OPT% -o %ENTRY%.exe SDL%VER%.dll

	if "%OPT%" == "%DEBUG%" (
		set COMPILE=-bt !COMPILE!
	)
	if "%GL11%" == "1" (
		set COMPILE=-DGL11 !COMPILE! -lopengl32
	)
)

set COMPILE=%CC% %SRC% !COMPILE!

echo %COMPILE%
%COMPILE%
