@echo off
cd /d "%~dp0"
setlocal enabledelayedexpansion
set SRC=
for /r "src" %%f in (*.c) do (
    set SRC=!SRC! "%%f"
)

set SDL1="bin\SDL-devel-1.2.15-VC\SDL-1.2.15\include"
set SDL2="bin\SDL2-devel-2.30.9-VC\SDL2-2.30.9\include"
set SDL3="bin\SDL3-devel-3.1.6-VC\SDL3-3.1.6\include"
set SSLINC="bin\openssl-0.9.8h-1-lib\include"
set SSLLIB="bin\openssl-0.9.8h-1-lib\lib"
::set SSLWEBINC="bin\openssl-web\include"
::set SSLWEBLIB="bin\openssl-web"
set RELEASE=-Wl,-subsystem=windows
set DEBUG=-Wl,-subsystem=console -g

set CC=bin\tcc\tcc
::set CC=tcc
::set CC=gcc
::set CC=emcc
set SDL=2
set ENTRY=client
set OPT=%DEBUG%

goto :setup
:shift1
shift
:shift2
shift
:setup
if "%1" == "-c" set CC=%2&& goto :shift1
if "%1" == "-v" set SDL=%2&& goto :shift1
if "%1" == "-e" set ENTRY=%2&& goto :shift1
if "%1" == "-r" set OPT=%RELEASE%&& goto :shift2
if "%1" == "" goto build

:usage
echo usage: build.bat [ options ... ]
echo options:
echo   -c    set compiler (tcc/gcc/emcc)
echo   -v    set sdl version (1/2/3)
echo   -e    set entry (client/playground/midi)
echo   -r    set release build
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

REM are the mingw and VC dlls the same?
if not exist SDL2.dll (
	copy bin\SDL2-devel-2.30.9-VC\SDL2-2.30.9\lib\x86\SDL2.dll SDL2.dll
)

if not exist SDL3.dll (
	copy bin\SDL3-devel-3.1.6-VC\SDL3-3.1.6\lib\x86\SDL3.dll SDL3.dll
)

REM add remaining debug builds
REM -DWITH_RSA_LIBTOM can be used instead of -DWITH_RSA_OPENSSL to avoid binaries but tcc doesn't include wincrypt.h by default
if "%CC%" == "cl" (
	echo TODO support some legacy version
	exit /B 1
) else if "%CC%" == "emcc" (
	%CC% %SRC% -fwrapv -gsource-map --shell-file shell.html --preload-file cache\client --preload-file SCC1_Florestan.sf2 --preload-file Roboto -s -Oz -ffast-math -flto -std=c99 -DWITH_RSA_BIGINT -D%ENTRY% -sALLOW_MEMORY_GROWTH -sINITIAL_HEAP=50MB -sSTACK_SIZE=1048576 -o index.html -sASYNCIFY -sSTRICT_JS -sDEFAULT_TO_CXX=0 && emrun --no-browser --hostname 0.0.0.0 .

	REM -fsanitize=null -fsanitize-minimal-runtime
	REM %CC% %SRC% -fwrapv -gsource-map --shell-file shell.html --preload-file cache\client --preload-file SCC1_Florestan.sf2 --preload-file Roboto -s -Oz -ffast-math -flto -std=c99 -DWITH_RSA_BIGINT -D%ENTRY% -DSDL=2 --use-port=sdl2 -sALLOW_MEMORY_GROWTH -sINITIAL_HEAP=50MB -sSTACK_SIZE=1048576 -o index.html -sASYNCIFY -sSTRICT_JS -sDEFAULT_TO_CXX=0 && emrun --no-browser --hostname 0.0.0.0 .
) else if "%CC%" == "gcc" (
	::%CC% %SRC% -s -O3 -ffast-math -std=c99 -DSDL_main=main -DWITH_RSA_OPENSSL -D%ENTRY% %SDL% -I%SSLINC% -lws2_32 %OPT% -o %ENTRY%.exe SDL%VER%.dll libeay32.dll
	REM added static linking for openssl, so linux mingw builds don't need the dll in same dir as well
	::%CC% %SRC% -s -O3 -ffast-math -std=c99 -DSDL_main=main -DWITH_RSA_OPENSSL -D%ENTRY% %SDL% -I%SSLINC% -L%SSLLIB% -lcrypto -lws2_32 %OPT% -o %ENTRY%.exe SDL%VER%.dll
	REM added libtom option
	%CC% %SRC% -s -O3 -ffast-math -std=c99 -DSDL_main=main -DWITH_RSA_LIBTOM -D%ENTRY% %SDL% -lws2_32 %OPT% -o %ENTRY%.exe SDL%VER%.dll
) else (
	REM if using your own tcc you could also add -b for better errors (SLOW, and libs not stored in repo)
	REM need to add else branch for now to add -bt until SRC is changed
	if "%OPT%" == "%DEBUG%" (
		%CC%.exe -bt -v %SRC% -std=c99 -Wall -Wwrite-strings -DWITH_RSA_OPENSSL -D%ENTRY% %SDL% -I%SSLINC% -lws2_32 %OPT% -o %ENTRY%.exe SDL%VER%.dll libeay32.dll
	) else (
		%CC%.exe -v %SRC% -std=c99 -Wall -Wwrite-strings -DWITH_RSA_OPENSSL -D%ENTRY% %SDL% -I%SSLINC% -lws2_32 %OPT% -o %ENTRY%.exe SDL%VER%.dll libeay32.dll
	)
)
