SRC := $(shell find src -type f -name '*.c')
OBJS = $(patsubst %.c, %.o, $(SRC))

BUILD_PRX = 1

CFLAGS = -Wno-parentheses -Wall -Dclient_psp -DWITH_RSA_LIBTOM -Wextra -O2 -G0
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
LIBS = -lpsppower

PSPSDK=$(shell psp-config --pspsdk-path)

TARGET = client

include $(PSPSDK)/lib/build.mak
