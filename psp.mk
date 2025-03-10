TARGET = client
SRC := $(shell find src -type f -name '*.c')
OBJS = $(patsubst %.c, %.o, $(SRC))
LIBS = -lpsppower

INCDIR =
CFLAGS = -Wno-parentheses -Wall -Dclient_psp -DWITH_RSA_LIBTOM
DEBUG := 0
ifeq ($(DEBUG),1)
CFLAGS += -g -pg -O2 -ffast-math -flto=$(shell nproc)
else
CFLAGS += -s -O2 -ffast-math -flto=$(shell nproc)
endif
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

LIBDIR =
LDFLAGS =

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = client

PSPSDK=$(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
