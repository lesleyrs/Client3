Set-Location -Path $PSScriptRoot

if (-not (Test-Path "SDL2.dll")) {
    copy bin/SDL2-devel-2.30.9-VC/SDL2-2.30.9/lib/x64/SDL2.dll SDL2.dll
}

if (-not (Test-Path "SDL3.dll")) {
    copy bin/SDL3-devel-3.1.6-VC/SDL3-3.1.6/lib/x64/SDL3.dll SDL3.dll
}

# $src = "src/*.c", "src/thirdparty/*.c", "src/datastruct/*.c", "src/platform/*.c", "src/sound/*.c", "src/wordenc/*.c", "src/entry/*.c"
$src = Get-ChildItem -Path src -Recurse -File -Filter *c

# $inc = "bin\SDL3-devel-3.1.6-VC\SDL3-3.1.6\include"
# $lib = "bin\SDL3-devel-3.1.6-VC\SDL3-3.1.6\lib\x64"
# $mingw_inc = "bin\SDL3-3.1.6\x86_64-w64-mingw32\include\SDL3"
# $mingw_lib = "bin\SDL3-3.1.6\x86_64-w64-mingw32\lib"

$inc = "bin\SDL2-devel-2.30.9-VC\SDL2-2.30.9\include"
$lib = "bin\SDL2-devel-2.30.9-VC\SDL2-2.30.9\lib\x64"
$mingw_inc = "bin\SDL2-2.30.9\x86_64-w64-mingw32\include\SDL2"
$mingw_lib = "bin\SDL2-2.30.9\x86_64-w64-mingw32\lib"

# TODO add -DSDL and -Dclient/midi/playground
# TODO clean this file and update it with ssl/non-ssl versions, and debug/release
# TODO cl add -MT or -MD -DNDEBUG -O3 -fp:fast and debug: -MTd or -MDd -Zi -Od -RTC1
# NOTE adding compiler to path after launching developer shell is not effective
# NOTE for console subsystem SDL2main.lib and Shell32.lib are unneeded if include "SDL.h" is removed from client.c (sdl3 doesn't need it at all)
# NOTE swapped -b bounds checking for just -g in tcc as it is extremely slow but gives better errors

# gcc $src -std=c99 -fwrapv -I"$mingw_inc" -DMODERN_POSIX -DWITH_RSA_LIBTOM -DSDL=2 -Dclient -ladvapi32 -o client.exe -L"$mingw_lib" -lmingw32 -lSDL2main -lSDL2 -lws2_32 && ./client $args
# clang -fwrapv -fsanitize=address -g -std=c99 -DMODERN_POSIX -Wall $src -I"$inc" -L"$lib" -DSDL=2 -Dclient -DMODERN_POSIX -DWITH_RSA_LIBTOM -lSDL2 -lSDL2main -lShell32 -lWs2_32 -lAdvapi32 -Xlinker -subsystem:windows -o client.exe -D_CRT_SECURE_NO_WARNINGS && ./client.exe $args
cl -fsanitize=address -D_CRT_SECURE_NO_WARNINGS -W3 -Zi -Feclient $src -I"$inc" -Dclient -DSDL=2 -DMODERN_POSIX -DWITH_RSA_LIBTOM -link -libpath:"$lib" -nologo Advapi32.lib SDL2.lib SDL2main.lib Shell32.lib Ws2_32.lib -subsystem:console && ./client.exe $args

