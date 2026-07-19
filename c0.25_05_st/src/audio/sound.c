// sound.c: fixed size voice mixer + OGG loading (stb_vorbis) + music
// scheduler, sitting on top of the platform specific audio_backend.

#include "sound.h"
#include "audio_backend.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// stb_vorbis.c defines single letter L/C/R and PLAYBACK_MONO/LEFT/RIGHT
// macros for its channel mapping table and never undefs them afterward, a
// known upstream wart (nothings/stb issue #282) that collides with same
// named macros already pulled in by MinGW's Windows headers on some
// toolchains. Clearing them right before the include neutralizes whatever
// is already defined, and clearing them again after stops the leak from
// reaching the rest of this file
#undef L
#undef C
#undef R
#undef PLAYBACK_MONO
#undef PLAYBACK_LEFT
#undef PLAYBACK_RIGHT
#include "../stb_vorbis.c"
#undef L
#undef C
#undef R
#undef PLAYBACK_MONO
#undef PLAYBACK_LEFT
#undef PLAYBACK_RIGHT

#ifdef _WIN32
  #include <windows.h>
  #define SOUND_LOCK_T       CRITICAL_SECTION
  #define SOUND_LOCK_INIT(l) InitializeCriticalSection(l)
  #define SOUND_LOCK(l)      EnterCriticalSection(l)
  #define SOUND_UNLOCK(l)    LeaveCriticalSection(l)
#else
  #include <pthread.h>
  #define SOUND_LOCK_T       pthread_mutex_t
  #define SOUND_LOCK_INIT(l) pthread_mutex_init(l, NULL)
  #define SOUND_LOCK(l)      pthread_mutex_lock(l)
  #define SOUND_UNLOCK(l)    pthread_mutex_unlock(l)
#endif

static SOUND_LOCK_T gLock;

// c0.0.23a_01: this version has exactly one sound effect family ("step.",
// covering both footsteps and block breaking) and one music base name
// ("calm"). Variant counts match what's actually present under resources/
// (sourced by hand), not a directory scan like the
// real client's resource manifest grouping, since there's nothing else to
// group here yet
#define STEP_VARIANT_COUNT  4
#define MUSIC_VARIANT_COUNT 3

#define MAX_VOICES     32
#define MAX_SFX_CACHE  16 // grass/gravel/stone/wood x4 variants each

typedef struct {
    char path[64];
    int16_t* samples; // interleaved, decoded channel count
    int channels;
    int frames;
    int sampleRate;
} SfxClip;

static SfxClip gSfxCache[MAX_SFX_CACHE];
static int gSfxCacheCount = 0;

typedef struct {
    bool active;
    const int16_t* samples;
    int channels;
    int frames;
    float pos;       // fractional frame position
    float step;      // advance per output frame (pitch * fileRate/deviceRate)
    float volume;
    float pan;       // -1..1
} Voice;

static Voice gVoices[MAX_VOICES];

// music (single active track, fully decoded/replaced on each scheduled pick,
// owns its own buffer separate from the sfx cache since tracks are large and
// only one is ever needed alive at a time)
static int16_t* gMusicSamples = NULL;
static int gMusicChannels = 0, gMusicFrames = 0, gMusicSampleRate = 0;
static float gMusicPos = 0.0f;
static bool gMusicPlaying = false;
static int gMusicTicksRemaining = 0;
// decoding a multi minute ogg file (especially in a debug, unoptimized
// build) takes long enough to be a visible multi second freeze if done on
// the main thread inside Sound_tickMusic, so it happens on a background
// thread instead (same pattern as this project's own async connect thread
// in net/connection.c); this flag stops two decodes from overlapping
static volatile int gMusicLoadInFlight = 0;

static bool gMusicEnabled = true;
static bool gSoundEnabled = true;
static bool gBackendOk = false;

static float gListenerX, gListenerY, gListenerZ, gListenerYaw;

static const SfxClip* loadSfx(const char* path) {
    for (int i = 0; i < gSfxCacheCount; ++i) {
        if (strcmp(gSfxCache[i].path, path) == 0) return &gSfxCache[i];
    }
    if (gSfxCacheCount >= MAX_SFX_CACHE) return NULL;

    int channels = 0, sampleRate = 0;
    short* output = NULL;
    int frames = stb_vorbis_decode_filename(path, &channels, &sampleRate, &output);
    if (frames <= 0 || !output) return NULL;

    SfxClip* clip = &gSfxCache[gSfxCacheCount++];
    snprintf(clip->path, sizeof clip->path, "%s", path);
    clip->samples = output;
    clip->channels = channels;
    clip->frames = frames;
    clip->sampleRate = sampleRate;
    return clip;
}

