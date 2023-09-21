CONFIG_UCLIBC_BUILD=n
CONFIG_MUSL_BUILD=y
CONFIG_STATIC_BUILD=y

CC = $(CROSS_COMPILE)gcc
CPLUSPLUS = $(CROSS_COMPILE)g++
LD = $(CROSS_COMPILE)ld
AR = $(CROSS_COMPILE)ar cr
STRIP = $(CROSS_COMPILE)strip

CFLAGS = $(INCLUDES) -O2 -Wall -march=mips32r2

ifeq ($(CONFIG_UCLIBC_BUILD), y)
CROSS_COMPILE?= mips-linux-uclibc-gnu-

CFLAGS += -muclibc
LDFLAG += -muclibc
endif

ifeq ($(CONFIG_MUSL_BUILD), y)
CROSS_COMPILE?= mipsel-openipc-linux-musl-
SDK_LIB_DIR = lib
SHIM = utils/musl_shim.o
endif

ifeq ($(CONFIG_UCLIBC_BUILD), y)
SDK_LIB_DIR = lib
endif

SDK_INC_DIR = include

LIBS = $(SDK_LIB_DIR)/libimp.so $(SDK_LIB_DIR)/libalog.so

INCLUDES = -I$(SDK_INC_DIR) -I./network -I./audio -I./client -I./utils

LDFLAG += -Wl,-gc-sections

ifeq ($(CONFIG_STATIC_BUILD), y)
LDFLAG += -static
LIBS = $(SDK_LIB_DIR)/libimp.a $(SDK_LIB_DIR)/libalog.a
endif

SAMPLES = audioplay_t31 \
          audio_daemon \
          audio_client \

AUDIO_DAEMON_OBJS = main.o audio/output.o network/network.o utils/utils.o utils/logging.o $(SHIM)
AUDIO_CLIENT_OBJS = audio_client.o client/cmdline.o client/client_network.o client/playback.o $(SHIM)

all: $(SAMPLES)

audio/audio.o: audio/output.c audio/output.h
	$(CC) -c $(CFLAGS) $< -o $@

network/network.o: network/network.c network/network.h
	$(CC) -c $(CFLAGS) $< -o $@

client/%.o: client/%.c client/%.h
	$(CC) -c $(CFLAGS) $< -o $@

utils/utils.o: utils/utils.c utils/utils.h utils/logging.c utils/logging.h
	$(CC) -c $(CFLAGS) $< -o $@

audio_daemon: $(SDK_LIB_DIR)/libimp.a $(SDK_LIB_DIR)/libalog.a $(AUDIO_DAEMON_OBJS)
	$(CPLUSPLUS) $(LDFLAG) -o $@ $^ $(LIBS) -lpthread -lm -lrt -ldl
	$(STRIP) $@

audio_client: $(AUDIO_CLIENT_OBJS)
	$(CC) $(LDFLAG) -o $@ $^ -lpthread -lm -lrt -ldl
	$(STRIP) $@

audioplay_t31: $(SDK_LIB_DIR)/libimp.a $(SDK_LIB_DIR)/libalog.a audioplay_t31.o $(SHIM)
	$(CPLUSPLUS) $(LDFLAG) -o $@ $^ $(LIBS) -lpthread -lm -lrt -ldl
	$(STRIP) $@

%.o:%.c sample-common.h
	$(CC) -c $(CFLAGS) $< -o $@

clean:
	rm -f *.o *~ audio/*.o network/*.o client/*.o utils/*.o audio_daemon audio_client audioplay_t31

distclean: clean
	rm -f $(SAMPLES)
