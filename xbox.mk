# eval "$(../nxdk/bin/activate -s)"
NXDK_DIR ?= $(CURDIR)/../nxdk

DEBUG = 0

ifeq ($(DEBUG),1)
DEBUG = y
else
CFLAGS += -O2
endif

XBE_TITLE = client
GEN_XISO = $(XBE_TITLE).iso
SRCS := $(shell find src -type f -name '*.c')
OUTPUT_DIR = rom
LTO = y
CFLAGS += -Wall -Dclient
CFLAGS += -DWITH_RSA_LIBTOM -DMP_NO_DEV_URANDOM -U_WIN32
NXDK_SDL = y

include $(NXDK_DIR)/Makefile