// mixes one voice's remaining samples into `out` (frames stereo pairs),
// converting mono sources to stereo via the same left/right gain, advancing
// its fractional read position by `step` each output frame (this is how
// pitch is applied: step==1.0 at native rate, >1.0 plays faster/higher)
static void mixVoice(int16_t* out, int frames, const int16_t* samples, int channels,
                     int totalFrames, float* pos, float step, float volume, float pan)
{
    // equal power pan law, pan -1(left)..0(center)..1(right)
    float leftGain  = cosf((pan + 1.0f) * 0.25f * (float)M_PI) * volume;
    float rightGain = sinf((pan + 1.0f) * 0.25f * (float)M_PI) * volume;

    for (int i = 0; i < frames; ++i) {
        int idx = (int)*pos;
        if (idx >= totalFrames - 1) break;

        float src;
        if (channels == 1) {
            src = (float)samples[idx];
        } else {
            src = ((float)samples[idx * 2] + (float)samples[idx * 2 + 1]) * 0.5f;
        }

        int l = out[i * 2]     + (int)(src * leftGain);
        int r = out[i * 2 + 1] + (int)(src * rightGain);
        if (l >  32767) l =  32767;
        if (l < -32768) l = -32768;
        if (r >  32767) r =  32767;
        if (r < -32768) r = -32768;
        out[i * 2]     = (int16_t)l;
        out[i * 2 + 1] = (int16_t)r;

        *pos += step;
    }
}

static void fillCallback(int16_t* buffer, int frames) {
    memset(buffer, 0, (size_t)frames * AUDIO_CHANNELS * sizeof(int16_t));

    SOUND_LOCK(&gLock);

    if (gMusicPlaying && gMusicSamples) {
        float step = (float)gMusicSampleRate / (float)AUDIO_SAMPLE_RATE;
        // c0.0.23a_01: confirmed against b/m.java, the real source's music
        // mixer adds decoded samples straight into the output buffer with no
        // volume scaling at all (full, unattenuated gain), not toned down
        // for being background music. volume 1.0 here matches that; pan 0.0
        // still goes through this mixer's own equal power pan law (a ~3dB
        // center dip), which is this port's own mixing design, not a source
        // behavior, and applies to every voice uniformly, not just music
        mixVoice(buffer, frames, gMusicSamples, gMusicChannels, gMusicFrames, &gMusicPos, step, 1.0f, 0.0f);
        if ((int)gMusicPos >= gMusicFrames - 1) {
            gMusicPlaying = false;
        }
    }

    for (int i = 0; i < MAX_VOICES; ++i) {
        Voice* v = &gVoices[i];
        if (!v->active) continue;
        mixVoice(buffer, frames, v->samples, v->channels, v->frames, &v->pos, v->step, v->volume, v->pan);
        if ((int)v->pos >= v->frames - 1) v->active = false;
    }

    SOUND_UNLOCK(&gLock);
}

void Sound_init(bool musicEnabled, bool soundEnabled) {
    SOUND_LOCK_INIT(&gLock);
    gMusicEnabled = musicEnabled;
    gSoundEnabled = soundEnabled;
    gBackendOk = (AudioBackend_init(fillCallback) == 0);
    // matches the real source exactly: SoundManager's own deadline field is
    // initialized to `now + 60000L` in its constructor, a fixed first wait
    // (not the random 5-20 min formula used for every reschedule after that)
    gMusicTicksRemaining = 1200;
}

void Sound_shutdown(void) {
    if (gBackendOk) AudioBackend_shutdown();
    for (int i = 0; i < gSfxCacheCount; ++i) free(gSfxCache[i].samples);
    gSfxCacheCount = 0;
    free(gMusicSamples);
    gMusicSamples = NULL;
}

void Sound_setEnabled(bool music, bool sound) {
    gMusicEnabled = music;
    gSoundEnabled = sound;
}

void Sound_setListener(float x, float y, float z, float yawDegrees) {
    gListenerX = x; gListenerY = y; gListenerZ = z;
    gListenerYaw = yawDegrees;
}

