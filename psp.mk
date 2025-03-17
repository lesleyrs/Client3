TARGET = client
SRC := $(shell find src -type f -name '*.c')
OBJS = $(patsubst %.c, %.o, $(SRC))
LIBS = -lpsppower

INCDIR =
CFLAGS = -Wno-parentheses -Wall -Dclient -DWITH_RSA_LIBTOM
PROF ?= 0
DEBUG ?= 0
ifeq ($(DEBUG),1)
BUILD_PRX = 1
# NOTE: libs out of order warning with flto?
CFLAGS += -g -O2 -ffast-math
else
CFLAGS += -s -O2 -ffast-math -flto=$(shell nproc)
endif
ifeq ($(PROF),1)
BUILD_PRX = 0
CFLAGS += -pg
endif
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

LIBDIR =
LDFLAGS =

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = client

PSPSDK=$(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
