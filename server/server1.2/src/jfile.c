// jfile.c

#include "jfile.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
  #include <windows.h>
  const char* const JFILE_NEWLINE = "\r\n";
#else
  const char* const JFILE_NEWLINE = "\n";
#endif

void jfile_notFound(const char* path, char* exception, size_t size) {
    char reason[256];
#if defined(_WIN32)
    DWORD err = GetLastError();
    DWORD n = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, err,
                             MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reason, sizeof reason, NULL);
    // drops the final period and line break
    if (n > 3) {
        if (reason[n - 1] == '\n') n--;
        if (reason[n - 1] == '\r') n--;
        if (reason[n - 1] == '.') n--;
    }
    reason[n] = '\0';
#else
    snprintf(reason, sizeof reason, "%s", strerror(errno));
#endif
    snprintf(exception, size, "java.io.FileNotFoundException: %s (%s)", path, reason);
}

uint8_t* jfile_read(const char* path, int* len, char* exception, size_t size) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        jfile_notFound(path, exception, size);
        return NULL;
    }
    size_t cap = 4096, n = 0;
    uint8_t* buf = (uint8_t*)malloc(cap);
    for (;;) {
        if (n == cap) {
            cap *= 2;
            buf = (uint8_t*)realloc(buf, cap);
        }
        size_t r = fread(buf + n, 1, cap - n, f);
        if (r == 0) break;
        n += r;
    }
    fclose(f);
    *len = (int)n;
    return buf;
}