void Sound_play(const char* name, float volume, float pitch, float x, float y, float z) {
    if (!gBackendOk || !gSoundEnabled) return;
    if (strncmp(name, "step.", 5) != 0) return;
    const char* base = name + 5;

    char path[64];
    int variant = 1 + rand() % STEP_VARIANT_COUNT;
    snprintf(path, sizeof path, "resources/sound/step/%s%d.ogg", base, variant);

    const SfxClip* clip = loadSfx(path);
    if (!clip) return;

    // distance falloff (silent past 32 blocks) and stereo pan relative to the
    // listener's own facing. This is a from scratch approximation of the
    // real client's positional audio math (not a line for line port of its
    // panner), since this port's own mixer already replaces the real
    // client's OpenAL style backend outright
    float dx = x - gListenerX, dy = y - gListenerY, dz = z - gListenerZ;
    float dist = sqrtf(dx * dx + dy * dy + dz * dz);
    float falloff = 1.0f - dist / 32.0f;
    if (falloff <= 0.0f) return;
    if (falloff > 1.0f) falloff = 1.0f;

    float yawRad = gListenerYaw * (float)M_PI / 180.0f;
    float angle = atan2f(dx, dz) - yawRad;
    float pan = sinf(angle);

    SOUND_LOCK(&gLock);
    for (int i = 0; i < MAX_VOICES; ++i) {
        if (gVoices[i].active) continue;
        Voice* v = &gVoices[i];
        v->samples = clip->samples;
        v->channels = clip->channels;
        v->frames = clip->frames;
        v->pos = 0.0f;
        v->step = pitch * ((float)clip->sampleRate / (float)AUDIO_SAMPLE_RATE);
        v->volume = volume * falloff;
        v->pan = pan;
        v->active = true;
        break;
    }
    SOUND_UNLOCK(&gLock);
}

// path is chosen by the caller (main thread), not in here: MinGW/MSVCRT's
// rand() keeps separate state per thread, so calling it fresh on a brand new
// thread every time (this function runs on one) always draws the same first
// value regardless of how much the main thread's own rand() stream has
// advanced, which is why every track picked this way came out identical
static char gPendingMusicPath[64];

// runs on a background thread: the actual decode never touches gLock, only
// the final buffer swap does, so the main thread is never blocked waiting
// on a multi second decode
static void runMusicLoad(void) {
    char path[64];
    snprintf(path, sizeof path, "%s", gPendingMusicPath);

    int channels = 0, sampleRate = 0;
    short* output = NULL;
    int frames = stb_vorbis_decode_filename(path, &channels, &sampleRate, &output);
    if (frames <= 0 || !output) {
        gMusicLoadInFlight = 0;
        return;
    }

    SOUND_LOCK(&gLock);
    free(gMusicSamples);
    gMusicSamples = output;
    gMusicChannels = channels;
    gMusicFrames = frames;
    gMusicSampleRate = sampleRate;
    gMusicPos = 0.0f;
    gMusicPlaying = true;
    SOUND_UNLOCK(&gLock);
    gMusicLoadInFlight = 0;
}

#ifdef _WIN32
static DWORD WINAPI musicLoadThreadMain(LPVOID arg) { (void)arg; runMusicLoad(); return 0; }
#else
static void* musicLoadThreadMain(void* arg) { (void)arg; runMusicLoad(); return NULL; }
#endif

static void startRandomMusicTrack(void) {
    if (gMusicLoadInFlight) return; // previous decode still running, try again next schedule
    gMusicLoadInFlight = 1;

    // picked here, on the main thread, not inside runMusicLoad (see the
    // comment above gPendingMusicPath)
    int variant = 1 + rand() % MUSIC_VARIANT_COUNT;
    snprintf(gPendingMusicPath, sizeof gPendingMusicPath, "resources/music/calm%d.ogg", variant);

#ifdef _WIN32
    HANDLE h = CreateThread(NULL, 0, musicLoadThreadMain, NULL, 0, NULL);
    if (h) CloseHandle(h); // detached: nothing needs to join it
    else gMusicLoadInFlight = 0;
#else
    pthread_t t;
    if (pthread_create(&t, NULL, musicLoadThreadMain, NULL) == 0) pthread_detach(t);
    else gMusicLoadInFlight = 0;
#endif
}

void Sound_tickMusic(void) {
    if (!gBackendOk || !gMusicEnabled) return;
    if (--gMusicTicksRemaining > 0) return;

    // 300000 + random(0,900000) ms, in 50ms ticks: 6000 + random(0,18000)
    gMusicTicksRemaining = 6000 + rand() % 18000;
    startRandomMusicTrack();
}
