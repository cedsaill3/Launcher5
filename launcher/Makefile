# Launcher5 - version PS5 - (c) 2026 cedsaill
# Dans Ubuntu (WSL) : make   ->  build/launcher5.elf  (le seul fichier a envoyer a la PS5)

SDK ?= $(HOME)/ps5-payload-sdk-full
CC  := $(SDK)/bin/prospero-clang
STRIP := $(SDK)/bin/prospero-strip
HB  := $(SDK)/target/user/homebrew

# version publiee : sans informations de debogage ni chemins de la machine de compilation
CFLAGS := -O2 -Wall -Wno-unused-parameter -Wno-format-truncation -D__PS5__ -DHAVE_MIXER \
          -I$(HB)/include -I$(HB)/include/SDL2 -I$(HB)/include/freetype2 -Isrc \
          -ffile-prefix-map=$(CURDIR)=. -ffile-prefix-map=$(SDK)=/sdk

UI_LIBS := $(HB)/lib/libSDL2main.a $(HB)/lib/libSDL2_mixer.a $(HB)/lib/libvorbisfile.a \
           $(HB)/lib/libvorbis.a $(HB)/lib/libogg.a $(HB)/lib/libmpg123.a $(HB)/lib/libFLAC.a \
           $(HB)/lib/libmodplug.a $(HB)/lib/libsamplerate.a $(HB)/lib/libSDL2.a $(HB)/lib/libSDL2_image.a \
           $(HB)/lib/libSDL2_ttf.a $(HB)/lib/libjansson.a $(HB)/lib/libfreetype.a \
           $(HB)/lib/libpng.a $(HB)/lib/libjpeg.a $(HB)/lib/libwebp.a $(HB)/lib/libwebpdemux.a \
           $(HB)/lib/libsharpyuv.a $(HB)/lib/libcurl.a $(HB)/lib/libpsl.a $(HB)/lib/libssl.a \
           $(HB)/lib/libcrypto.a $(HB)/lib/libzstd.a \
           -lSceSystemService -lSceUserService -lSceVideoOut -lSceAudioOut -lScePad \
           -lSceKeyboard -lSceImeDialog -lkernel_sys -lm -lz -lbz2 -liconv

UI_SRCS := src/main.c src/menu.c src/scan.c src/theme.c src/i18n.c src/platform_ps5.c src/font_data.c src/sha256.c
GPL_SRCS := gpl/pt.c gpl/elfldr.c gpl/hbldr.c gpl/fs_readfile.c

all: build/launcher5.elf

build:
	mkdir -p build

build/launcher5-ui.elf: $(UI_SRCS) src/menu.h src/platform.h src/theme.h src/i18n.h src/version.h src/sha256.h | build
	$(CC) $(CFLAGS) -o $@ $(UI_SRCS) $(UI_LIBS)
	$(STRIP) --strip-debug $@

build/launcher5.elf: src/bootstrap.c src/i18n.c src/i18n.h src/version.h $(GPL_SRCS) build/launcher5-ui.elf assets/relay/param.json assets/relay/icon0.png $(wildcard assets/fonds/*.jpg) $(wildcard assets/sons/*.mp3)
	$(CC) $(CFLAGS) -o $@ src/bootstrap.c src/i18n.c $(GPL_SRCS) $(HB)/lib/libsqlite3.a -lkernel_sys -lSceSystemService -lSceUserService -lSceAppInstUtil -lpthread
	$(STRIP) --strip-debug $@

clean:
	rm -rf build
