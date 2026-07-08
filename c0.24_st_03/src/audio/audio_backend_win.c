// audio/audio_backend_win.c: windows pcm output through waveOut, refilled from a thread of its own

#include "audio_backend.h"
#include <windows.h>
#include <mmsystem.h>
#include <string.h>

// the mixer writes 4410 frames at a time into a line that holds a quarter of that
#define NUM_BUFFERS   2
#define BUFFER_FRAMES 4410

static HWAVEOUT waveOut;
static WAVEHDR headers[NUM_BUFFERS];
static int16_t buffers[NUM_BUFFERS][BUFFER_FRAMES * AUDIO_CHANNELS];
static AudioFillCallback fillBuffer;
static HANDLE doneEvent;
static HANDLE thread;
static volatile LONG running;

static DWORD WINAPI audioThread(LPVOID arg) {
    (void)arg;
    while (running) {
        WaitForSingleObject(doneEvent, INFINITE);
        for (int i = 0; i < NUM_BUFFERS && running; ++i) {
            if (!(headers[i].dwFlags & WHDR_DONE)) continue;
            fillBuffer(buffers[i], BUFFER_FRAMES);
            waveOutWrite(waveOut, &headers[i], sizeof(WAVEHDR));
        }
    }
    return 0;
}

int AudioBackend_init(AudioFillCallback fill) {
    fillBuffer = fill;
    WAVEFORMATEX fmt;
    memset(&fmt, 0, sizeof fmt);
    fmt.wFormatTag      = WAVE_FORMAT_PCM;
    fmt.nChannels       = AUDIO_CHANNELS;
    fmt.nSamplesPerSec  = AUDIO_SAMPLE_RATE;
    fmt.wBitsPerSample  = 16;
    fmt.nBlockAlign     = (WORD)(fmt.nChannels * fmt.wBitsPerSample / 8);
    fmt.nAvgBytesPerSec = fmt.nSamplesPerSec * fmt.nBlockAlign;

    doneEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (!doneEvent) return -1;
    if (waveOutOpen(&waveOut, WAVE_MAPPER, &fmt, (DWORD_PTR)doneEvent, 0, CALLBACK_EVENT) != MMSYSERR_NOERROR) {
        CloseHandle(doneEvent);
        waveOut = NULL;
        return -1;
    }
    running = 1;
    for (int i = 0; i < NUM_BUFFERS; ++i) {
        memset(&headers[i], 0, sizeof(WAVEHDR));
        headers[i].lpData = (LPSTR)buffers[i];
        headers[i].dwBufferLength = BUFFER_FRAMES * AUDIO_CHANNELS * sizeof(int16_t);
        waveOutPrepareHeader(waveOut, &headers[i], sizeof(WAVEHDR));
        fillBuffer(buffers[i], BUFFER_FRAMES);
        waveOutWrite(waveOut, &headers[i], sizeof(WAVEHDR));
    }
    thread = CreateThread(NULL, 0, audioThread, NULL, 0, NULL);
    if (!thread) {
        AudioBackend_shutdown();
        return -1;
    }
    return 0;
}

void AudioBackend_shutdown(void) {
    running = 0;
    if (thread) {
        SetEvent(doneEvent);
        WaitForSingleObject(thread, INFINITE);
        CloseHandle(thread);
        thread = NULL;
    }
    if (waveOut) {
        waveOutReset(waveOut);
        for (int i = 0; i < NUM_BUFFERS; ++i) waveOutUnprepareHeader(waveOut, &headers[i], sizeof(WAVEHDR));
        waveOutClose(waveOut);
        waveOut = NULL;
    }
    if (doneEvent) {
        CloseHandle(doneEvent);
        doneEvent = NULL;
    }
}
