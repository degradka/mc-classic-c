// audio/sound.h: step sounds and music mixed in 4410 frame chunks

#ifndef SOUND_H
#define SOUND_H

#include <stdbool.h>

struct Entity;
struct Options;

// loads the sounds and music from resources, opens the audio device and starts its threads
void Sound_init(const struct Options* options);
void Sound_shutdown(void);

// Level.playSound, the sound follows the entity while it plays
void Sound_playEntity(const char* name, struct Entity* source, const struct Entity* listener, float volume, float pitch);
// Level.playSound at a fixed spot
void Sound_playAt(const char* name, float x, float y, float z, const struct Entity* listener, float volume, float pitch);

// the music check at the start of every tick
void Sound_tick(void);

// an entity stays alive while a sound holds it, the level frees its zombies through this
void Sound_releaseEntity(struct Entity* e);

#endif
