// stdin_reader.c

#include "stdin_reader.h"
#include "server.h"
#include "commands.h"
#include "log.h"
#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
  #include <windows.h>
#else
  #include <pthread.h>
#endif

#define STDIN_QUEUE_CAP 64
#define STDIN_LINE_LEN  256

// single producer (the reader thread below), single consumer (StdinReader_poll
// on the main thread). A plain mutex-guarded ring buffer is plenty for
// console input rates
static char sQueue[STDIN_QUEUE_CAP][STDIN_LINE_LEN];
static int sHead = 0, sCount = 0;

#if defined(_WIN32)
static CRITICAL_SECTION sLock;
#else
static pthread_mutex_t sLock = PTHREAD_MUTEX_INITIALIZER;
#endif

static void lock(void)   {
#if defined(_WIN32)
    EnterCriticalSection(&sLock);
#else
    pthread_mutex_lock(&sLock);
#endif
}
static void unlock(void) {
#if defined(_WIN32)
    LeaveCriticalSection(&sLock);
#else
    pthread_mutex_unlock(&sLock);
#endif
}

static void pushLine(const char* line) {
    lock();
    if (sCount < STDIN_QUEUE_CAP) {
        int tail = (sHead + sCount) % STDIN_QUEUE_CAP;
        snprintf(sQueue[tail], STDIN_LINE_LEN, "%s", line);
        sCount++;
    }
    unlock();
}

#if defined(_WIN32)
static DWORD WINAPI threadMain(LPVOID arg) {
#else
static void* threadMain(void* arg) {
#endif
    (void)arg;
    char line[STDIN_LINE_LEN];
    while (fgets(line, sizeof line, stdin)) {
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) line[--len] = '\0';
        pushLine(line);
    }
    Log_warn("stdin: end of file! No more direct console input is possible.");
#if defined(_WIN32)
    return 0;
#else
    return NULL;
#endif
}

void StdinReader_start(MinecraftServer* srv) {
    (void)srv;
#if defined(_WIN32)
    InitializeCriticalSection(&sLock);
    HANDLE h = CreateThread(NULL, 0, threadMain, NULL, 0, NULL);
    if (h) CloseHandle(h); // detached: nothing needs to join it
#else
    pthread_t t;
    if (pthread_create(&t, NULL, threadMain, NULL) == 0) pthread_detach(t);
#endif
}

void StdinReader_poll(MinecraftServer* srv) {
    for (;;) {
        char line[STDIN_LINE_LEN];
        lock();
        if (sCount == 0) { unlock(); break; }
        snprintf(line, sizeof line, "%s", sQueue[sHead]);
        sHead = (sHead + 1) % STDIN_QUEUE_CAP;
        sCount--;
        unlock();

        Commands_dispatch(srv, NULL, line);
    }
}
