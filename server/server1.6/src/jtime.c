// jtime.c

#include "jtime.h"
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
  #include <windows.h>
#else
  #include <time.h>
  #include <sys/time.h>
#endif

int64_t jtime_nanos(void) {
#if defined(_WIN32)
    static LARGE_INTEGER freq;
    if (!freq.QuadPart) QueryPerformanceFrequency(&freq);
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return (int64_t)((double)now.QuadPart * 1e9 / (double)freq.QuadPart);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000000LL + ts.tv_nsec;
#endif
}

int64_t jtime_millis(void) {
#if defined(_WIN32)
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    uint64_t t = ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    return (int64_t)(t / 10000ULL - 11644473600000ULL);
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
#endif
}

void jtime_sleep(int millis) {
#if defined(_WIN32)
    Sleep((DWORD)millis);
#else
    struct timespec ts = { millis / 1000, (long)(millis % 1000) * 1000000L };
    nanosleep(&ts, NULL);
#endif
}

// the shortest digits that read back the same
void jtime_doubleString(double v, char* out, size_t size) {
    char digits[40];
    int exp10 = 0;
    for (int p = 1; p <= 17; ++p) {
        snprintf(digits, sizeof digits, "%.*e", p - 1, v);
        if (strtod(digits, NULL) == v) break;
    }
    char* e = strchr(digits, 'e');
    exp10 = atoi(e + 1);
    *e = '\0';
    char mant[40];
    int n = 0;
    bool neg = digits[0] == '-';
    for (char* c = digits + (neg ? 1 : 0); *c; ++c) if (*c != '.') mant[n++] = *c;
    while (n > 1 && mant[n - 1] == '0') n--;
    mant[n] = '\0';
    char buf[64];
    int k = 0;
    if (neg) buf[k++] = '-';
    if (v == 0.0) {
        snprintf(out, size, "%s0.0", neg ? "-" : "");
        return;
    }
    if (exp10 >= -3 && exp10 < 7) {
        if (exp10 < 0) {
            buf[k++] = '0';
            buf[k++] = '.';
            for (int i = -1; i > exp10; --i) buf[k++] = '0';
            for (int i = 0; i < n; ++i) buf[k++] = mant[i];
        } else {
            for (int i = 0; i <= exp10; ++i) buf[k++] = i < n ? mant[i] : '0';
            buf[k++] = '.';
            if (n > exp10 + 1) for (int i = exp10 + 1; i < n; ++i) buf[k++] = mant[i];
            else buf[k++] = '0';
        }
        buf[k] = '\0';
    } else {
        buf[k++] = mant[0];
        buf[k++] = '.';
        if (n > 1) for (int i = 1; i < n; ++i) buf[k++] = mant[i];
        else buf[k++] = '0';
        k += snprintf(buf + k, sizeof buf - (size_t)k, "E%d", exp10);
    }
    snprintf(out, size, "%s", buf);
}
