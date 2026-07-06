# Makefile: the server for linux, macos and windows (mingw)

CC      := gcc
CSTD    := -std=c99
WARN    := -Wall -Wextra
INCLUDE := -I.

BUILD ?= debug

SRC = main.c server.c player.c player_list.c properties.c log.c \
      jexc.c jstring.c jfile.c jgzip.c jtime.c jrandom.c \
      level/level.c level/level_io.c level/javaser.c level/tile/tile.c \
      level/levelgen/level_gen.c \
      level/levelgen/synth/synth.c level/levelgen/synth/improved_noise.c \
      level/levelgen/synth/perlin_noise.c level/levelgen/synth/distort.c \
      net/net_socket.c net/packet.c net/connection.c

OBJ := $(SRC:.c=.o)
DEP := $(OBJ:.o=.d)

UNAME_S := $(shell uname -s)

CFLAGS  := $(CSTD) $(WARN) $(INCLUDE)
DEPFLAGS := -MMD -MP

ifeq ($(BUILD),release)
    CFLAGS += -O2 -DNDEBUG
else
    CFLAGS += -O0 -g
endif

ifeq ($(UNAME_S),Linux)
    EXE     := minecraft-server
    CFLAGS  += -D_POSIX_C_SOURCE=200809L
    LDFLAGS := -lz -lpthread -lm
endif

ifeq ($(UNAME_S),Darwin)
    EXE     := minecraft-server
    LDFLAGS := -lz -lpthread
endif

ifeq ($(OS),Windows_NT)
    EXE := minecraft-server.exe
    # MSYS2 / MinGW
    LDFLAGS := -lz -lws2_32
endif

.PHONY: all
all: $(EXE)

$(EXE): $(OBJ)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

.PHONY: debug release run clean
debug:
	$(MAKE) BUILD=debug
release:
	$(MAKE) BUILD=release
run: $(EXE)
	./$(EXE)

clean:
	@rm -f $(EXE) $(OBJ) $(DEP) 2>/dev/null || true

-include $(DEP)
