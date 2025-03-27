LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := main

SDL_PATH := ../SDL

LOCAL_C_INCLUDES := $(LOCAL_PATH)/$(SDL_PATH)/include

LOCAL_CFLAGS := -Dclient -DWITH_RSA_LIBTOM -DSDL=2 -Wall -s -O3 -ffast-math -flto

LOCAL_SRC_FILES := src/allocator.c src/animbase.c src/animframe.c src/clientstream.c src/collisionmap.c src/component.c src/custom.c src/datastruct/doublylinkable.c src/datastruct/doublylinklist.c src/datastruct/hashtable.c src/datastruct/jstring.c src/datastruct/linkable.c src/datastruct/linklist.c src/datastruct/lrucache.c src/entity.c src/entry/client.c src/entry/midi.c src/entry/playground.c src/flotype.c src/gameshell.c src/ground.c src/idktype.c src/inputtracking.c src/jagfile.c src/locentity.c src/locmergeentity.c src/loctype.c src/model.c src/npcentity.c src/npctype.c src/objstackentity.c src/objtype.c src/packet.c src/pathingentity.c src/pix24.c src/pix2d.c src/pix3d.c src/pix8.c src/pixfont.c src/pixmap.c src/platform/3ds.c src/platform/dreamcast.c src/platform/nds.c src/platform/psp.c src/platform/sdl1.c src/platform/sdl2.c src/platform/sdl3.c src/platform/vita.c src/platform/wii.c src/platform/xbox.c src/platform.c src/playerentity.c src/projectileentity.c src/seqtype.c src/sound/envelope.c src/sound/tone.c src/sound/wave.c src/spotanimentity.c src/spotanimtype.c src/thirdparty/bn.c src/thirdparty/bzip.c src/thirdparty/ini.c src/thirdparty/isaac.c src/thirdparty/mpi.c src/thirdparty/rsa-bigint.c src/thirdparty/rsa-libtom.c src/thirdparty/rsa-openssl.c src/thirdparty/rsa-tiny.c src/tileoverlay.c src/tileunderlay.c src/varptype.c src/wordenc/wordpack.c src/world.c src/world3d.c

LOCAL_SHARED_LIBRARIES := SDL2

LOCAL_LDLIBS := -lGLESv1_CM -lGLESv2 -lOpenSLES -llog -landroid

include $(BUILD_SHARED_LIBRARY)
