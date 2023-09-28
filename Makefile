# Variables
commit_tag=$(shell git rev-parse --short HEAD)

CC = $(CROSS_COMPILE)gcc
CXX = $(CROSS_COMPILE)g++
STRIP = $(CROSS_COMPILE)strip

SDK_INC_DIR = include
INCLUDES = -I$(SDK_INC_DIR) -I./build -I./src/network -I./src/audio -I./src/client -I./src/utils
CFLAGS = $(INCLUDES) -O2 -Wall -march=mips32r2
LDFLAGS += -Wl,-gc-sections
LDLIBS = -lpthread -lm -lrt -ldl

# Configuration
CONFIG_UCLIBC_BUILD=n
CONFIG_MUSL_BUILD=y
CONFIG_STATIC_BUILD=y

ifeq ($(CONFIG_UCLIBC_BUILD), y)
CROSS_COMPILE?= mips-linux-uclibc-gnu-
CFLAGS += -muclibc
LDFLAGS += -muclibc
SDK_LIB_DIR = lib
endif

ifeq ($(CONFIG_MUSL_BUILD), y)
CROSS_COMPILE?= mipsel-openipc-linux-musl-
SDK_LIB_DIR = lib
SHIM = src/common/musl_shim.o
endif

ifeq ($(CONFIG_STATIC_BUILD), y)
LDFLAGS += -static
LIBS = $(SDK_LIB_DIR)/libimp.a $(SDK_LIB_DIR)/libalog.a
else
LIBS = $(SDK_LIB_DIR)/libimp.so $(SDK_LIB_DIR)/libalog.so
endif

# Targets and Object Files
AUDIO_PROGS = build/bin/audioplay build/bin/iad build/bin/iac

iad_OBJS = build/obj/iad.o build/obj/audio/output.o build/obj/audio/input.o build/obj/network/network.o build/obj/utils/utils.o build/obj/utils/logging.o $(SHIM)
iac_OBJS = build/obj/iac.o build/obj/client/cmdline.o build/obj/client/client_network.o build/obj/client/playback.o build/obj/client/record.o $(SHIM)
audioplay_OBJS = build/obj/standalone/audioplay.o $(SHIM)
wc_console_OBJS = build/obj/wc-console/wc-console.o

.PHONY: all version clean distclean iad iac audioplay deps

all: prepare version $(AUDIO_PROGS)

deps:
	./config/make_libwebsockets_deps.sh

dependancies: deps

prepare:
	mkdir -p build/obj/audio build/obj/network build/obj/client build/obj/utils build/obj/standalone build/bin build/obj/wc-console build/bin

version:
	@if  ! grep "$(commit_tag)" build/version.h >/dev/null 2>&1 ; then \
	echo "update version.h" ; \
	sed 's/COMMIT_TAG/"$(commit_tag)"/g' config/version.tpl.h > build/version.h ; \
	fi

#As libimp is based on C++ libraries, so mips-linux-gnu-g++ is used for linking process.
#API linking order: [IVS libraries] [mxu libraries] [libimp/libsysutils] [libalog]
#(2022). T31 Development resource compilation (Rev 1.0). [Ingenic]. Section 4.1, Page 9.

build/obj/%.o: src/iad/%.c
	$(CC) -c $(CFLAGS) $< -o $@

build/obj/%.o: src/iac/%.c
	$(CC) -c $(CFLAGS) $< -o $@

build/obj/standalone/%.o: src/standalone/%.c
	$(CC) -c $(CFLAGS) $< -o $@

build/obj/wc-console/%.o: src/wc-console/%.c
	$(CC) -c $(CFLAGS) $< -o $@

iad: build/bin/iad

build/bin/iad: version $(iad_OBJS)
	$(CXX) $(LDFLAGS) -o $@ $(iad_OBJS) $(LIBS) $(LDLIBS)
	$(STRIP) $@

iac: build/bin/iac

build/bin/iac: version $(iac_OBJS)
	$(CXX) $(LDFLAGS) -o $@ $(iac_OBJS) $(LDLIBS)
	$(STRIP) $@

audioplay: build/bin/audioplay

build/bin/audioplay: version $(audioplay_OBJS)
	$(CXX) $(LDFLAGS) -o $@ $(audioplay_OBJS) $(LIBS) $(LDLIBS)
	$(STRIP) $@

wc-console: build/bin/wc-console

build/bin/wc-console: version $(wc_console_OBJS)
	$(CXX) $(LDFLAGS) -o $@ $(wc_console_OBJS) $(SDK_LIB_DIR)/libwebsockets.a $(LDLIBS)
	$(STRIP) $@

clean:
	find build/obj -type f -name "*.o" -exec rm {} \;
	rm -f build/bin/* build/version.h
	rm -rf build/lws-build
	rm -rf include/libwebsockets
	rm -f lib/libwebsockets.a
	rm -f include/lws_config.h
	rm -f include/libwebsockets.h

distclean: clean
	rm -f $(AUDIO_PROGS)
