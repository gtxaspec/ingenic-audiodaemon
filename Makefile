# Variables
commit_tag=$(shell git rev-parse --short HEAD)

CC = ccache $(CROSS_COMPILE)gcc
STRIP = $(CROSS_COMPILE)strip

# Configuration
# uClibc & GCC + Static is broken... compiler 'TLS_DTPREL_VALUE' error.
CONFIG_UCLIBC_BUILD=n
CONFIG_GCC_BUILD=n
CONFIG_MUSL_BUILD=y
CONFIG_STATIC_BUILD=n
DEBUG=n

SDK_INC_DIR = include
INCLUDES += -I$(SDK_INC_DIR) \
           -I./src/iad/network \
           -I./src/iad/audio \
           -I./src/iac/client \
           -I./src/iad/utils \
           -I./build \
	   -I$(SDK_INC_DIR)/libwebsockets

CFLAGS ?= -O2 -Wall -march=mips32r2
CFLAGS += $(INCLUDES)

LDFLAGS ?= -Wl,-gc-sections

ifeq ($(DEBUG), y)
CFLAGS += -g
STRIPCMD = @echo "Not stripping binary due to DEBUG mode."
else
STRIPCMD = $(STRIP)
endif

ifeq ($(CONFIG_GCC_BUILD), y)
CROSS_COMPILE?= mips-linux-gnu-
LDFLAGS += -Wl,--no-as-needed -Wl,--allow-shlib-undefined
SDK_LIB_DIR = lib
LDLIBS = -lpthread -lm -lrt -ldl
endif

ifeq ($(CONFIG_UCLIBC_BUILD), y)
CROSS_COMPILE?= mips-linux-uclibc-gnu-
CFLAGS += -muclibc
# Set interpreter directory to ./libs
LDFLAGS += -muclibc -Wl,-dynamic-linker,libs/ld-uClibc.so.0
SDK_LIB_DIR = lib
LDLIBS = -lpthread -lm -lrt -ldl
endif

ifeq ($(CONFIG_MUSL_BUILD), y)
CROSS_COMPILE?= mipsel-linux-
SDK_LIB_DIR = lib
SHIM = build/obj/musl_shim.o
endif

ifeq ($(CONFIG_STATIC_BUILD), y)
CFLAGS += -DINGENIC_MMAP_STATIC
LDFLAGS += -static
LIBS = $(SDK_LIB_DIR)/libimp.a $(SDK_LIB_DIR)/libalog.a
LWS = $(SDK_LIB_DIR)/libwebsockets.a
CJSON = $(SDK_LIB_DIR)/libcjson.a
else
LIBS = $(SDK_LIB_DIR)/libimp.so $(SDK_LIB_DIR)/libalog.so
LWS = $(SDK_LIB_DIR)/libwebsockets.so
CJSON = $(SDK_LIB_DIR)/libcjson.so
endif

# Targets and Object Files
AUDIO_PROGS = build/bin/audioplay build/bin/iad build/bin/iac build/bin/wc-console build/bin/web_client
iad_OBJS = build/obj/iad.o build/obj/audio/output.o build/obj/audio/input.o build/obj/audio/audio_common.o \
build/obj/audio/audio_imp.o \
build/obj/network/network.o build/obj/network/control_server.o build/obj/network/input_server.o build/obj/network/output_server.o \
build/obj/utils/utils.o build/obj/utils/logging.o build/obj/utils/config.o build/obj/utils/cmdline.o $(SHIM)
iac_OBJS = build/obj/iac.o build/obj/client/cmdline.o build/obj/client/client_network.o build/obj/client/playback.o build/obj/client/record.o
web_client_OBJS = build/obj/web_client.o build/obj/web_client_src/cmdline.o build/obj/web_client_src/client_network.o build/obj/web_client_src/playback.o build/obj/web_client_src/utils.o
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
	./scripts/make_cJSON_deps.sh

dependancies: deps

build/obj/%.o: ingenic_musl/%.c
	$(CC) $(CFLAGS) -c $< -o $@

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

iad: build/bin/iad

build/bin/iad: version $(iad_OBJS)
	@mkdir -p $(@D)
	$(CC) $(LDFLAGS) -o $@ $(iad_OBJS) $(LIBS) ${CJSON} $(LDLIBS)
	$(STRIPCMD) $@

iac: build/bin/iac

build/bin/iac: version $(iac_OBJS)
	@mkdir -p $(@D)
	$(CC) $(LDFLAGS) -o $@ $(iac_OBJS) $(LDLIBS)
	$(STRIPCMD) $@

audioplay: build/bin/audioplay

build/bin/audioplay: version $(audioplay_OBJS)
	@mkdir -p $(@D)
	$(CC) $(LDFLAGS) -o $@ $(audioplay_OBJS) $(LIBS) $(LDLIBS)
	$(STRIPCMD) $@

wc-console: build/bin/wc-console

build/bin/wc-console: version $(wc_console_OBJS)
	@mkdir -p $(@D)
	$(CC) $(LDFLAGS) -o $@ $(wc_console_OBJS) ${LWS} $(LDLIBS)
	$(STRIPCMD) $@

web_client: build/bin/web_client

build/bin/web_client: version $(web_client_OBJS)
	@mkdir -p $(@D)
	$(CC) $(LDFLAGS) -o $@ $(web_client_OBJS) ${LWS} $(LDLIBS)
	$(STRIPCMD) $@

clean:
	-find build/obj -type f -name "*.o" -exec rm {} \;
	-rm -f build/version.h

distclean: clean
	-rm -f $(AUDIO_PROGS)
	-rm -rf build/*
	-rm -f lib/libwebsockets.* include/lws_config.h include/libwebsockets.h lib/libcjson.so
	-rm -rf include/libwebsockets
