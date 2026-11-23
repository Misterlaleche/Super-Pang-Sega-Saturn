JO_MINIMAL = 1
JO_DEBUG = 0
JO_NTSC = 1
JO_SCREEN_WIDTH = 320
JO_SCREEN_HEIGHT = 240
JO_FRAMERATE = 1
JO_MAX_SPRITE = 2400
JO_VDP1_MAX_COMMANDS = 512
# Keep the ordinary High-RAM heap distributed across useful contiguous zones.
# The linker gives both the pre-JO_WORK alignment gap and the post-work tail
# to the allocator too. Moving 14 KiB from this reservation keeps JO_WORK
# below the unchanged stack boundary after adding the native title/item art.
# 108544 also fits CELLT, CHARM, the decode dictionary and the temporary CD
# sector together, so CHARM does not spill into the audio prefix zone.
JO_GLOBAL_MEMORY_SIZE_FOR_MALLOC = 108544
JO_COMPILE_WITH_AUDIO_MODULE = 1
JO_COMPILE_WITH_FS_MODULE = 1
JO_COMPILE_WITH_BACKUP_MODULE = 1
JO_COMPILE_WITH_RAM_CARD_MODULE = 0
JO_COMPILE_WITH_PRINTF_MODULE = 0
JO_GAME_TITLE = SUPER PANG SATURN 1.4
JO_GAME_VERSION = V1.400
JO_GAME_RELEASE_DATE = 20261121
JO_CD_SECTOR_SIZE = 2352
JO_CD_AUDIO_DIR = cd_audio
JO_ENGINE_SRC_DIR = ./jo_engine
COMPILER_DIR = ./Compiler
SRCS = boot_diag.c main.c renderer.c render_common.c hardware_diag.c savegame.c stack_usage.c audio/audio.c $(wildcard stage/*.c frontend/*.c)
include $(COMPILER_DIR)/COMMON/jo_engine_makefile
SATURN_TOOLCHAIN_ROOT ?= ./toolchain/sh2eb-elf
CC = $(SATURN_TOOLCHAIN_ROOT)/bin/sh2eb-elf-gcc
CONV = $(SATURN_TOOLCHAIN_ROOT)/bin/sh2eb-elf-objcopy
LIBS = -Wl,--relax -lgcc
CCFLAGS += -mb -flto=4 -Wno-inline -I. -Iinclude -Istage -MMD -MP
LDFLAGS += -mb -nostdlib -Wl,--no-warn-rwx-segments
CUE_MAKER = ./tools/JoEngineCueMaker
ISO_TO_BIN = ./tools/JoEngineIsoToBin
MKISOFS = ./tools/mkisofs
# xorrisofs applies exclusions while importing paths: place them before ./cd.
MKISOFS_FLAGS := -exclude-list tools/disc_excludes.txt $(MKISOFS_FLAGS)

tools/disc_excludes.txt: tools/prepare_disc.py $(wildcard cd/ART/*)
	python3 tools/prepare_disc.py

game.iso: tools/disc_excludes.txt $(wildcard cd/ART/* cd/AUDIO/* cd/*.TXT)

-include $(SRCS:.c=.d)

