// audio_backend_alsa.c: Linux PCM output via ALSA's plain snd_pcm_* API.
//
// NOT tested on real Linux hardware. Written directly against ALSA's
// documented blocking-write usage pattern (open in SND_PCM_STREAM_PLAYBACK,
// set the usual hw params, then a dedicated thread loop of fill+writei),
// which is the standard minimal way to drive ALSA, but this file has not
// been compiled or run on an actual Linux machine. If audio doesn't work
// here, this is the first place to check.

#include "audio_backend.h"
#include <alsa/asoundlib.h>
#include <pthread.h>
#include <string.h>

#define PERIOD_FRAMES 1024

static snd_pcm_t* gPcm;
static pthread_t gThread;
static AudioFillCallback gFill;
static volatile int gRunning;

static void* audioThreadMain(void* arg) {
    (void)arg;
    int16_t buffer[PERIOD_FRAMES * AUDIO_CHANNELS];

    while (gRunning) {
        gFill(buffer, PERIOD_FRAMES);
        snd_pcm_sframes_t written = snd_pcm_writei(gPcm, buffer, PERIOD_FRAMES);
        if (written < 0) {
            written = snd_pcm_recover(gPcm, (int)written, 1);
            if (written < 0) break;
        }
    }
    return NULL;
}

int AudioBackend_init(AudioFillCallback fill) {
    gFill = fill;

    if (snd_pcm_open(&gPcm, "default", SND_PCM_STREAM_PLAYBACK, 0) < 0) {
        gPcm = NULL;
        return -1;
    }

    unsigned int rate = AUDIO_SAMPLE_RATE;
    if (snd_pcm_set_params(gPcm, SND_PCM_FORMAT_S16_LE, SND_PCM_ACCESS_RW_INTERLEAVED,
                            AUDIO_CHANNELS, rate, 1, 100000 /* 100ms latency */) < 0) {
        snd_pcm_close(gPcm);
        gPcm = NULL;
        return -1;
    }

    gRunning = 1;
    if (pthread_create(&gThread, NULL, audioThreadMain, NULL) != 0) {
        gRunning = 0;
        snd_pcm_close(gPcm);
        gPcm = NULL;
        return -1;
    }
    return 0;
}

void AudioBackend_shutdown(void) {
    if (!gPcm) return;
    gRunning = 0;
    pthread_join(gThread, NULL);
    snd_pcm_close(gPcm);
    gPcm = NULL;
}
