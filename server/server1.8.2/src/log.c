// log.c: console equivalents of the client's loading screen hooks, plus the
// shared Log_info/Log_warn helpers used everywhere else. The server has no
// GUI, so LevelGen's title/status/progress calls just become console lines

#include "log.h"
#include <stdio.h>
#include <time.h>

// new in server1.4.1: the real source adds a second log handler writing to
// server.log alongside the existing console output. Opened lazily so a
// server that never logs anything doesn't create an empty file
static FILE* logFile = NULL;

static void logLine(const char* severity, const char* fmt, va_list args) {
    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    char stamp[16];
    strftime(stamp, sizeof stamp, "%H:%M:%S", t);

    char msg[512];
    vsnprintf(msg, sizeof msg, fmt, args);

    fprintf(stderr, "%s  %s  %s\n", severity, stamp, msg);

    if (!logFile) logFile = fopen("server.log", "a");
    if (logFile) {
        fprintf(logFile, "%s  %s  %s\n", severity, stamp, msg);
        fflush(logFile);
    }
}

void Log_info(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    logLine("   ", fmt, args);
    va_end(args);
}

void Log_warn(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    logLine("  !", fmt, args);
    va_end(args);
}

void Log_severe(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    logLine("***", fmt, args);
    va_end(args);
}

void Server_beginLevelLoading(const char* title) {
    Log_info("%s", title);
}

void Server_levelLoadUpdate(const char* status) {
    Log_info("%s", status);
}

void Server_levelLoadProgress(int percent) {
    (void)percent; // one line per phase is enough on a console, no need to spam every tick of progress
}
