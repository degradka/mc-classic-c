// jtime.h: System.nanoTime, currentTimeMillis, Thread.sleep and Double.toString

#ifndef JTIME_H
#define JTIME_H

#include <stddef.h>
#include <stdint.h>

int64_t jtime_nanos(void);
int64_t jtime_millis(void);
void jtime_sleep(int millis);
void jtime_doubleString(double v, char* out, size_t size);

#endif
