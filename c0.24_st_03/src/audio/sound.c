// audio/sound.c: the mixer (SoundPlayer, Sound, SoundInfo, Music) on
// top of the platform specific audio_backend, OGG decoding via stb_vorbis

#include "sound.h"
#include "audio_backend.h"
#include "../jrandom.h"
#include "../timer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// stb_vorbis.c defines L/C/R and PLAYBACK_MONO/LEFT/RIGHT
// macros for its channel table and never undefs them
// after (nothings/stb/issues/282) those names clash with
// MinGW Windows headers so clear them before + after the include
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

#define STEP_VARIANT_COUNT  4
#define MUSIC_VARIANT_COUNT 3

#define MAX_VOICES     32
#define MAX_MUSIC      4
#define MAX_SFX_CACHE  16 // grass/gravel/stone/wood x4 variants each
#define CHUNK          4410

// SoundData, mono
typedef struct {
    char path[64];
    int16_t* data;
    int length;
    float rate;
} SfxClip;

static SfxClip gSfxCache[MAX_SFX_CACHE];
static int gSfxCacheCount = 0;

// Sound + SoundInfo + its SoundPos
typedef struct {
    bool active;
    const SfxClip* clip;
    float seek, step, volume;
    const Entity* source; // null for a fixed position
    float x, y, z;
    float pan, gain;      // what the last chunk ended on
} Voice;

static Voice gVoices[MAX_VOICES];

typedef struct {
    bool active;
    int16_t* samples; // interleaved stereo
    int frames, pos;
} MusicStream;

static MusicStream gMusic[MAX_MUSIC];
static volatile int gMusicLoadInFlight = 0;

static bool gMusicEnabled = true;
static bool gSoundEnabled = true;
static bool gBackendOk = false;

static const Entity* gListener = NULL;
static float gListenerX, gListenerY, gListenerZ, gListenerYaw;

static JRandom gRandom;          // SoundManager.random
static long long gLastMusic = 0;

static int gMixL[CHUNK], gMixR[CHUNK];
static int16_t gOut[CHUNK * 2];
static int gOutPos = CHUNK;

static const SfxClip* loadSfx(const char* path) {
    for (int i = 0; i < gSfxCacheCount; ++i) {
        if (strcmp(gSfxCache[i].path, path) == 0) return &gSfxCache[i];
    }
    if (gSfxCacheCount >= MAX_SFX_CACHE) return NULL;

    int channels = 0, sampleRate = 0;
    short* output = NULL;
    int frames = stb_vorbis_decode_filename(path, &channels, &sampleRate, &output);
    if (frames <= 0 || !output || channels <= 0) return NULL;

    // downmix to mono
    for (int i = 0; i < frames; ++i) {
        int sum = 0;
        for (int c = 0; c < channels; ++c) sum += output[i * channels + c];
        output[i] = (int16_t)(sum / channels);
    }

    SfxClip* clip = &gSfxCache[gSfxCacheCount++];
    snprintf(clip->path, sizeof clip->path, "%s", path);
    clip->data = output;
    clip->length = frames;
    clip->rate = (float)sampleRate;
    return clip;
}

// BaseSoundPos.getRotationDiff, NaN when the source is the listener itself
static float rotationDiff(float x, float z) {
    x -= gListenerX;
    z -= gListenerZ;
    float d = (float)sqrt(x * x + z * z);
    x /= d;
    z /= d;
    d /= 2.0f;
    if (d > 1.0f) d = 1.0f;
    float c = (float)cos((double)(-gListenerYaw) * M_PI / 180.0 + M_PI);
    float s = (float)sin((double)(-gListenerYaw) * M_PI / 180.0 + M_PI);
    return (s * z - c * x) * d;
}

// BaseSoundPos.getDistanceSq
static float distanceGain(float x, float y, float z) {
    float dz = z - gListenerZ;
    x -= gListenerX;
    y -= gListenerY;
    float d = (float)sqrt(x * x + y * y + dz * dz);
    d = 1.0f - d / 32.0f;
    if (d < 0.0f) d = 0.0f;
    return d;
}

// SoundInfo.update
static int readVoice(Voice* v, int16_t* out, int n) {
    const int16_t* data = v->clip->data;
    int len = v->clip->length;
    if (v->seek >= (float)len) return 0;
    for (int i = 0; i < n; ++i) {
        int k = (int)v->seek;
        int16_t s = data[k];
        int16_t s2 = k < len - 1 ? data[k + 1] : 0;
        out[i] = (int16_t)((float)s + (float)(s2 - s) * (v->seek - (float)k));
        v->seek += v->step;
        if (v->seek >= (float)len) return i;
    }
    return n;
}

