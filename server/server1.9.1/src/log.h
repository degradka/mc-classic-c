// log.h: the console lines, a level mark, HH:mm:ss and the message on stderr
// fine messages are below the logger's level and never show
// the same lines go to server.log, flushed after every one

#ifndef LOG_H
#define LOG_H

void Log_openFile(void);
void Log_info(const char* fmt, ...);
void Log_warning(const char* fmt, ...);
void Log_severe(const char* fmt, ...);
// printStackTrace, only the first line
void Log_stackTrace(const char* exception);

#endif
