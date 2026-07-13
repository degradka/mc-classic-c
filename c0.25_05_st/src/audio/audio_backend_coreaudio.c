// audio_backend_coreaudio.c: macOS PCM output via the AudioQueue API, the
// simplest CoreAudio playback path (roughly WinMM waveOut's counterpart).
//
// NOT tested on a real Mac. Written directly against Apple's documented
// AudioQueueNewOutput/AudioQueueEnqueueBuffer usage pattern, but this file
// has not been compiled or run on actual macOS. If audio doesn't work here,
// this is the first place to check.

#include "audio_backend.h"
#include <AudioToolbox/AudioToolbox.h>
#include <string.h>

#define NUM_BUFFERS   4
#define BUFFER_FRAMES 1024

static AudioQueueRef gQueue;
static AudioQueueBufferRef gBuffers[NUM_BUFFERS];
static AudioFillCallback gFill;

static void queueCallback(void* userData, AudioQueueRef queue, AudioQueueBufferRef buffer) {
    (void)userData;
    int frames = BUFFER_FRAMES;
    gFill((int16_t*)buffer->mAudioData, frames);
    buffer->mAudioDataByteSize = (UInt32)(frames * AUDIO_CHANNELS * sizeof(int16_t));
    AudioQueueEnqueueBuffer(queue, buffer, 0, NULL);
}

int AudioBackend_init(AudioFillCallback fill) {
    gFill = fill;

    AudioStreamBasicDescription fmt;
    memset(&fmt, 0, sizeof fmt);
    fmt.mSampleRate       = AUDIO_SAMPLE_RATE;
    fmt.mFormatID         = kAudioFormatLinearPCM;
    fmt.mFormatFlags      = kLinearPCMFormatFlagIsSignedInteger | kLinearPCMFormatFlagIsPacked;
    fmt.mBitsPerChannel   = 16;
    fmt.mChannelsPerFrame = AUDIO_CHANNELS;
    fmt.mBytesPerFrame    = AUDIO_CHANNELS * sizeof(int16_t);
    fmt.mFramesPerPacket  = 1;
    fmt.mBytesPerPacket   = fmt.mBytesPerFrame;

    if (AudioQueueNewOutput(&fmt, queueCallback, NULL, NULL, NULL, 0, &gQueue) != noErr) {
        gQueue = NULL;
        return -1;
    }

    UInt32 bufferBytes = BUFFER_FRAMES * AUDIO_CHANNELS * sizeof(int16_t);
    for (int i = 0; i < NUM_BUFFERS; ++i) {
        if (AudioQueueAllocateBuffer(gQueue, bufferBytes, &gBuffers[i]) != noErr) {
            gQueue = NULL;
            return -1;
        }
        gBuffers[i]->mAudioDataByteSize = bufferBytes;
        memset(gBuffers[i]->mAudioData, 0, bufferBytes);
        queueCallback(NULL, gQueue, gBuffers[i]);
    }

    if (AudioQueueStart(gQueue, NULL) != noErr) {
        return -1;
    }
    return 0;
}

void AudioBackend_shutdown(void) {
    if (!gQueue) return;
    AudioQueueStop(gQueue, true);
    AudioQueueDispose(gQueue, true);
    gQueue = NULL;
}
