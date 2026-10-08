# Makefile: minecraft for linux, macos and windows (mingw)

CC      := gcc
CSTD    := -std=c99
WARN    := -Wall -Wextra
INCLUDE := -I.

# debug (default) or release
BUILD ?= debug

SRC = minecraft.c player.c mob.c mob_ai.c timer.c  hitresult.c user.c options.c jrandom.c save.c math_helper.c java_sort.c \
      inventory.c \
      level/chunk.c level/level.c level/level_renderer.c level/javaser.c \
      level/tile/tile.c phys/aabb.c entity.c character/creature.c \
      item/arrow.c item/item.c \
      character/polygon.c character/cube.c \
      level/levelgen/level_gen.c \
      level/levelgen/synth/synth.c level/levelgen/synth/improved_noise.c \
      level/levelgen/synth/perlin_noise.c level/levelgen/synth/distort.c \
      particle/particle_engine.c particle/particle.c \
      character/zombie_model.c character/mob_zombie_model.c \
      character/mob_skeleton_model.c character/quadruped_model.c \
      character/mob_creeper_model.c character/mob_spider_model.c character/humanoid_armor_model.c \
      renderer/textures.c renderer/tessellator.c \
      renderer/texture_fx.c \
      renderer/frustum.c gui/font.c gui/screen.c gui/pause_screen.c \
      gui/generate_level_screen.c gui/chat_input_screen.c gui/message_screen.c \
      gui/options_screen.c gui/controls_screen.c gui/block_select_screen.c \
      gui/game_over_screen.c \
      net/net_socket.c net/packet.c net/connection.c net/network_player.c \
      audio/sound.c

# stb_vorbis.c is included by sound.c
# comm/ and server/ aren't built, the client never uses them

UNAME_S := $(shell uname -s)

CFLAGS  := $(CSTD) $(WARN) $(INCLUDE)
DEPFLAGS := -MMD -MP

ifeq ($(BUILD),release)
    CFLAGS  += -O2 -DNDEBUG
else
    CFLAGS  += -O0 -g
endif

ifeq ($(UNAME_S),Linux)
    EXE     := minecraft
    # -lpthread for the connect thread, -lasound for audio
    SRC     += audio/audio_backend_alsa.c
    LDFLAGS := -lglfw -lGLEW -lGL -lGLU -lm -lz -lpthread -lasound
endif

ifeq ($(UNAME_S),Darwin)
    EXE     := minecraft
    # glfw and glew from homebrew, AudioToolbox for audio
    SRC     += audio/audio_backend_coreaudio.c
    LDFLAGS := -lglfw -lGLEW -framework OpenGL -framework AudioToolbox -lz -lpthread
endif

ifeq ($(OS),Windows_NT)
    EXE := minecraft.exe
    # -lwinmm for timeBeginPeriod and audio
    SRC += audio/audio_backend_win.c
    ifeq ($(BUILD),release)
        LDFLAGS := -lglfw3 -lglew32 -lopengl32 -lglu32 -lz -lws2_32 -lwinmm -mwindows
    else
        LDFLAGS := -lglfw3 -lglew32 -lopengl32 -lglu32 -lz -lws2_32 -lwinmm
    endif
endif

# after the platform blocks, they add the audio backend to SRC
OBJ := $(SRC:.c=.o)
DEP := $(OBJ:.o=.d)

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
