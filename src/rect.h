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

#endif
