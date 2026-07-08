// audio/sound.c

#include "sound.h"
#include "audio_backend.h"
#include "../entity.h"
#include "../options.h"
#include "../jrandom.h"
#include "../timer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// stb_vorbis.c defines L, C, R and the PLAYBACK_ macros and never undefines them,
// they clash with the mingw windows headers
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
  typedef CRITICAL_SECTION Lock;
  #define LOCK_INIT(l) InitializeCriticalSection(l)
  #define LOCK(l)      EnterCriticalSection(l)
  #define UNLOCK(l)    LeaveCriticalSection(l)
#else
  #include <pthread.h>
  typedef pthread_mutex_t Lock;
  #define LOCK_INIT(l) pthread_mutex_init(l, NULL)
  #define LOCK(l)      pthread_mutex_lock(l)
  #define UNLOCK(l)    pthread_mutex_unlock(l)
#endif

#define CHUNK 4410

// SoundData, one channel at its own rate
typedef struct {
    short* data;
    int length;
    float rate;
} SoundData;

// a name like "step.grass" and its variants in the order they were registered
typedef struct {
    char name[32];
    SoundData* list;
    int count;
} SoundList;

typedef struct {
    char name[32];
    char paths[8][96];
    int count;
} MusicList;

// SoundPos, an entity or a fixed spot, heard by the listener
typedef struct {
    struct Entity* source;
    float x, y, z;
    const struct Entity* listener;
} SoundPos;

// a Sound with its SoundInfo, or a Music
typedef struct {
    bool music;
    // Sound
    const SoundData* data;
    float seek;
    float step;
    float infoVolume;
    SoundPos pos;
    float pan;
    float volume;
    // Music
    short* pcm;
    volatile int decoded;
    volatile bool finished;
    volatile bool stopped;
    int played;
    // the queue and the decode thread each hold a music
    int refs;
    char path[96];
} Audio;

#define MAX_LISTS 16
static SoundList sounds[MAX_LISTS];
static int soundCount = 0;
static MusicList musics[MAX_LISTS];
static int musicCount = 0;

static JRandom random;
static long long lastMusic;

static const Options* options;
static bool running = false;
static Lock lock;

static Audio** queue = NULL;
static int queueCount = 0, queueCapacity = 0;

static int mixLeft[CHUNK], mixRight[CHUNK];
static short chunk[CHUNK * 2];
static int chunkPos = CHUNK;
static short soundBuffer[CHUNK];

// BaseSoundPos.getRotationDiff, NaN when the sound is right on the listener
static float rotationDiff(const SoundPos* p, float x, float z) {
    x -= p->listener->x;
    z -= p->listener->z;
    float d = (float)sqrt(x * x + z * z);
    x /= d;
    z /= d;
    if ((d /= 2.0f) > 1.0f) d = 1.0f;
    float c = (float)cos((double)(-p->listener->yRotation) * M_PI / 180.0 + M_PI);
    float s = (float)sin((double)(-p->listener->yRotation) * M_PI / 180.0 + M_PI);
    return (s * z - c * x) * d;
}

// BaseSoundPos.getDistanceSq, really a volume that fades out over 32 blocks
static float distanceVolume(const SoundPos* p, float x, float y, float z) {
    x -= p->listener->x;
    y -= p->listener->y;
    float zd = z - p->listener->z;
    float d = (float)sqrt(x * x + y * y + zd * zd);
    if ((d = 1.0f - d / 32.0f) < 0.0f) d = 0.0f;
    return d;
}

static float posRotationDiff(const SoundPos* p) {
    if (p->source) return rotationDiff(p, p->source->x, p->source->z);
    return rotationDiff(p, p->x, p->z);
}

static float posVolume(const SoundPos* p) {
    if (p->source) return distanceVolume(p, p->source->x, p->source->y, p->source->z);
    return distanceVolume(p, p->x, p->y, p->z);
}

// SoundInfo.update, how many samples it gave before it ran out
static int infoUpdate(Audio* a, short* out, int n) {
    if (a->seek >= (float)a->data->length) return 0;
    for (int i = 0; i < n; ++i) {
        int s0i = (int)a->seek;
        short s0 = a->data->data[s0i];
        short s1 = s0i < a->data->length - 1 ? a->data->data[s0i + 1] : 0;
        out[i] = (short)((float)s0 + (float)(s1 - s0) * (a->seek - (float)s0i));
        a->seek += a->step;
        if (a->seek >= (float)a->data->length) return i;
    }
    return n;
}