# gcc $src -std=c99 -fwrapv -I"$mingw_inc" -DMODERN_POSIX -DWITH_RSA_OPENSSL -I"C:\Program Files\FireDaemon OpenSSL 3\include" -L"C:\Program Files\FireDaemon OpenSSL 3\lib" -lcrypto -o client.exe -L"$mingw_lib" -lmingw32 -lSDL2main -lSDL2 -lws2_32 && ./client $args
# gcc $src -std=c99 -fwrapv -I"$mingw_inc" -o client.exe -DMODERN_POSIX -L"$mingw_lib" -lmingw32 -lSDL2main -lSDL2 -lws2_32 && ./client $args
# tcc $src -std=c99 -Wall -Wimplicit-function-declaration -Wwrite-strings -I"$inc" -lws2_32 "-Wl,-subsystem=windows" -DMODERN_POSIX -v -g -bt -o client.exe -run SDL2.dll $args
# tcc $src -std=c99 -Wall -Wimplicit-function-declaration -Wwrite-strings -I"$inc" -lws2_32 "-Wl,-subsystem=console" -DMODERN_POSIX -DWITH_RSA_OPENSSL -I"C:\Program Files\FireDaemon OpenSSL 3\include" -L. -lcrypto -v -g -bt -o client.exe SDL2.dll $args
# tcc $src -std=c99 -Wall -Wimplicit-function-declaration -Wwrite-strings -I"$inc" -lws2_32 "-Wl,-subsystem=console" -DMODERN_POSIX -DWITH_RSA_OPENSSL -Iinclude -v -g -bt -o client.exe SDL2.dll libeay32.dll $args
# tcc $src -std=c99 -Wall -Wimplicit-function-declaration -Wwrite-strings -I"$inc" -lws2_32 "-Wl,-subsystem=console" -DMODERN_POSIX -v -b -bt -o client.exe SDL2.dll $args
# clang -fwrapv -fsanitize=address -g -std=c99 -Wall $src -I"$inc" -DMODERN_POSIX -DWITH_RSA_OPENSSL -I"C:\Program Files\FireDaemon OpenSSL 3\include" -L"C:\Program Files\FireDaemon OpenSSL 3\lib" -L"$lib" -llibcrypto -lSDL2 -lSDL2main -lShell32 -lWs2_32 -Xlinker -subsystem:console -o client.exe -D_CRT_SECURE_NO_WARNINGS && ./client.exe $args
# cl -fsanitize=address -D_CRT_SECURE_NO_WARNINGS -W3 -Zi -Feclient $src -I"$inc" -Dclient -DSDL=2 -DMODERN_POSIX -DWITH_RSA_OPENSSL -I"C:\Program Files\FireDaemon OpenSSL 3\include" -link -libpath:"C:\Program Files\FireDaemon OpenSSL 3\lib" -libpath:"$lib" -nologo libcrypto.lib SDL2.lib SDL2main.lib Shell32.lib Ws2_32.lib -subsystem:console && ./client.exe $args
# cl -D_CRT_SECURE_NO_WARNINGS -W3 -Zi -Feclient $src -I"$inc" -Dclient -DSDL=2 -DMODERN_POSIX -DWITH_RSA_OPENSSL -I"C:\Program Files\FireDaemon OpenSSL 3\include" -link -libpath:"C:\Program Files\FireDaemon OpenSSL 3\lib" -libpath:"$lib" -nologo libcrypto.lib SDL2.lib SDL2main.lib Shell32.lib Ws2_32.lib -subsystem:console && ./client.exe $args
# cl -D_CRT_SECURE_NO_WARNINGS -W3 -Zi -Feclient $src -I"$inc" -link -libpath:"$lib" -nologo SDL2.lib SDL2main.lib Shell32.lib Ws2_32.lib -subsystem:console && ./client.exe $args

# emscripten
# NOTE initial_heap is recommended over initial_memory
# -sINITIAL_HEAP=50MB
# -sINITIAL_MEMORY=50MB
# can also use -fsanitize=null -fsanitize-minimal-runtime

$src = (Get-ChildItem -Path "src" -Filter "*.c" -Recurse | ForEach-Object { $_.FullName })
# no sdl or sanitizers
# emcc -fwrapv -gsource-map --shell-file shell.html --preload-file cache/client --preload-file SCC1_Florestan.sf2 --preload-file Roboto @src -DWITH_RSA_BIGINT -Dclient -sALLOW_MEMORY_GROWTH -sINITIAL_HEAP=50MB -sSTACK_SIZE=1048576 -o index.html -sASYNCIFY -sSTRICT_JS -sDEFAULT_TO_CXX=0 -s -Oz -ffast-math -flto && emrun --no-browser --hostname 0.0.0.0 .

# TODO make a release build
# emcc -fwrapv -gsource-map --shell-file shell.html --preload-file cache/client --preload-file SCC1_Florestan.sf2 --preload-file Roboto @src -DWITH_RSA_BIGINT -Dclient -DSDL=2 --use-port=sdl2 -sALLOW_MEMORY_GROWTH -sINITIAL_HEAP=50MB -sSTACK_SIZE=1048576 -o index.html -sASYNCIFY -fsanitize=null -fsanitize-minimal-runtime -sSTRICT_JS -sDEFAULT_TO_CXX=0 && emrun --no-browser --hostname 0.0.0.0 .

$sslinc = "bin/openssl-web/include"
$ssllib = "bin/openssl-web"
# emcc -fwrapv -gsource-map --shell-file shell.html --preload-file cache/client --preload-file SCC1_Florestan.sf2 --preload-file Roboto @src -I"$sslinc" -L"$ssllib" -lcrypto -DWITH_RSA_OPENSSL -Dclient -DSDL=2 --use-port=sdl2 -sALLOW_MEMORY_GROWTH -sINITIAL_HEAP=50MB -sSTACK_SIZE=1048576 -o index.html -sASYNCIFY -fsanitize=null -fsanitize-minimal-runtime -sSTRICT_JS -sDEFAULT_TO_CXX=0 && emrun --no-browser --hostname 0.0.0.0 .

