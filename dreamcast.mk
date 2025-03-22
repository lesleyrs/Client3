TARGET = client.elf
SRC := $(shell find src -type f -name '*.c')
OBJS = $(patsubst %.c, %.o, $(SRC))
CFLAGS += -Dclient -DWITH_RSA_LIBTOM -Wno-parentheses -Wall
DEBUG = 0
ifeq ($(DEBUG),1)
CFLAGS += -g
else
CFLAGS += -s -O3 -ffast-math -flto=$(shell nproc)
endif
MKDCDISC = /opt/toolchains/dc/mkdcdisc/builddir/mkdcdisc

all: rm-elf $(TARGET)

include $(KOS_BASE)/Makefile.rules

clean: rm-elf
	-rm -f $(OBJS)

rm-elf:
	-rm -f $(TARGET)

$(TARGET): $(OBJS)
	kos-cc -o $(TARGET) $(OBJS) -lSDL -lppp
	$(MKDCDISC) -e client.elf -o client.cdi -D rom --no-padding

run: $(TARGET)
	$(KOS_LOADER) $(TARGET)

dist: $(TARGET)
	-rm -f $(OBJS)
	$(KOS_STRIP) $(TARGET)