// Sound.play, the pan and volume slide from the last chunk's to this one's
static bool soundPlay(Audio* a, int* left, int* right, int n) {
    int count = infoUpdate(a, soundBuffer, n);
    bool more = count > 0;
    float pan = posRotationDiff(&a->pos);
    float volume = posVolume(&a->pos) * a->infoVolume;
    int l0 = (int)((a->pan > 0.0f ? 1.0f - a->pan : 1.0f) * a->volume * 65536.0f);
    int r0 = (int)((a->pan < 0.0f ? 1.0f + a->pan : 1.0f) * a->volume * 65536.0f);
    int l1 = (int)((pan > 0.0f ? 1.0f - pan : 1.0f) * volume * 65536.0f);
    int r1 = (int)((pan < 0.0f ? pan + 1.0f : 1.0f) * volume * 65536.0f);
    l1 -= l0;
    r1 -= r0;
    if (l1 != 0 || r1 != 0) {
        for (int i = 0; i < count; ++i) {
            int l = l0 + l1 * i / n;
            int r = r0 + r1 * i / n;
            left[i] += soundBuffer[i] * l >> 16;
            right[i] += soundBuffer[i] * r >> 16;
        }
    } else if (l0 >= 0 || r0 != 0) {
        for (int i = 0; i < count; ++i) {
            left[i] += soundBuffer[i] * l0 >> 16;
            right[i] += soundBuffer[i] * r0 >> 16;
        }
    }
    a->pan = pan;
    a->volume = volume;
    return more;
}

// Music.play, the decode thread may still be behind
static bool musicPlay(Audio* a, int* left, int* right, int n) {
    if (!options->music) {
        a->stopped = true;
        return false;
    }
    int avail = a->decoded - a->played;
    if (avail > n) avail = n;
    for (int i = 0; i < avail; ++i) {
        left[i] += a->pcm[(a->played + i) * 2];
        right[i] += a->pcm[(a->played + i) * 2 + 1];
    }
    a->played += avail;
    return a->played < a->decoded || !a->finished;
}

static void freeAudio(Audio* a) {
    if (a->music) {
        a->stopped = true;
        if (--a->refs == 0) {
            free(a->pcm);
            free(a);
        }
        return;
    }
    struct Entity* e = a->pos.source;
    if (e && --e->soundRefs == 0 && e->soundReleased) free(e);
    free(a);
}

// one pass of SoundPlayer.run
static void mixChunk(void) {
    memset(mixLeft, 0, sizeof mixLeft);
    memset(mixRight, 0, sizeof mixRight);
    LOCK(&lock);
    for (int i = 0; i < queueCount; ++i) {
        Audio* a = queue[i];
        bool more = a->music ? musicPlay(a, mixLeft, mixRight, CHUNK) : soundPlay(a, mixLeft, mixRight, CHUNK);
        if (more) continue;
        freeAudio(a);
        memmove(&queue[i], &queue[i + 1], (size_t)(queueCount - i - 1) * sizeof(Audio*));
        queueCount--;
        i--;
    }
    UNLOCK(&lock);
    if (!options->music && !options->sound) {
        memset(chunk, 0, sizeof chunk);
        return;
    }
    for (int i = 0; i < CHUNK; ++i) {
        int l = mixLeft[i];
        int r = mixRight[i];
        if (l < -32000) l = -32000;
        if (r < -32000) r = -32000;
        if (l >= 32000) l = 32000;
        if (r >= 32000) r = 32000;
        chunk[i * 2] = (short)l;
        chunk[i * 2 + 1] = (short)r;
    }
}

// the device asks for any amount, the mixer always mixes whole chunks
static void fill(int16_t* buffer, int frames) {
    while (frames > 0) {
        if (chunkPos == CHUNK) {
            mixChunk();
            chunkPos = 0;
        }
        int n = CHUNK - chunkPos;
        if (n > frames) n = frames;
        memcpy(buffer, chunk + chunkPos * 2, (size_t)n * 2 * sizeof(short));
        buffer += n * 2;
        frames -= n;
        chunkPos += n;
    }
}

// SoundPlayer.play, nothing is queued when the device didn't open
static void play(Audio* a) {
    if (!running) {
        freeAudio(a);
        return;
    }
    LOCK(&lock);
    if (queueCount == queueCapacity) {
        queueCapacity = queueCapacity ? queueCapacity * 2 : 32;
        queue = (Audio**)realloc(queue, (size_t)queueCapacity * sizeof(Audio*));
    }
    queue[queueCount++] = a;
    UNLOCK(&lock);
}

