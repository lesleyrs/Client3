# eval "$(../nxdk/bin/activate -s)"

XBE_TITLE = client
GEN_XISO = $(XBE_TITLE).iso
SRCS := $(shell find src -type f -name '*.c')
NXDK_DIR ?= $(CURDIR)/../nxdk
OUTPUT_DIR = rom
LTO = y
CFLAGS = -Wall -Dclient -O2
CFLAGS += -DWITH_RSA_LIBTOM -DMP_NO_DEV_URANDOM -U_WIN32
# for stb_image
CFLAGS += -U_MSC_VER

include $(NXDK_DIR)/Makefile
