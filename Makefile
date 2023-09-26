# Variables
commit_tag=$(shell git rev-parse --short HEAD)

CC = $(CROSS_COMPILE)gcc
CPLUSPLUS = $(CROSS_COMPILE)g++
STRIP = $(CROSS_COMPILE)strip

SDK_INC_DIR = include
INCLUDES = -I$(SDK_INC_DIR) -I./network -I./audio -I./client -I./utils
CFLAGS = $(INCLUDES) -O2 -Wall -march=mips32r2
LDFLAG += -Wl,-gc-sections

# Configuration
CONFIG_UCLIBC_BUILD=n
CONFIG_MUSL_BUILD=y
CONFIG_STATIC_BUILD=y

ifeq ($(CONFIG_UCLIBC_BUILD), y)
CROSS_COMPILE?= mips-linux-uclibc-gnu-
CFLAGS += -muclibc
LDFLAG += -muclibc
SDK_LIB_DIR = lib
endif

ifeq ($(CONFIG_MUSL_BUILD), y)
CROSS_COMPILE?= mipsel-openipc-linux-musl-
SDK_LIB_DIR = lib
SHIM = utils/musl_shim.o
endif

ifeq ($(CONFIG_STATIC_BUILD), y)
LDFLAG += -static
LIBS = $(SDK_LIB_DIR)/libimp.a $(SDK_LIB_DIR)/libalog.a
else
LIBS = $(SDK_LIB_DIR)/libimp.so $(SDK_LIB_DIR)/libalog.so
endif

# Targets and Object Files
AUDIO_PROGS = audioplay iad iac

iad_OBJS = iad.o audio/output.o audio/input.o network/network.o utils/utils.o utils/logging.o $(SHIM)
iac_OBJS = iac.o client/cmdline.o client/client_network.o client/playback.o client/record.o $(SHIM)

.PHONY: all version clean distclean

all: version $(AUDIO_PROGS)

version:
	@if  ! grep "$(commit_tag)" version.h >/dev/null ; then \
	echo "update version.h" ; \
	sed 's/COMMIT_TAG/"$(commit_tag)"/g' version.tpl.h > version.h ; \
	fi

%.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@

#As libimp is based on C++ libraries, so mips-linux-gnu-g++ is used for linking process.
#API linking order: [IVS libraries] [mxu libraries] [libimp/libsysutils] [libalog]
#(2022). T31 Development resource compilation (Rev 1.0). [Ingenic]. Section 4.1, Page 9.

iad: $(iad_OBJS)
	$(CPLUSPLUS) $(LDFLAG) -o $@ $^ $(LIBS) -lpthread -lm -lrt -ldl
	$(STRIP) $@

iac: $(iac_OBJS)
	$(CPLUSPLUS) $(LDFLAG) -o $@ $^ -lpthread -lm -lrt -ldl
	$(STRIP) $@

audioplay: standalone/audioplay.o $(SHIM)
	$(CPLUSPLUS) $(LDFLAG) -o $@ $^ $(LIBS) -lpthread -lm -lrt -ldl
	$(STRIP) $@

clean:
	rm -f *.o *~ audio/*.o network/*.o client/*.o utils/*.o version.h iad iac audioplay

distclean: clean
	rm -f $(AUDIO_PROGS)