// Sound.play
static bool mixVoice(Voice* v) {
    static int16_t data[CHUNK];
    int n = readVoice(v, data, CHUNK);
    bool alive = n > 0;
    float pan = rotationDiff(v->x, v->z);
    float gain = distanceGain(v->x, v->y, v->z) * v->volume;
    int l0 = (int)((v->pan > 0.0f ? 1.0f - v->pan : 1.0f) * v->gain * 65536.0f);
    int r0 = (int)((v->pan < 0.0f ? 1.0f + v->pan : 1.0f) * v->gain * 65536.0f);
    int dl = (int)((pan > 0.0f ? 1.0f - pan : 1.0f) * gain * 65536.0f) - l0;
    int dr = (int)((pan < 0.0f ? pan + 1.0f : 1.0f) * gain * 65536.0f) - r0;
    if (dl != 0 || dr != 0) {
        for (int i = 0; i < n; ++i) {
            int l = l0 + dl * i / CHUNK;
            int r = r0 + dr * i / CHUNK;
            gMixL[i] += data[i] * l >> 16;
            gMixR[i] += data[i] * r >> 16;
        }
    } else if (l0 >= 0 || r0 != 0) {
        for (int i = 0; i < n; ++i) {
            gMixL[i] += data[i] * l0 >> 16;
            gMixR[i] += data[i] * r0 >> 16;
        }
    }
    v->pan = pan;
    v->gain = gain;
    return alive;
}

// Music.play, raw stereo, disabling music discards active streams
static bool mixMusic(MusicStream* m) {
    if (!gMusicEnabled) return false;
    int n = m->frames - m->pos;
    if (n > CHUNK) n = CHUNK;
    for (int i = 0; i < n; ++i) {
        gMixL[i] += m->samples[(m->pos + i) * 2];
        gMixR[i] += m->samples[(m->pos + i) * 2 + 1];
    }
    m->pos += n;
    return m->pos < m->frames;
}

// SoundPlayer.run
static void renderChunk(void) {
    memset(gMixL, 0, sizeof gMixL);
    memset(gMixR, 0, sizeof gMixR);
    for (int i = 0; i < MAX_MUSIC; ++i) {
        MusicStream* m = &gMusic[i];
        if (m->active && !mixMusic(m)) {
            free(m->samples);
            m->samples = NULL;
            m->active = false;
        }
    }
    for (int i = 0; i < MAX_VOICES; ++i) {
        Voice* v = &gVoices[i];
        if (v->active && !mixVoice(v)) v->active = false;
    }
    bool silent = !gMusicEnabled && !gSoundEnabled;
    for (int i = 0; i < CHUNK; ++i) {
        int l = gMixL[i], r = gMixR[i];
        if (l < -32000) l = -32000;
        if (r < -32000) r = -32000;
        if (l >= 32000) l = 32000;
        if (r >= 32000) r = 32000;
        gOut[i * 2] = silent ? 0 : (int16_t)l;
        gOut[i * 2 + 1] = silent ? 0 : (int16_t)r;
    }
    gOutPos = 0;
}

static void fillCallback(int16_t* buffer, int frames) {
    SOUND_LOCK(&gLock);
    while (frames > 0) {
        if (gOutPos >= CHUNK) renderChunk();
        int n = CHUNK - gOutPos;
        if (n > frames) n = frames;
        memcpy(buffer, gOut + gOutPos * 2, (size_t)n * 2 * sizeof(int16_t));
        buffer += n * 2;
        gOutPos += n;
        frames -= n;
    }
    SOUND_UNLOCK(&gLock);
}

static void startMusicThread(void);

void Sound_init(bool musicEnabled, bool soundEnabled) {
    SOUND_LOCK_INIT(&gLock);
    gMusicEnabled = musicEnabled;
    gSoundEnabled = soundEnabled;
    JRandom_initRandom(&gRandom);
    gLastMusic = currentTimeMillis() + 60000LL;
    gBackendOk = (AudioBackend_init(fillCallback) == 0);
    if (gBackendOk) startMusicThread();
}

void Sound_shutdown(void) {
    if (gBackendOk) AudioBackend_shutdown();
    for (int i = 0; i < gSfxCacheCount; ++i) free(gSfxCache[i].data);
    gSfxCacheCount = 0;
    for (int i = 0; i < MAX_MUSIC; ++i) {
        free(gMusic[i].samples);
        gMusic[i].samples = NULL;
        gMusic[i].active = false;
    }
}

void Sound_setEnabled(bool music, bool sound) {
    gMusicEnabled = music;
    gSoundEnabled = sound;
}

static void copyListener(void) {
    if (!gListener) return;
    gListenerX = gListener->x;
    gListenerY = gListener->y;
    gListenerZ = gListener->z;
    gListenerYaw = gListener->yRotation;
}

void Sound_update(const Entity* listener) {
    SOUND_LOCK(&gLock);
    gListener = listener;
    copyListener();
    for (int i = 0; i < MAX_VOICES; ++i) {
        Voice* v = &gVoices[i];
        if (!v->active || !v->source) continue;
        v->x = v->source->x;
        v->y = v->source->y;
        v->z = v->source->z;
    }
    SOUND_UNLOCK(&gLock);
}

