# Variables
commit_tag=$(shell git rev-parse --short HEAD)

CC = ccache $(CROSS_COMPILE)gcc
CXX = ccache $(CROSS_COMPILE)g++
STRIP = $(CROSS_COMPILE)strip

SDK_INC_DIR = include
INCLUDES = -I$(SDK_INC_DIR) \
           -I./src/iad/network \
           -I./src/iad/audio \
           -I./src/iac/client \
           -I./src/iad/utils \
           -I./build \
	   -I$(SDK_INC_DIR)/libwebsockets

CFLAGS = $(INCLUDES) -O2 -Wall -march=mips32r2
LDFLAGS += -Wl,-gc-sections
LDLIBS = -lpthread -lm -lrt -ldl

# Configuration
# uClibc + Static is broken... compiler 'TLS_DTPREL_VALUE' error.
CONFIG_UCLIBC_BUILD=n
CONFIG_MUSL_BUILD=y
CONFIG_STATIC_BUILD=y
DEBUG=n

ifeq ($(DEBUG), y)
CFLAGS += -g -O0  # Add -g for debugging symbols and -O0 to disable optimizations
STRIPCMD = @echo "Not stripping binary due to DEBUG mode."
else
CFLAGS += -O2  # Use -O2 optimization level when not in DEBUG mode
STRIPCMD = $(STRIP)
endif

ifeq ($(CONFIG_UCLIBC_BUILD), y)
CROSS_COMPILE?= mips-linux-uclibc-gnu-
CFLAGS += -muclibc
# Set interpreter directory to ./libs
LDFLAGS += -muclibc -Wl,-dynamic-linker,libs/ld-uClibc.so.0
SDK_LIB_DIR = lib
endif

ifeq ($(CONFIG_MUSL_BUILD), y)
CROSS_COMPILE?= mipsel-openipc-linux-musl-
SDK_LIB_DIR = lib
SHIM = build/obj/musl_shim.o
endif

ifeq ($(CONFIG_STATIC_BUILD), y)
LDFLAGS += -static
LIBS = $(SDK_LIB_DIR)/libimp.a $(SDK_LIB_DIR)/libalog.a
LWS = $(SDK_LIB_DIR)/libwebsockets.a
else
LIBS = $(SDK_LIB_DIR)/libimp.so $(SDK_LIB_DIR)/libalog.so
LWS = $(SDK_LIB_DIR)/libwebsockets.so
endif

# Targets and Object Files
AUDIO_PROGS = build/bin/audioplay build/bin/iad build/bin/iac build/bin/wc-console build/bin/web_client

iad_OBJS = build/obj/iad.o build/obj/audio/output.o build/obj/audio/input.o build/obj/network/network.o build/obj/utils/utils.o build/obj/utils/logging.o build/obj/utils/config.o build/obj/utils/cmdline.o build/cJSON-build/cJSON/cJSON.o $(SHIM)
iac_OBJS = build/obj/iac.o build/obj/client/cmdline.o build/obj/client/client_network.o build/obj/client/playback.o build/obj/client/record.o $(SHIM)
web_client_OBJS = build/obj/web_client.o build/obj/web_client_src/cmdline.o build/obj/web_client_src/client_network.o build/obj/web_client_src/playback.o $(SHIM)
audioplay_OBJS = build/obj/standalone/audioplay.o $(SHIM)
wc_console_OBJS = build/obj/wc-console/wc-console.o

.PHONY: all version clean distclean audioplay wc-console web_client

all: version $(AUDIO_PROGS)

BUILD_DIR = build

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

version: $(BUILD_DIR)
	@if  ! grep "$(commit_tag)" build/version.h >/dev/null 2>&1 ; then \
	echo "update version.h" ; \
	sed 's/COMMIT_TAG/"$(commit_tag)"/g' config/version.tpl.h > build/version.h ; \
	fi

deps:
	./scripts/make_libwebsockets_deps.sh
	./scripts/make_cJSON_deps.sh download_only

dependancies: deps

build/obj/%.o: src/common/%.c
	@mkdir -p $(@D)
	$(CC) -c $(CFLAGS) $< -o $@

build/obj/%.o: src/iad/%.c
	@mkdir -p $(@D)
	$(CC) -c $(CFLAGS) $< -o $@

build/obj/%.o: src/iac/%.c
	@mkdir -p $(@D)
	$(CC) -c $(CFLAGS) $< -o $@

build/obj/%.o: src/web_client/%.c
	@mkdir -p $(@D)
	$(CC) -c $(CFLAGS) $< -o $@

build/obj/standalone/%.o: src/standalone/%.c
	@mkdir -p $(@D)
	$(CC) -c $(CFLAGS) $< -o $@

build/obj/wc-console/%.o: src/wc-console/%.c
	@mkdir -p $(@D)
	$(CC) -c $(CFLAGS) $< -o $@

#As libimp is based on C++ libraries, so mips-linux-gnu-g++ is used for linking process.
#API linking order: [IVS libraries] [mxu libraries] [libimp/libsysutils] [libalog]
#(2022). T31 Development resource compilation (Rev 1.0). [Ingenic]. Section 4.1, Page 9.

iad: build/bin/iad

build/bin/iad: version $(iad_OBJS)
	@mkdir -p $(@D)
	$(CXX) $(LDFLAGS) -o $@ $(iad_OBJS) $(LIBS) $(LDLIBS)
	$(STRIPCMD) $@

iac: build/bin/iac

build/bin/iac: version $(iac_OBJS)
	@mkdir -p $(@D)
	$(CXX) $(LDFLAGS) -o $@ $(iac_OBJS) $(LDLIBS)
	$(STRIPCMD) $@

audioplay: build/bin/audioplay

build/bin/audioplay: version $(audioplay_OBJS)
	@mkdir -p $(@D)
	$(CXX) $(LDFLAGS) -o $@ $(audioplay_OBJS) $(LIBS) $(LDLIBS)
	$(STRIPCMD) $@

wc-console: build/bin/wc-console

build/bin/wc-console: version $(wc_console_OBJS)
	@mkdir -p $(@D)
	$(CXX) $(LDFLAGS) -o $@ $(wc_console_OBJS) ${LWS} $(LDLIBS)
	$(STRIPCMD) $@

web_client: build/bin/web_client

build/bin/web_client: version $(web_client_OBJS)
	@mkdir -p $(@D)
	$(CXX) $(LDFLAGS) -o $@ $(web_client_OBJS) ${LWS} $(LDLIBS)
	$(STRIPCMD) $@

clean:
	-find build/obj -type f -name "*.o" -exec rm {} \;
	-rm -f build/version.h

distclean: clean
	-rm -f $(AUDIO_PROGS)
	-rm -rf build/*
	-rm -f lib/libwebsockets.* include/lws_config.h include/libwebsockets.h lib/libcjson.a include/cJSON.h
	-rm -rf include/libwebsockets
