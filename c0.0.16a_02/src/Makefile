# Makefile: minecraft for linux, macos and windows (mingw)

CC      := gcc
CSTD    := -std=c99
WARN    := -Wall -Wextra
INCLUDE := -I.

# debug (default) or release
BUILD ?= debug

SRC = minecraft.c player.c timer.c  hitresult.c jrandom.c java_sort.c \
      level/chunk.c level/level.c level/level_io.c level/javaser.c jgzip.c level/level_renderer.c \
      level/tile/tile.c phys/aabb.c entity.c character/zombie.c \
      character/polygon.c character/cube.c \
      level/levelgen/level_gen.c \
      level/levelgen/synth/synth.c level/levelgen/synth/improved_noise.c \
      level/levelgen/synth/perlin_noise.c level/levelgen/synth/distort.c \
      particle/particle_engine.c particle/particle.c \
      character/zombie_model.c renderer/textures.c renderer/tessellator.c \
      renderer/frustum.c gui/font.c gui/screen.c gui/pause_screen.c gui/new_level_screen.c

OBJ := $(SRC:.c=.o)
DEP := $(OBJ:.o=.d)

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
    LDFLAGS := -lglfw -lGLEW -lGL -lGLU -lm -lz
endif

ifeq ($(UNAME_S),Darwin)
    EXE     := minecraft
    # Homebrew glfw/glew: -lglfw -lGLEW, OpenGL(+GLU) comes via framework
    LDFLAGS := -lglfw -lGLEW -framework OpenGL -lz
endif

ifeq ($(OS),Windows_NT)
    EXE := minecraft.exe
    # MSYS2 / MinGW (assumes -lglfw3 -lglew32 in your env)
    ifeq ($(BUILD),release)
        LDFLAGS := -lglfw3 -lglew32 -lopengl32 -lglu32 -lz -mwindows
    else
        LDFLAGS := -lglfw3 -lglew32 -lopengl32 -lglu32 -lz
    endif
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
