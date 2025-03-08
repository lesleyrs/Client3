# you can also use emcc with `emmake make`
ifeq ($(CC),cc)
CC = gcc
# CC = clang
# CC = i686-w64-mingw32-gcc
# CC = x86_64-w64-mingw32-gcc
# CC = tcc
# CC = emcc
endif
ENTRY ?= $(or $(filter midi playground,$(word 1, $(MAKECMDGOALS))),client)
# DEBUG 0/1/2
DEBUG ?= 1
SANITIZE ?= 0
# MODERN_POSIX required by musl libc as it doesn't have the older networking functions
MODERN_POSIX ?= 1
# RSA bits divided by 4? 128 for 512, 256 for 1024
RSA_LENGTH ?= 128

ifeq ($(basename $(notdir $(CC))),emcc)
WITH_JS_BIGINT ?= 1
# getnameinfo does nothing with emscripten so use old api
MODERN_POSIX = 0
# NOTE waiting for sdl3 emscripten
SDL = 2
else ifeq ($(findstring i686-w64-mingw32-gcc,$(CC)),i686-w64-mingw32-gcc)
# sdl1 dll is the only one guaranteed to be 32 bits, others can be found in bin
SDL = 1
else
SDL ?= 2
endif

# we only include 32 bit libcrypto to simplify building for legacy systems
ifeq ($(CC),x86_64-w64-mingw32-gcc)
WITH_OPENSSL = 0
else
WITH_OPENSSL ?= 1
endif

