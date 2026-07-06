// log.c

#include "log.h"
#include <stdarg.h>
#include <stdio.h>
#include <time.h>

static void line(const char* mark, const char* fmt, va_list args) {
    time_t now = time(NULL);
    char stamp[16];
    strftime(stamp, sizeof stamp, "%H:%M:%S", localtime(&now));
    fprintf(stderr, "%s  %s  ", mark, stamp);
    vfprintf(stderr, fmt, args);
    fputc('\n', stderr);
    fflush(stderr);
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