# windows 98 need to link sdlmain instead of -DSDL_main=main?
$sdl12_inc = "bin\SDL-devel-1.2.15-VC\SDL-1.2.15\include"
# tcc $src -std=c99 -Wall -Wimplicit-function-declaration -Wwrite-strings -I"$sdl12_inc" -lws2_32 "-Wl,-subsystem=console" -DSDL=1 -DWITH_RSA_OPENSSL -Ibin/openssl-0.9.8h-1-lib/include -v -o client.exe SDL.dll libeay32.dll $args
# ..\tinycc\win32\tcc.exe $src -std=c99 -Wall -Werror -Wimplicit-function-declaration -Wwrite-strings -DSDL=1 -I"$sdl12_inc" "-Wl,-subsystem=console" -DSDL_main=main -v -o client.exe SDL.dll && ./client $args
# ..\tinycc\win32\tcc.exe src/entry/midi.c src/packet.c src/lib/isaac.c src/lib/bzip.c -DSDL=1 -DMIDI -std=c99 -Wall -I"$sdl12_inc" -DSDL_main=main -v -o midi.exe SDL.dll $args

# -march=pentiumpro -mtune=pentiumpro
# gcc $src -s -O3 -ffast-math -std=c99 -Wall -Wimplicit-function-declaration -Wwrite-strings -DSDL=1 -I"$sdl12_inc" "-Wl,-subsystem=windows" -DSDL_main=main -o client.exe SDL.dll && ./client $args
# gcc src/entry/midi.c src/packet.c src/lib/isaac.c src/lib/bzip.c -s -O3 -ffast-math -DSDL=1 -DMIDI -std=c99 -Wall -I"$sdl12_inc" "-Wl,-subsystem=console" -DSDL_main=main -o midi.exe SDL.dll $args


# other entrypoints, needs updating
# PLAYGROUND
# tcc -DPLAYGROUND src/entry/playground.c src/platform/sdl2.c src/gameshell.c src/pix2d.c src/pix3d.c src/model.c src/pixfont.c src/pixmap.c src/pix8.c src/jagfile.c src/packet.c src/lib/isaac.c src/lib/bzip.c -std=c99 -Wall -I"$inc" -DSDL_main=main -v -o playground.exe -run SDL2.dll $args
# clang -fwrapv -std=c99 -Wall src/entry/playground.c src/platform/sdl2.c src/gameshell.c src/pix2d.c src/pix3d.c src/model.c src/pixfont.c src/pixmap.c src/pix8.c src/jagfile.c src/packet.c src/lib/isaac.c src/lib/bzip.c -ISDL2 -lSDL2 -o playground.exe -D_CRT_SECURE_NO_WARNINGS && ./playground.exe $args
# cl -DPLAYGROUND -D_CRT_SECURE_NO_WARNINGS -W3 src/entry/playground.c src/platform/sdl2.c src/gameshell.c src/pix2d.c src/pix3d.c src/model.c src/pixfont.c src/pixmap.c src/pix8.c src/jagfile.c src/packet.c src/lib/isaac.c src/lib/bzip.c -I"$inc" -link -libpath:"$lib" sdl2.lib sdl2main.lib shell32.lib /nologo /subsystem:console && ./playground.exe $args
# gcc -std=c99 -Wall src/entry/playground.c src/platform/sdl2.c src/gameshell.c src/pix2d.c src/pix3d.c src/model.c src/pixfont.c src/pixmap.c src/pix8.c src/jagfile.c src/packet.c src/lib/isaac.c src/lib/bzip.c -ISDL2 -lSDL2 -o playground && ./playground.exe $args
# emcc -fwrapv --preload-file cache/client --preload-file SCC1_Florestan.sf2 src/entry/playground.c src/platform/sdl2.c src/gameshell.c src/pix2d.c src/pix3d.c src/model.c src/pixfont.c src/pixmap.c src/pix8.c src/jagfile.c src/packet.c src/lib/isaac.c src/lib/bzip.c --use-port=sdl2 -o index.html -sASYNCIFY -fsanitize=null -fsanitize-minimal-runtime && py -m http.server

# MIDI
# tcc -DMIDI -std=c99 -Wall src/entry/midi.c src/packet.c src/lib/isaac.c src/lib/bzip.c -I"$inc" -DSDL_main=main -v -o midi.exe -run SDL2.dll $args
# clang -fwrapv -DMIDI -std=c99 -Wall src/entry/midi.c src/packet.c src/lib/isaac.c src/lib/bzip.c -I"$inc" -L"$lib" -lSDL2 -lSDL2main -lShell32 -Xlinker -subsystem:console -o midi.exe -D_CRT_SECURE_NO_WARNINGS && ./midi.exe $args
# cl -DMIDI src/entry/midi.c src/packet.c src/lib/isaac.c src/lib/bzip.c -I"$inc" -link -libpath:"$lib" sdl2.lib -nologo -subsystem:console && ./midi.exe $args
# gcc -DMIDI -std=c99 -Wall src/entry/midi.c src/lib/bzip.c -I"$inc" -L"$lib" -lSDL2 -o midi && ./midi.exe $args
# emcc -fwrapv -DMIDI --preload-file cache/client --preload-file SCC1_Florestan.sf2 src/entry/midi.c src/lib/bzip.c --use-port=sdl2 -o index.html -sASYNCIFY -fsanitize=null -fsanitize-minimal-runtime && py -m http.server
