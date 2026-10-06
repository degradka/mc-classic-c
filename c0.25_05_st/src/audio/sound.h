// audio/sound.h: footstep/break sounds and music

#ifndef SOUND_H
#define SOUND_H

#include <stdbool.h>
#include "../entity.h"

void Sound_init(bool musicEnabled, bool soundEnabled);
void Sound_shutdown(void);
void Sound_setEnabled(bool music, bool sound);

// once per frame, copies the listener and every moving source for the mixer thread
void Sound_update(const Entity* listener);
// a source entity moved to another slot, or was removed (to = NULL keeps its last spot)
void Sound_retargetEntity(const Entity* from, const Entity* to);
void Sound_detachAll(void);

// Level.playSound at a fixed spot and following an entity
void Sound_playAt(const char* name, float x, float y, float z, float volume, float pitch);
void Sound_playEntity(const char* name, const Entity* e, float volume, float pitch);

// the music timer, once per tick
void Sound_tickMusic(void);

#endif
