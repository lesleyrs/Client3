TARGET = client
SRCS := $(shell find src -type f -name '*.c')
OBJS = $(patsubst %.c, %.o, $(SRCS))
LIBS = -lpsppower

INCDIR =
CFLAGS = -Wno-parentheses -Wall -Dclient
DEBUG := 0
ifeq ($(DEBUG),1)
CFLAGS += -g
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
