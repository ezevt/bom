#ifndef RECT_H
#define RECT_H

#include "defs.h"

typedef struct {
    i32 x, y;
    i32 w, h;
} Rect;

static inline Rect rect_cut_bottom(Rect* r, i32 h) {
    if (h > r->h) h = r->h;
    if (h < 0) h = 0;
    r->h -= h;
    Rect s = { r->x, r->y + r->h, r->w, h };
    return s;
}

static inline void scroll_track(i32* offset, i32 cursor, i32 size, i32 margin) {
    if (cursor < *offset + margin) {
        *offset = cursor - margin;
    }

    if (cursor >= *offset + size - margin) {
        *offset = cursor - size + margin + 1;
    }

    if (*offset < 0)
        *offset = 0;
}

#endif
