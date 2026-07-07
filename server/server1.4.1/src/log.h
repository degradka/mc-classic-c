// log.h: console line formatting matching the real Java server's style
// (severity code, HH:mm:ss timestamp, message), e.g. "     11:52:24  guest connected"
// or "  !  11:52:24  guest lost connection suddenly"

#ifndef LOG_H
#define LOG_H

#include <stdarg.h>

void Log_info(const char* fmt, ...);
void Log_warn(const char* fmt, ...);
void Log_severe(const char* fmt, ...);

#endif
