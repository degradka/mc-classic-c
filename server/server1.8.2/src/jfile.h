// jfile.h: file handling that differs per platform

#ifndef JFILE_H
#define JFILE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

extern const char* const JFILE_NEWLINE;

// the whole file, NULL and out the FileNotFoundException's toString when it can't be opened
uint8_t* jfile_read(const char* path, int* len, char* exception, size_t size);
bool jfile_exists(const char* path);
// the toString of the FileNotFoundException for the last failed open of path
void jfile_notFound(const char* path, char* exception, size_t size);

#endif
