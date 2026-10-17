// log.c

#include "log.h"
#include "jfile.h"
#include <stdarg.h>
#include <stdio.h>
#include <time.h>


static FILE* logFile;

void Log_openFile(void) {
    logFile = fopen("server.log", "wb");
    if (logFile) return;
    char exception[512];
    jfile_notFound("server.log", exception, sizeof exception);
    Log_warning("Failed to open file server.log for writing: %s", exception);
}

static void line(const char* mark, const char* fmt, va_list args) {
    time_t now = time(NULL);
    char stamp[16];
    strftime(stamp, sizeof stamp, "%H:%M:%S", localtime(&now));
    char text[4096];
    vsnprintf(text, sizeof text, fmt, args);
    fprintf(stderr, "%s  %s  %s\n", mark, stamp, text);
    fflush(stderr);
    if (logFile) {
        fprintf(logFile, "%s  %s  %s\n", mark, stamp, text);
        fflush(logFile);
    }
}

void Log_info(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    line("   ", fmt, args);
    va_end(args);
}

void Log_warning(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    line("  !", fmt, args);
    va_end(args);
}

void Log_severe(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    line("***", fmt, args);
    va_end(args);
}

void Log_stackTrace(const char* exception) {
    fprintf(stderr, "%s\n", exception);
    fflush(stderr);
}
