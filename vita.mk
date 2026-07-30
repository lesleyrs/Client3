PHONY := all package clean
rwildcard=$(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2) $(filter $(subst *,%,$2),$d))

CC := arm-vita-eabi-gcc
STRIP := arm-vita-eabi-strip

PROJECT_TITLE := Jagex
PROJECT_TITLEID := VSDK20225

PROJECT := client

DEBUG ?= 0
SDL ?= 0
WITH_OPENSSL ?= 0
GL ?= 0

# GNU coreutils `nproc` is not available on macOS.
NPROC := $(shell nproc 2>/dev/null || sysctl -n hw.logicalcpu 2>/dev/null || getconf _NPROCESSORS_ONLN 2>/dev/null || echo 1)

ifeq ($(DEBUG),1)
CFLAGS += -g
else
CFLAGS += -O3 -ffast-math -flto=$(NPROC)
endif

ifeq ($(GL),1)
CFLAGS += -DGL11
LIBS += -lvitaGL -lvitashark -lmathneon -lSceGxm_stub -lSceShaccCg_stub -lSceShaccCgExt -lSceAppMgr_stub -lSceKernelDmacMgr_stub -lSceCommonDialog_stub -ltaihen_stub
endif

ifeq ($(SDL),2)
CFLAGS += -DSDL=$(SDL) $(shell $(VITASDK)/arm-vita-eabi/bin/sdl2-config --cflags)
LIBS += $(shell $(VITASDK)/arm-vita-eabi/bin/sdl2-config --libs)
else
LIBS += -lstdc++ -lm -lSceDisplay_stub -lSceTouch_stub -lSceCtrl_stub -lScePower_stub -lSceAudio_stub
endif

ifeq ($(WITH_OPENSSL),1)
CFLAGS += -DWITH_RSA_OPENSSL
LIBS += -lcrypto
else
CFLAGS += -DWITH_RSA_LIBTOM
endif

CFLAGS += -D$(PROJECT)
CFLAGS += -Wl,-q -std=c99

SOURCES := $(call rwildcard, src/, *.c)

OBJ_DIRS := $(sort $(addprefix out/, $(dir $(SOURCES:src/%.c=%.o))))
OBJS := $(addprefix out/, $(SOURCES:src/%.c=%.o))

all: package

package: $(PROJECT).vpk

$(PROJECT).vpk: eboot.bin param.sfo
	vita-pack-vpk -s param.sfo -b eboot.bin \
		--add sce_sys/icon0.png=sce_sys/icon0.png \
		--add sce_sys/livearea/contents/bg.png=sce_sys/livearea/contents/bg.png \
		--add sce_sys/livearea/contents/startup.png=sce_sys/livearea/contents/startup.png \
		--add sce_sys/livearea/contents/template.xml=sce_sys/livearea/contents/template.xml \
		--add rom/cache=rom/cache \
		--add rom/Roboto=Roboto \
		--add rom/SCC1_Florestan.sf2=SCC1_Florestan.sf2 \
		--add rom/config.ini=config.ini \
	$(PROJECT).vpk

eboot.bin: $(PROJECT).velf
	vita-make-fself $(PROJECT).velf eboot.bin

param.sfo:
	vita-mksfoex -s TITLE_ID="$(PROJECT_TITLEID)" "$(PROJECT_TITLE)" param.sfo

$(PROJECT).velf: $(PROJECT).elf
	$(STRIP) -g $<
	vita-elf-create $< $@

$(PROJECT).elf: $(OBJS)
	$(CC) $(CFLAGS) $^ $(LIBS) -o $@

$(OBJ_DIRS):
	@mkdir -p $@

out/%.o : src/%.c | $(OBJ_DIRS)
	$(CC) -c $(CFLAGS) -o $@ $<

clean:
	rm -f $(PROJECT).velf $(PROJECT).elf $(PROJECT).vpk param.sfo eboot.bin $(OBJS)
	rm -r $(abspath $(OBJ_DIRS))
