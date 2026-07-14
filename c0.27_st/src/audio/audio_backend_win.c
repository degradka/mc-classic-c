// audio_backend_win.c: Windows PCM output via WinMM waveOut. No COM, no
// device enumeration, matches the same low dependency spirit as this port's
// existing winmm use (timeBeginPeriod in timer.c). waveOutOpen's own
// CALLBACK_FUNCTION mode drives buffer refills from a hidden worker thread
// winmm manages internally, so no thread is created here directly.

#include "audio_backend.h"
#include <windows.h>
#include <mmsystem.h>
#include <string.h>

#define NUM_BUFFERS   4
#define BUFFER_FRAMES 1024

static HWAVEOUT gWaveOut;
static WAVEHDR gHeaders[NUM_BUFFERS];
static int16_t gBuffers[NUM_BUFFERS][BUFFER_FRAMES * AUDIO_CHANNELS];
static AudioFillCallback gFill;
static volatile LONG gRunning;

static void CALLBACK waveOutProc(HWAVEOUT hwo, UINT uMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2) {
    (void)hwo; (void)dwInstance; (void)dwParam2;
    if (uMsg != WOM_DONE || !gRunning) return;
    WAVEHDR* hdr = (WAVEHDR*)dwParam1;
    gFill((int16_t*)hdr->lpData, BUFFER_FRAMES);
    waveOutWrite(gWaveOut, hdr, sizeof(WAVEHDR));
}

int AudioBackend_init(AudioFillCallback fill) {
    gFill = fill;

    WAVEFORMATEX fmt;
    memset(&fmt, 0, sizeof fmt);
    fmt.wFormatTag      = WAVE_FORMAT_PCM;
    fmt.nChannels       = AUDIO_CHANNELS;
    fmt.nSamplesPerSec  = AUDIO_SAMPLE_RATE;
    fmt.wBitsPerSample  = 16;
    fmt.nBlockAlign     = (WORD)(fmt.nChannels * fmt.wBitsPerSample / 8);
    fmt.nAvgBytesPerSec = fmt.nSamplesPerSec * fmt.nBlockAlign;

    if (waveOutOpen(&gWaveOut, WAVE_MAPPER, &fmt, (DWORD_PTR)waveOutProc, 0, CALLBACK_FUNCTION) != MMSYSERR_NOERROR) {
        gWaveOut = NULL;
        return -1;
    }

    gRunning = 1;
    for (int i = 0; i < NUM_BUFFERS; ++i) {
        memset(&gHeaders[i], 0, sizeof(WAVEHDR));
        gFill(gBuffers[i], BUFFER_FRAMES);
        gHeaders[i].lpData         = (LPSTR)gBuffers[i];
        gHeaders[i].dwBufferLength = BUFFER_FRAMES * AUDIO_CHANNELS * sizeof(int16_t);
        waveOutPrepareHeader(gWaveOut, &gHeaders[i], sizeof(WAVEHDR));
        waveOutWrite(gWaveOut, &gHeaders[i], sizeof(WAVEHDR));
    }
    return 0;
}

void AudioBackend_shutdown(void) {
    gRunning = 0;
    if (gWaveOut) {
        waveOutReset(gWaveOut);
        for (int i = 0; i < NUM_BUFFERS; ++i) waveOutUnprepareHeader(gWaveOut, &gHeaders[i], sizeof(WAVEHDR));
        waveOutClose(gWaveOut);
        gWaveOut = NULL;
    }
}
