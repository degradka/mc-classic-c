# Makefile — build minecraft (Linux/macOS/Windows-MinGW)

# Toolchain
CC      := gcc
CSTD    := -std=c99
WARN    := -Wall -Wextra
INCLUDE := -I.

# Build type: debug (default) or release
BUILD ?= debug

# Sources / Objects
SRC = minecraft.c player.c timer.c  hitresult.c user.c options.c \
      level/chunk.c level/level.c level/level_renderer.c \
      level/tile/tile.c phys/aabb.c entity.c character/zombie.c \
      character/polygon.c character/cube.c \
      level/levelgen/level_gen.c \
      level/levelgen/synth/synth.c level/levelgen/synth/improved_noise.c \
      level/levelgen/synth/perlin_noise.c level/levelgen/synth/distort.c \
      particle/particle_engine.c particle/particle.c \
      character/zombie_model.c renderer/textures.c renderer/tessellator.c \
      renderer/texture_fx.c \
      renderer/frustum.c gui/font.c gui/screen.c gui/pause_screen.c \
      gui/generate_level_screen.c gui/chat_input_screen.c gui/message_screen.c \
      gui/inventory_screen.c gui/options_screen.c gui/controls_screen.c \
      net/net_socket.c net/packet.c net/connection.c net/network_player.c \
      audio/sound.c

# stb_vorbis.c is intentionally not listed here: sound.c #includes it
# directly as a single translation unit (the common way to use it), so
# compiling it again as its own object would just double define every symbol

# comm/socket_connection.c comm/socket_server.c server/client.c
# server/minecraft_server.c are a faithful port of the multiplayer classes,
# matching the real client, which never wires them up either in this version

# Platform detect
UNAME_S := $(shell uname -s)

# Common flags
CFLAGS  := $(CSTD) $(WARN) $(INCLUDE)
DEPFLAGS := -MMD -MP

# Per-build-type flags
ifeq ($(BUILD),release)
    CFLAGS  += -O2 -DNDEBUG
else
    CFLAGS  += -O0 -g
endif

# Linker flags / output name per-OS
ifeq ($(UNAME_S),Linux)
    EXE     := minecraft
    # -lpthread: c0.0.17a's connect is a background thread now, matching
    # net/c.java becoming a Thread subclass. -lasound: c0.0.23a_01's audio
    # backend (audio/audio_backend_alsa.c, NOT tested on real Linux hardware,
    # see the file's own header comment)
    SRC     += audio/audio_backend_alsa.c
    LDFLAGS := -lglfw -lGLEW -lGL -lGLU -lm -lz -lpthread -lasound
endif

ifeq ($(UNAME_S),Darwin)
    EXE     := minecraft
    # Homebrew glfw/glew: -lglfw -lGLEW, OpenGL(+GLU) comes via framework.
    # AudioToolbox: c0.0.23a_01's audio backend (audio_backend_coreaudio.c,
    # NOT tested on a real Mac, see the file's own header comment)
    SRC     += audio/audio_backend_coreaudio.c
    LDFLAGS := -lglfw -lGLEW -framework OpenGL -framework AudioToolbox -lz -lpthread
endif

ifeq ($(OS),Windows_NT)
    EXE := minecraft.exe
    # MSYS2 / MinGW (assumes -lglfw3 -lglew32 in your env). -lwinmm: needed
    # for timeBeginPeriod (100fps cap granularity) and now also for
    # c0.0.23a_01's audio backend (audio_backend_win.c, WinMM waveOut)
    SRC += audio/audio_backend_win.c
    ifeq ($(BUILD),release)
        LDFLAGS := -lglfw3 -lglew32 -lopengl32 -lglu32 -lz -lws2_32 -lwinmm -mwindows
    else
        LDFLAGS := -lglfw3 -lglew32 -lopengl32 -lglu32 -lz -lws2_32 -lwinmm
    endif
endif

# Computed after the platform blocks above, since those append the platform
# specific audio backend file to SRC via +=
OBJ := $(SRC:.c=.o)
DEP := $(OBJ:.o=.d)

# Default target
.PHONY: all
all: $(EXE)

$(EXE): $(OBJ)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# Compile C -> OBJ with depgen
%.o: %.c
	$(CC) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

# Convenience targets
.PHONY: debug release run clean
debug:
	$(MAKE) BUILD=debug
release:
	$(MAKE) BUILD=release
run: $(EXE)
	./$(EXE)

clean:
	@rm -f $(EXE) $(OBJ) $(DEP) 2>/dev/null || true

# Include generated deps (if present)
-include $(DEP)
