# You should only use Makefile-based build if you know what you're doing.
# For most vitasdk projects, CMake is a better choice. See CMakeLists.txt for an example.

PHONY := all package clean
rwildcard=$(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2) $(filter $(subst *,%,$2),$d))

CC := arm-vita-eabi-gcc
CXX := arm-vita-eabi-g++
STRIP := arm-vita-eabi-strip

PROJECT_TITLE := Jagex
PROJECT_TITLEID := VSDK20225

PROJECT := client
CFLAGS += -Wl,-q -I../common

SDL ?= 2
CFLAGS += -Dclient
CFLAGS += -DSDL=$(SDL) $(shell $(VITASDK)/arm-vita-eabi/bin/sdl2-config --cflags)
# CFLAGS += -DWITH_RSA_OPENSSL
CFLAGS += -DWITH_RSA_LIBTOM
DEBUG := 0
ifeq ($(DEBUG),1)
CFLAGS += -g
else
CFLAGS += -s -O2 -ffast-math -flto=$(shell nproc)
endif
CXXFLAGS += -Wl,-q -std=c++11 -I../common

SRC_C :=$(call rwildcard, src/, *.c)
SRC_CPP :=$(call rwildcard, src/, *.cpp)

OBJ_DIRS := $(addprefix out/, $(dir $(SRC_C:src/%.c=%.o))) $(addprefix out/, $(dir $(SRC_CPP:src/%.cpp=%.o)))
OBJS := $(addprefix out/, $(SRC_C:src/%.c=%.o)) $(addprefix out/, $(SRC_CPP:src/%.cpp=%.o))

LIBS += $(shell $(VITASDK)/arm-vita-eabi/bin/sdl2-config --libs) -lcrypto

all: package

package: $(PROJECT).vpk

$(PROJECT).vpk: eboot.bin param.sfo
	vita-pack-vpk -s param.sfo -b eboot.bin \
		--add sce_sys/icon0.png=sce_sys/icon0.png \
		--add sce_sys/livearea/contents/bg.png=sce_sys/livearea/contents/bg.png \
		--add sce_sys/livearea/contents/startup.png=sce_sys/livearea/contents/startup.png \
		--add sce_sys/livearea/contents/template.xml=sce_sys/livearea/contents/template.xml \
		--add rom/cache=cache \
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
	$(CXX) $(CXXFLAGS) $^ $(LIBS) -o $@

$(OBJ_DIRS):
	mkdir -p $@

out/%.o : src/%.cpp | $(OBJ_DIRS)
	arm-vita-eabi-g++ -c $(CXXFLAGS) -o $@ $<

out/%.o : src/%.c | $(OBJ_DIRS)
	arm-vita-eabi-gcc -c $(CFLAGS) -o $@ $<

clean:
	rm -f $(PROJECT).velf $(PROJECT).elf $(PROJECT).vpk param.sfo eboot.bin $(OBJS)
	rm -r $(abspath $(OBJ_DIRS))
