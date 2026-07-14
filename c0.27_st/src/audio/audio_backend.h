// audio_backend.h: thin per platform PCM output interface, matching the
// old "one native driver per target" split (id Software's i_sound.c
// pattern), rather than pulling in a cross platform audio engine. sound.c's
// mixer is the only client, and is entirely platform agnostic itself: it
// just asks to be called back whenever the backend wants more samples

#ifndef AUDIO_BACKEND_H
#define AUDIO_BACKEND_H

#include <stdint.h>

#define AUDIO_SAMPLE_RATE 44100
#define AUDIO_CHANNELS    2

// Fills `frames` stereo interleaved 16 bit samples into `buffer` (buffer
// holds frames*AUDIO_CHANNELS int16_t values). Called from whatever thread
// the backend's own native API drives its playback callback on
typedef void (*AudioFillCallback)(int16_t* buffer, int frames);

// Starts the platform audio device. Returns 0 on success, non zero if the
// device could not be opened (sound stays silent, nothing else fails)
int  AudioBackend_init(AudioFillCallback fill);
void AudioBackend_shutdown(void);

#endif
