// audio/audio_backend.h: thin per platform PCM output interface for the audio mixer

#ifndef AUDIO_BACKEND_H
#define AUDIO_BACKEND_H

#include <stdint.h>

#define AUDIO_SAMPLE_RATE 44100
#define AUDIO_CHANNELS    2

// fills `frames` stereo interleaved 16 bit samples into `buffer`
// called from whatever thread the backend's own native API drives its playback callback on
typedef void (*AudioFillCallback)(int16_t* buffer, int frames);

// starts the platform audio device
// failure disables sound
int  AudioBackend_init(AudioFillCallback fill);
void AudioBackend_shutdown(void);

#endif
