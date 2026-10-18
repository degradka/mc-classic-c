// jmd5.h: MessageDigest MD5

#ifndef JMD5_H
#define JMD5_H

#include <stddef.h>
#include <stdint.h>

void jmd5(const uint8_t* data, size_t len, uint8_t out[16]);

#endif