void Sound_retargetEntity(const Entity* from, const Entity* to) {
    SOUND_LOCK(&gLock);
    for (int i = 0; i < MAX_VOICES; ++i) {
        if (gVoices[i].active && gVoices[i].source == from) gVoices[i].source = to;
    }
    SOUND_UNLOCK(&gLock);
}

void Sound_detachAll(void) {
    SOUND_LOCK(&gLock);
    for (int i = 0; i < MAX_VOICES; ++i) gVoices[i].source = NULL;
    SOUND_UNLOCK(&gLock);
}

// SoundManager.getAudioInfo then SoundPlayer.play
static void play(const char* name, float volume, float pitch, const Entity* source, float x, float y, float z) {
    if (!gBackendOk || !gSoundEnabled) return;
    if (strncmp(name, "step.", 5) != 0) return;

    char path[64];
    int variant = 1 + JRandom_nextIntN(&gRandom, STEP_VARIANT_COUNT);
    snprintf(path, sizeof path, "resources/sound/step/%s%d.ogg", name + 5, variant);
    const SfxClip* clip = loadSfx(path);
    if (!clip) return;

    SOUND_LOCK(&gLock);
    // the live player is read here, so your own steps start centred
    copyListener();
    for (int i = 0; i < MAX_VOICES; ++i) {
        Voice* v = &gVoices[i];
        if (v->active) continue;
        v->clip = clip;
        v->seek = 0.0f;
        v->step = pitch * 44100.0f / clip->rate;
        v->volume = volume;
        v->source = source;
        v->x = x;
        v->y = y;
        v->z = z;
        v->pan = rotationDiff(x, z);
        v->gain = distanceGain(x, y, z) * volume;
        v->active = true;
        break;
    }
    SOUND_UNLOCK(&gLock);
}

void Sound_playAt(const char* name, float x, float y, float z, float volume, float pitch) {
    play(name, volume, pitch, NULL, x, y, z);
}

void Sound_playEntity(const char* name, const Entity* e, float volume, float pitch) {
    if (!gListener) return;
    float dx = e->x - gListener->x, dy = e->y - gListener->y, dz = e->z - gListener->z;
    if (dx * dx + dy * dy + dz * dz >= 1024.0f) return;
    play(name, volume, pitch, e, e->x, e->y, e->z);
}

// path is picked on the main thread, the decode runs on its own thread so a
// multi minute track doesn't freeze the game
static char gPendingMusicPath[64];

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
    if (channels == 1) {
        short* stereo = (short*)malloc((size_t)frames * 2 * sizeof(short));
        if (!stereo) { free(output); gMusicLoadInFlight = 0; return; }
        for (int i = 0; i < frames; ++i) stereo[i * 2] = stereo[i * 2 + 1] = output[i];
        free(output);
        output = stereo;
    }

    SOUND_LOCK(&gLock);
    int slot = -1;
    for (int i = 0; i < MAX_MUSIC; ++i) if (!gMusic[i].active) { slot = i; break; }
    if (slot >= 0) {
        gMusic[slot].samples = output;
        gMusic[slot].frames = frames;
        gMusic[slot].pos = 0;
        gMusic[slot].active = true;
    } else {
        free(output);
    }
    SOUND_UNLOCK(&gLock);
    gMusicLoadInFlight = 0;
}

// one music thread from startup, it gets handed the path
static volatile int gMusicPending = 0;

static void musicLoop(void) {
    for (;;) {
        if (gMusicPending) {
            gMusicPending = 0;
            runMusicLoad();
        }
        sleepMillis(10);
    }
}

#ifdef _WIN32
static DWORD WINAPI musicLoadThreadMain(LPVOID arg) { (void)arg; musicLoop(); return 0; }
#else
static void* musicLoadThreadMain(void* arg) { (void)arg; musicLoop(); return NULL; }
#endif

static void startMusicThread(void) {
#ifdef _WIN32
    HANDLE h = CreateThread(NULL, 0, musicLoadThreadMain, NULL, 0, NULL);
    if (h) CloseHandle(h);
#else
    pthread_t t;
    if (pthread_create(&t, NULL, musicLoadThreadMain, NULL) == 0) pthread_detach(t);
#endif
}

static void startRandomMusicTrack(void) {
    int variant = 1 + JRandom_nextIntN(&gRandom, MUSIC_VARIANT_COUNT);
    if (gMusicLoadInFlight) return;
    gMusicLoadInFlight = 1;
    snprintf(gPendingMusicPath, sizeof gPendingMusicPath, "resources/music/calm%d.ogg", variant);
    gMusicPending = 1;
}

// runs whatever the music option says, a track started with music off is dropped by the mixer
void Sound_tickMusic(void) {
    if (!gBackendOk) return;
    long long now = currentTimeMillis();
    if (now > gLastMusic) {
        startRandomMusicTrack();
        gLastMusic = now + JRandom_nextIntN(&gRandom, 900000) + 300000LL;
    }
}
