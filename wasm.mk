LIBC = ../wasmlite/libc

WASM_SOURCEMAP = ../tools/emscripten/tools/wasm-sourcemap.py
# NOTE: system llvm-dwarfdump might be incompatible as wasm-sourcemap.py uses new features, so we use emsdk repo
DWARFDUMP = $(HOME)/emsdk/upstream/bin/llvm-dwarfdump
# DWARFDUMP = /usr/bin/llvm-dwarfdump

CC = clang --target=wasm32 --sysroot=$(LIBC) -nodefaultlibs -mbulk-memory
LDFLAGS = -Wl,--export-table -Wl,--stack-first -Wl,--error-limit=0 -lm
ENTRY ?= client
# ENTRY ?= playground
DEBUG ?= 1
# RSA bits divided by 4? 128 for 512, 256 for 1024
RSA_LENGTH ?= 128

# SRC := $(shell find src -type f -name '*.c')
SRC = $(wildcard src/*.c src/datastruct/*.c src/sound/*.c src/wordenc/*.c src/thirdparty/*.c)
SRC += src/entry/$(ENTRY).c
SRC += src/platform/webassembly.c

CFLAGS += -D$(ENTRY)
CFLAGS += -fwrapv -std=c99 -Wall -Wpedantic -Wvla -Wshadow -Wmissing-prototypes -Wstrict-prototypes -Wmissing-declarations -Wredundant-decls
CFLAGS += -Wextra
CFLAGS += -Wno-null-pointer-subtraction

CFLAGS += -DMP_NO_DEV_URANDOM
WITH_LIBTOM ?= 1
WITH_JS_BIGINT ?= 1

ifeq ($(WITH_JS_BIGINT), 1)
CFLAGS += -DWITH_RSA_BIGINT
else ifeq ($(WITH_LIBTOM), 1)
CFLAGS += -DWITH_RSA_LIBTOM
endif

ifeq ($(DEBUG),0)
CFLAGS += -DNDEBUG -s -Oz -ffast-math -flto
LDFLAGS += -lc
else
CFLAGS += -g
LDFLAGS += -lc-dbg
endif

OUT = $(ENTRY).wasm
RUN = python3 -m http.server

.PHONY: all client playground midi
all:
	$(CC) $(CFLAGS) $(SRC) $(LDFLAGS) -o $(OUT)
ifeq ($(DEBUG),0)
	wasm-opt $(OUT) -o $(OUT) -Oz && wasm-strip $(OUT)
else
	$(WASM_SOURCEMAP) $(OUT) -w $(OUT) -p $(CURDIR) -s -u ./$(OUT).map -o $(OUT).map --dwarfdump=$(DWARFDUMP)
endif

# llvm-dwarfdump -a $(OUT) > $(OUT).dwarf
# $(WASM_SOURCEMAP) $(OUT) -w $(OUT) -p $(CURDIR) -s -u ./$(OUT).map -o $(OUT).map --dwarfdump-output=$(OUT).dwarf

run: all
	$(RUN)
