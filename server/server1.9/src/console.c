// console.c

#include "console.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
  #include <windows.h>
  static CRITICAL_SECTION lock;
  #define LOCK() EnterCriticalSection(&lock)
  #define UNLOCK() LeaveCriticalSection(&lock)
#else
  #include <pthread.h>
  static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
  #define LOCK() pthread_mutex_lock(&lock)
  #define UNLOCK() pthread_mutex_unlock(&lock)
#endif

typedef struct Line {
    struct Line* next;
    int len;
    uint8_t bytes[];
} Line;

static Line* head;
static Line* tail;

static void push(const uint8_t* bytes, int len) {
    Line* l = (Line*)malloc(sizeof *l + (size_t)len);
    l->next = NULL;
    l->len = len;
    memcpy(l->bytes, bytes, (size_t)len);
    LOCK();
    if (tail) tail->next = l;
    else head = l;
    tail = l;
    UNLOCK();
}

// BufferedReader.readLine on stdin, \n, \r or \r\n end a line
static void readLoop(void) {
    static uint8_t buf[65536];
    int len = 0;
    bool lastCR = false;
    int c;
    while ((c = getchar()) != EOF) {
        if (lastCR && c == '\n') {
            lastCR = false;
            continue;
        }
        lastCR = c == '\r';
        if (c == '\n' || c == '\r') {
            push(buf, len);
            len = 0;
            continue;
        }
        if (len < (int)sizeof buf) buf[len++] = (uint8_t)c;
    }
    if (len > 0) push(buf, len);
    Log_warning("stdin: end of file! No more direct console input is possible.");
}

#if defined(_WIN32)
static DWORD WINAPI threadMain(LPVOID arg) { (void)arg; readLoop(); return 0; }
#else
static void* threadMain(void* arg) { (void)arg; readLoop(); return NULL; }
#endif

void Console_start(void) {
#if defined(_WIN32)
    InitializeCriticalSection(&lock);
    HANDLE h = CreateThread(NULL, 0, threadMain, NULL, 0, NULL);
    if (h) CloseHandle(h);
#else
    pthread_t t;
    if (pthread_create(&t, NULL, threadMain, NULL) == 0) pthread_detach(t);
#endif
}

bool Console_poll(JStr* line) {
    LOCK();
    Line* l = head;
    if (l) {
        head = l->next;
        if (!head) tail = NULL;
    }
    UNLOCK();
    if (!l) return false;
    jstr_fromPlatform(line, l->bytes, l->len);
    free(l);
    return true;
}
