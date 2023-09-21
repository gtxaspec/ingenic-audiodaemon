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
CROSS_COMPILE?= mips-linux-uclibc-

CFLAGS += -muclibc
LDFLAG += -muclibc
endif

ifeq ($(CONFIG_MUSL_BUILD), y)
CROSS_COMPILE?= mipsel-openipc-linux-musl-
SDK_LIB_DIR = lib
SHIM = musl_shim.o
endif

ifeq ($(CONFIG_UCLIBC_BUILD), y)
SDK_LIB_DIR = lib
else
#SDK_LIB_DIR = ../../lib/glibc
endif

SDK_INC_DIR = include

LIBS = $(SDK_LIB_DIR)/libimp.so $(SDK_LIB_DIR)/libalog.so

INCLUDES = -I$(SDK_INC_DIR)

LDFLAG += -Wl,-gc-sections

ifeq ($(CONFIG_STATIC_BUILD), y)
LDFLAG += -static
#CFLAGS += -fPIC
LIBS = $(SDK_LIB_DIR)/libimp.a $(SDK_LIB_DIR)/libalog.a
endif

SAMPLES = audioplay_t31 \
	audio_daemon \
	audio_client \

all: 	$(SAMPLES)

audio_daemon: $(SDK_LIB_DIR)/libimp.a $(SDK_LIB_DIR)/libalog.a audio_daemon.o $(SHIM)
	$(CPLUSPLUS) $(LDFLAG) -o $@ $^ $(LIBS) -lpthread -lm -lrt -ldl
	$(STRIP) $@

audio_client: audio_client.o $(SHIM)
	$(CC) $(LDFLAG) -o $@ $^ -lpthread -lm -lrt -ldl
	$(STRIP) $@

audioplay_t31: $(SDK_LIB_DIR)/libimp.a $(SDK_LIB_DIR)/libalog.a  audioplay_t31.o $(SHIM)
	$(CPLUSPLUS) $(LDFLAG) -o $@ $^ $(LIBS) -lpthread -lm -lrt -ldl
	$(STRIP) $@

%.o:%.c sample-common.h
	$(CC) -c $(CFLAGS) $< -o $@

clean:
	rm -f *.o *~

distclean: clean
	rm -f $(SAMPLES)
