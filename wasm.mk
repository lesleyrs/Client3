CC = clang --target=wasm32 --sysroot=../wasm/libc -nodefaultlibs -mbulk-memory
LDFLAGS = -Wl,--allow-undefined -Wl,--export-table -lm -lc
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

# TODO
CFLAGS += -DMP_NO_DEV_URANDOM
# WITH_LIBTOM ?= 1
# ifeq ($(WITH_JS_BIGINT), 1)
# CFLAGS += -DWITH_RSA_BIGINT
# endif

ifeq ($(DEBUG),0)
# TODO use -Oz or -O3?
CFLAGS += -DNDEBUG -s -Oz -ffast-math -flto
else ifeq ($(DEBUG),1)
CFLAGS += -g
endif

OUT = $(ENTRY).wasm
RUN = python3 -m http.server

.PHONY: all client playground midi
all:
ifeq ($(DEBUG),0)
	$(CC) $(CFLAGS) $(SRC) $(LDFLAGS) -o $(OUT) && wasm-opt $(OUT) -o $(OUT) -Oz && wasm-strip $(OUT)
else
	$(CC) $(CFLAGS) $(SRC) $(LDFLAGS) -o $(OUT)
endif

run: all
	$(RUN)

# llvm-dwarfdump -a $(OUT) > $(OUT).dwarf
# ../emscripten/tools/wasm-sourcemap.py $(OUT) -w $(OUT) -p $(CURDIR) -s -u ./$(OUT).map -o $(OUT).map --dwarfdump-output=$(OUT).dwarf
dump:
	../emscripten/tools/wasm-sourcemap.py $(OUT) -w $(OUT) -p $(CURDIR) -s -u ./$(OUT).map -o $(OUT).map --dwarfdump=/usr/bin/llvm-dwarfdump