// "step/grass1.ogg" is "step.grass", the digits before the extension go
static void soundName(const char* file, char* out, size_t size) {
    snprintf(out, size, "%s", file);
    size_t len = strlen(out);
    if (len >= 4) out[len - 4] = '\0';
    for (char* p = out; *p; p++) {
        if (*p == '/') *p = '.';
    }
    len = strlen(out);
    while (len > 0 && out[len - 1] >= '0' && out[len - 1] <= '9') out[--len] = '\0';
}

// SoundReader, the channels are averaged into one
static void registerSound(const char* path, const char* file) {
    char name[32];
    soundName(file, name, sizeof name);
    int channels = 0, rate = 0;
    short* pcm = NULL;
    int frames = stb_vorbis_decode_filename(path, &channels, &rate, &pcm);
    if (frames <= 0 || !pcm) return;
    short* mono = (short*)malloc((size_t)frames * sizeof(short));
    for (int i = 0; i < frames; ++i) {
        int sum = 0;
        for (int c = 0; c < channels; ++c) sum += pcm[i * channels + c];
        mono[i] = (short)(sum / channels);
    }
    free(pcm);

    SoundList* list = NULL;
    for (int i = 0; i < soundCount; ++i) {
        if (strcmp(sounds[i].name, name) == 0) list = &sounds[i];
    }
    if (!list) {
        if (soundCount == MAX_LISTS) { free(mono); return; }
        list = &sounds[soundCount++];
        snprintf(list->name, sizeof list->name, "%s", name);
        list->list = NULL;
        list->count = 0;
    }
    list->list = (SoundData*)realloc(list->list, (size_t)(list->count + 1) * sizeof(SoundData));
    list->list[list->count++] = (SoundData){ mono, frames, (float)rate };
}

static void registerMusic(const char* file, const char* path) {
    char name[32];
    soundName(file, name, sizeof name);
    MusicList* list = NULL;
    for (int i = 0; i < musicCount; ++i) {
        if (strcmp(musics[i].name, name) == 0) list = &musics[i];
    }
    if (!list) {
        if (musicCount == MAX_LISTS) return;
        list = &musics[musicCount++];
        snprintf(list->name, sizeof list->name, "%s", name);
        list->count = 0;
    }
    if (list->count < 8) snprintf(list->paths[list->count++], sizeof list->paths[0], "%s", path);
}

// registered in the server's list order
static const char* SOUND_FILES[] = {
    "step/grass1.ogg", "step/grass2.ogg", "step/grass3.ogg", "step/grass4.ogg",
    "step/gravel1.ogg", "step/gravel2.ogg", "step/gravel3.ogg", "step/gravel4.ogg",
    "step/stone1.ogg", "step/stone2.ogg", "step/stone3.ogg", "step/stone4.ogg",
    "step/wood1.ogg", "step/wood2.ogg", "step/wood3.ogg", "step/wood4.ogg",
};
static const char* MUSIC_FILES[] = { "calm1.ogg", "calm2.ogg", "calm3.ogg" };

// one music thread from startup
#define MAX_PENDING 8
static Audio* pending[MAX_PENDING];
static int pendingCount = 0;

static void decodeMusic(Audio* a);

static void musicLoop(void) {
    for (;;) {
        Audio* a = NULL;
        LOCK(&lock);
        if (pendingCount > 0) {
            a = pending[0];
            memmove(pending, pending + 1, (size_t)(pendingCount - 1) * sizeof(Audio*));
            pendingCount--;
        }
        UNLOCK(&lock);
        if (a) decodeMusic(a);
        else Timer_sleepMillis(10);
    }
}

#ifdef _WIN32
static DWORD WINAPI musicThread(LPVOID arg) { (void)arg; musicLoop(); return 0; }
#else
static void* musicThread(void* arg) { (void)arg; musicLoop(); return NULL; }
#endif

static void startMusicThread(void) {
#ifdef _WIN32
    HANDLE h = CreateThread(NULL, 0, musicThread, NULL, 0, NULL);
    if (h) CloseHandle(h);
#else
    pthread_t t;
    if (pthread_create(&t, NULL, musicThread, NULL) == 0) pthread_detach(t);
#endif
}

