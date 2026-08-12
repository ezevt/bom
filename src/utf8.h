#ifndef UTF8_H
#define UTF8_H

#include "defs.h"

static inline b8 utf8_is_cont(u8 c) { return (c & 0xC0) == 0x80; }

static inline i32 utf8_seq_len(u8 c) {
    if (c < 0x80) return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    return 1;
}

static inline i32 utf8_decode(const char* s, i32 len, i32* cp) {
    u8 c = (u8)s[0];
    i32 n = utf8_seq_len(c);

    if (n == 1 || n > len) { *cp = 0xFFFD; return 1; }

    static const u8 mask[5] = {0, 0x7F, 0x1F, 0x0F, 0x07};
    i32 v = c & mask[n];

    for (i32 i = 1; i < n; i++) {
        if (!utf8_is_cont((u8)s[i])) { *cp = 0xFFFD; return 1; }
        v = (v << 6) | ((u8)s[i] & 0x3F);
    }

    *cp = v;
    return n;
}

static inline i32 utf8_encode(char* out, i32 cp) {
    if (cp < 0x80)    { out[0] = cp; return 1; }
    if (cp < 0x800)   { out[0] = 0xC0 | (cp >> 6);
                        out[1] = 0x80 | (cp & 0x3F); return 2; }
    if (cp < 0x10000) { out[0] = 0xE0 | (cp >> 12);
                        out[1] = 0x80 | ((cp >> 6) & 0x3F);
                        out[2] = 0x80 | (cp & 0x3F); return 3; }
    out[0] = 0xF0 | (cp >> 18);
    out[1] = 0x80 | ((cp >> 12) & 0x3F);
    out[2] = 0x80 | ((cp >> 6) & 0x3F);
    out[3] = 0x80 | (cp & 0x3F);
    return 4;
}

static inline i32 utf8_next(const char* s, i32 size, i32 i) {
    if (i >= size) return size;
    i++;
    while (i < size && utf8_is_cont((u8)s[i])) i++;
    return i;
}

static inline i32 utf8_prev(const char* s, i32 i) {
    if (i <= 0) return 0;
    i--;
    while (i > 0 && utf8_is_cont((u8)s[i])) i--;
    return i;
}

static inline i32 utf8_width(i32 cp) {
    if (cp == '\t') return 4;
    if (cp < 0x20)  return 2;   /* ^X */
    return 1;
}

#endif