ifeq ($(ENTRY),midi)
SRC := src/entry/midi.c src/thirdparty/bzip.c
else
# SRC := $(shell find src -type f -name '*.c')
SRC = $(wildcard src/*.c src/thirdparty/*.c src/datastruct/*.c src/sound/*.c src/wordenc/*.c)
SRC += src/entry/$(ENTRY).c
ifdef SDL
SRC += src/platform/sdl$(SDL).c
endif
endif

CFLAGS += -D$(ENTRY)
ifdef SDL
CFLAGS += -DSDL=$(SDL)
endif
CFLAGS += -fwrapv -std=c99 -Wall -Wpedantic -Wvla -Wshadow -Wmissing-prototypes -Wstrict-prototypes -Wmissing-declarations -Wredundant-decls
CFLAGS += -Wextra
CFLAGS += -DRSA_BUF_LEN=$(RSA_LENGTH)

ifeq ($(findstring gcc,$(CC)),gcc)
# removed (mingw)-gcc warning as clangd doesn't autocomplete them due to being less strict
CFLAGS += -Wno-parentheses
else ifneq ($(filter clang emcc,$(basename $(notdir $(CC)))),)
CFLAGS += -Wno-null-pointer-subtraction
endif

ifeq ($(MODERN_POSIX),1)
ifneq ($(CC),i686-w64-mingw32-gcc)
CFLAGS += -DMODERN_POSIX=1
# https://man7.org/linux/man-pages/man7/feature_test_macros.7.html
# http://www.schweikhardt.net/identifiers.html
CFLAGS += -D_POSIX_C_SOURCE=200112L
# CFLAGS += -D_POSIX_C_SOURCE=200809L
# CFLAGS += -D_XOPEN_SOURCE=600
# CFLAGS += -D_XOPEN_SOURCE=700
endif
endif

# Faster RSA encryption
ifeq ($(WITH_JS_BIGINT), 1)
CFLAGS += -DWITH_RSA_BIGINT
endif

ifeq ($(WITH_OPENSSL), 1)
CFLAGS += -DWITH_RSA_OPENSSL
ifeq ($(basename $(notdir $(CC))),emcc)
CFLAGS += -Ibin/openssl-web/include
LDFLAGS += -Lbin/openssl-web -lcrypto
# CFLAGS += $(shell pkg-config --cflags openssl/libcrypto.pc)
# LDFLAGS += $(shell pkg-config --libs openssl/libcrypto.pc)
else ifeq ($(findstring -w64-mingw32-gcc,$(CC)),-w64-mingw32-gcc)
CFLAGS += -Ibin/openssl-0.9.8h-1-lib/include
LDFLAGS += -Lbin/openssl-0.9.8h-1-lib/lib -lcrypto
else
CFLAGS += $(shell pkg-config --cflags libcrypto)
LDFLAGS += $(shell pkg-config --libs libcrypto)
endif
endif

ifeq ($(SDL),1)
ifeq ($(findstring -w64-mingw32-gcc,$(CC)),-w64-mingw32-gcc)
CFLAGS += -D_WIN32_WINNT=0x0501
CFLAGS += -I"bin/SDL-1.2.15/include/SDL" -D_GNU_SOURCE=1 -Dmain=SDL_main
LDFLAGS += -L"bin/SDL-1.2.15/lib" -lmingw32 -lSDLmain -lSDL -mwindows
else
CFLAGS += $(shell sdl-config --cflags)
LDFLAGS += $(shell sdl-config --libs) -lm
endif
endif

ifeq ($(SDL),2)
ifeq ($(CC),tcc)
CFLAGS += -DSDL_DISABLE_IMMINTRIN_H
# NOTE: this was also needed for some reason
# sudo ln -s /usr/lib/x86_64-linux-gnu/pulseaudio/libpulsecommon-16.1.so /usr/lib/x86_64-linux-gnu/libpulsecommon-16.1.so
endif

ifeq ($(basename $(notdir $(CC))),emcc)
CFLAGS += --shell-file shell.html --preload-file cache/client --preload-file SCC1_Florestan.sf2 --preload-file Roboto
# CFLAGS += -sJSPI
CFLAGS += -sASYNCIFY
CFLAGS += -sSTACK_SIZE=1048576 -sINITIAL_HEAP=50MB
CFLAGS += -sALLOW_MEMORY_GROWTH -sASSERTIONS=2
CFLAGS += -sDEFAULT_TO_CXX=0
CFLAGS += -sWEBSOCKET_URL=ws://
LDFLAGS += --use-port=sdl2
else ifeq ($(findstring -w64-mingw32-gcc,$(CC)),-w64-mingw32-gcc)
CFLAGS += $(shell bin/SDL2-2.30.9/$(word 1, $(subst -, ,$(CC)))-w64-mingw32/bin/sdl2-config --cflags)
LDFLAGS += $(shell bin/SDL2-2.30.9/$(word 1, $(subst -, ,$(CC)))-w64-mingw32/bin/sdl2-config --libs)
else
CFLAGS += $(shell sdl2-config --cflags)
LDFLAGS += $(shell sdl2-config --libs) -lm
endif
endif

ifeq ($(SDL),3)
ifeq ($(findstring -w64-mingw32-gcc,$(CC)),-w64-mingw32-gcc)
CFLAGS += $(shell pkg-config bin/SDL3-3.1.6/$(word 1, $(subst -, ,$(CC)))-w64-mingw32/lib/pkgconfig/sdl3.pc --cflags)
LDFLAGS += $(shell pkg-config bin/SDL3-3.1.6/$(word 1, $(subst -, ,$(CC)))-w64-mingw32/lib/pkgconfig/sdl3.pc --libs)
else
CFLAGS += $(shell pkg-config sdl3 --cflags)
LDFLAGS += $(shell pkg-config sdl3 --libs) -lm
endif
endif

ifeq ($(findstring -w64-mingw32-gcc,$(CC)),-w64-mingw32-gcc)
LDFLAGS += -lws2_32
DEBUG = 0
endif

ifeq ($(DEBUG),0)
ifeq ($(basename $(notdir $(CC))),emcc)
# TODO use -Oz or -O3?
CFLAGS += -DNDEBUG -s -Oz -ffast-math
else
CFLAGS += -DNDEBUG -s -O3 -ffast-math
endif
ifeq ($(findstring gcc,$(CC)),gcc)
CFLAGS += -flto=$(shell nproc)
else
CFLAGS += -flto
endif
else ifeq ($(DEBUG),1)
ifeq ($(basename $(notdir $(CC))),emcc)
CFLAGS += -gsource-map
# LDFLAGS += -sSOCKET_DEBUG -sRUNTIME_DEBUG=0
SAN += -fsanitize=null -fsanitize-minimal-runtime
# SAN += -fsanitize=undefined
else
SAN += -fsanitize=address,undefined
CFLAGS += -g
ifeq ($(CC),tcc)
# CFLAGS += -b
# CFLAGS += -bt
endif
endif
else ifeq ($(DEBUG),2)
ifeq ($(basename $(notdir $(CC))),emcc)
CFLAGS += -gsource-map
endif
SAN += -fsanitize=address,undefined
# avoids the "too many locals" emcc error in client_read for -fsanitize=address
CFLAGS += -fno-omit-frame-pointer -g -O2 -fno-inline -ffast-math
endif

ifeq ($(basename $(notdir $(CC))),emcc)
OUT = index.html
RUN = emrun --no-browser --hostname localhost .
else
OUT = $(if $(findstring client,$(ENTRY)),client,$(ENTRY))
RUN = $(if $(findstring -w64-mingw32-gcc,$(CC)),$(if $(filter $(shell uname -r | grep -c microsoft), 0),wine)) ./$(OUT)$(if $(findstring -w64-mingw32-gcc,$(CC)),.exe)
endif

ifeq ($(SANITIZE), 0)
SAN :=
endif

.PHONY: all client playground midi
all:
	$(CC) $(CFLAGS) $(SRC) $(LDFLAGS) $(SAN) -o $(OUT)

run: all
	$(RUN)

cg:
	$(CC) $(CFLAGS) $(SRC) $(LDFLAGS) -o $(OUT) && valgrind --tool=callgrind ./$(OUT)
	callgrind_annotate $$(ls callgrind.out.* | sort -V | tail -n 1) | less

vg:
	$(CC) $(CFLAGS) $(SRC) $(LDFLAGS) -o $(OUT) && valgrind ./$(OUT)

vga:
	$(CC) $(CFLAGS) $(SRC) $(LDFLAGS) -o $(OUT) && valgrind --leak-check=full --show-leak-kinds=all ./$(OUT)

perf:
	perf record ./client && perf report

gdb: all
	gdb ./$(OUT)

check:
	cppcheck src -j$(shell nproc)

scan:
	scan-build $(MAKE) -j$(shell nproc)

san:
	$(MAKE) -j$(shell nproc) WITH_OPENSSL=1 DEBUG=1 SANITIZE=1 run

dev:
	$(MAKE) -j$(shell nproc) WITH_OPENSSL=1 DEBUG=1 vg

# we can copy either vc or mingw, both seem to work?
sdl32:
	cp bin/SDL2-devel-2.30.9-VC/SDL2-2.30.9/lib/x86/SDL2.dll SDL2.dll
	cp bin/SDL3-devel-3.1.6-VC/SDL3-3.1.6/lib/x86/SDL3.dll SDL3.dll

sdl64:
	cp bin/SDL2-devel-2.30.9-VC/SDL2-2.30.9/lib/x64/SDL2.dll SDL2.dll
	cp bin/SDL3-devel-3.1.6-VC/SDL3-3.1.6/lib/x64/SDL3.dll SDL3.dll

playground: run
midi: run