void Sound_init(const Options* o) {
    options = o;
    LOCK_INIT(&lock);
    JRandom_initRandom(&random);
    lastMusic = currentTimeMillis() + 60000LL;
    for (size_t i = 0; i < sizeof SOUND_FILES / sizeof SOUND_FILES[0]; ++i) {
        char path[96];
        snprintf(path, sizeof path, "resources/sound/%s", SOUND_FILES[i]);
        registerSound(path, SOUND_FILES[i]);
    }
    for (size_t i = 0; i < sizeof MUSIC_FILES / sizeof MUSIC_FILES[0]; ++i) {
        char path[96];
        snprintf(path, sizeof path, "resources/music/%s", MUSIC_FILES[i]);
        registerMusic(MUSIC_FILES[i], path);
    }
    running = AudioBackend_init(fill) == 0;
    if (running) startMusicThread();
}

void Sound_shutdown(void) {
    if (running) AudioBackend_shutdown();
    running = false;
}

// SoundManager.getAudioInfo, the volume and pitch swap places on the way in
static Audio* getAudioInfo(const char* name, float volume, float pitch) {
    const SoundList* list = NULL;
    for (int i = 0; i < soundCount; ++i) {
        if (strcmp(sounds[i].name, name) == 0) list = &sounds[i];
    }
    if (!list) return NULL;
    const SoundData* data = &list->list[JRandom_nextIntN(&random, list->count)];
    Audio* a = (Audio*)calloc(1, sizeof(Audio));
    a->data = data;
    a->seek = 0.0f;
    // multiplies by 44100 where it should divide, every ogg is 44100 Hz anyway
    a->step = pitch * 44100.0f / data->rate;
    a->infoVolume = volume;
    return a;
}

static void startSound(Audio* a, SoundPos pos) {
    a->pos = pos;
    a->pan = posRotationDiff(&a->pos);
    a->volume = posVolume(&a->pos) * a->infoVolume;
    if (pos.source) {
        LOCK(&lock);
        pos.source->soundRefs++;
        UNLOCK(&lock);
    }
    play(a);
}

void Sound_playEntity(const char* name, struct Entity* source, const struct Entity* listener, float volume, float pitch) {
    if (!options->sound) return;
    Audio* a = getAudioInfo(name, volume, pitch);
    if (!a) return;
    startSound(a, (SoundPos){ source, 0.0f, 0.0f, 0.0f, listener });
}

void Sound_playAt(const char* name, float x, float y, float z, const struct Entity* listener, float volume, float pitch) {
    if (!options->sound) return;
    Audio* a = getAudioInfo(name, volume, pitch);
    if (!a) return;
    startSound(a, (SoundPos){ NULL, x, y, z, listener });
}

void Sound_releaseEntity(struct Entity* e) {
    LOCK(&lock);
    bool held = e->soundRefs > 0;
    if (held) e->soundReleased = true;
    UNLOCK(&lock);
    if (!held) free(e);
}

// the music thread, decodes the whole track in one go
static void decodeMusic(Audio* a) {
    int channels = 0, rate = 0;
    short* pcm = NULL;
    int frames = a->stopped ? 0 : stb_vorbis_decode_filename(a->path, &channels, &rate, &pcm);
    LOCK(&lock);
    if (frames > 0 && pcm && channels == 2) {
        a->pcm = pcm;
        a->decoded = frames;
    } else {
        free(pcm);
    }
    a->finished = true;
    if (--a->refs == 0) {
        free(a->pcm);
        free(a);
    }
    UNLOCK(&lock);
}


// SoundManager.playMusic, false only when nothing is registered under the name
static bool playMusic(const char* name) {
    const MusicList* list = NULL;
    for (int i = 0; i < musicCount; ++i) {
        if (strcmp(musics[i].name, name) == 0) list = &musics[i];
    }
    if (!list) return false;
    const char* path = list->paths[JRandom_nextIntN(&random, list->count)];
    if (!running) return true;
    Audio* a = (Audio*)calloc(1, sizeof(Audio));
    a->music = true;
    a->refs = 2;
    snprintf(a->path, sizeof a->path, "%s", path);
    play(a);
    LOCK(&lock);
    if (pendingCount < MAX_PENDING) pending[pendingCount++] = a;
    UNLOCK(&lock);
    return true;
}

void Sound_tick(void) {
    if (currentTimeMillis() > lastMusic && playMusic("calm")) {
        lastMusic = currentTimeMillis() + (long long)JRandom_nextIntN(&random, 900000) + 300000LL;
    }
}
