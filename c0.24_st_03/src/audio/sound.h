// sound.h: footstep/break sound + music playback, on top of the platform
// specific audio_backend. Owns its own small fixed size voice mixer (matching
// the real client's own from scratch mixing rather than delegating to a
// generic audio engine's spatializer), decoding OGG files via stb_vorbis.

#ifndef SOUND_H
#define SOUND_H

#include <stdbool.h>

void Sound_init(bool musicEnabled, bool soundEnabled);
void Sound_shutdown(void);
void Sound_setEnabled(bool music, bool sound);

// Updates the listener (local player camera) position/yaw used to compute
// pan and distance falloff for every Sound_play call until the next update.
// Call once per rendered frame.
void Sound_setListener(float x, float y, float z, float yawDegrees);

// Plays "step.<soundtype>" (the only sound family this version has) once, at
// world position (x,y,z), gated on the sound option. volume/pitch are
// expected to already include the caller's own randomization (matching
// SoundType_getVolume/getPitch being called at each real playSound call
// site, not baked into a static table).
void Sound_play(const char* name, float volume, float pitch, float x, float y, float z);

// Drives the music scheduler (calm1/2/3.ogg, replayed randomly every
// 5-20 minutes). Call once per game tick (20/sec).
void Sound_tickMusic(void);

#endif
