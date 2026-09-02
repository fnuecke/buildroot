/* SPDX-License-Identifier: MIT */
/* GCC emits calls to these even freestanding, so they must exist. */

#include "fw.h"

void *memcpy(void *dst, const void *src, u64 n) {
    u8 *d = dst;
    const u8 *s = src;
    while (n--) {
        *d++ = *s++;
    }
    return dst;
}

void *memset(void *dst, const int value, u64 n) {
    u8 *d = dst;
    while (n--) {
        *d++ = (u8) value;
    }
    return dst;
}

int memcmp(const void *a, const void *b, u64 n) {
    const u8 *x = a, *y = b;
    for (; n--; x++, y++) {
        if (*x != *y) {
            return *x - *y;
        }
    }
    return 0;
}

int strcmp(const char *a, const char *b) {
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return (u8) *a - (u8) *b;
}
