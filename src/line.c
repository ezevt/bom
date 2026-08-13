#include "line.h"
#include "utf8.h"

#include "stdio.h"
#include "string.h"

static void line_update_render(Line* l) {
    i32 tabs = 0;
    for (i32 i = 0; i < l->size; i++)
        if (l->chars[i] == '\t') tabs++;

    free(l->render);
    l->render = malloc(l->size + tabs * (TAB_STOP - 1) + 1);

    i32 idx = 0;
    for (i32 i = 0; i < l->size; i++) {
        if (l->chars[i] == '\t') {
            l->render[idx++] = ' ';
            while (idx % TAB_STOP != 0) l->render[idx++] = ' ';
        } else {
            l->render[idx++] = l->chars[i];
        }
    }
    l->render[idx] = '\0';
    l->rsize = idx;
}

void line_init(Line* l) {
    l->size = 0;
    l->rsize = 0;
    l->chars = NULL;
    l->render = NULL;
}

void line_free(Line* l) {
    free(l->chars);            
    free(l->render);
}

void line_set(Line* l, const char* s, i32 len) {
    l->size = len;
    l->chars = malloc(len + 1);
    memcpy(l->chars, s, len);
    l->chars[len] = '\0';
    l->render = NULL;

    line_update_render(l);
}

void line_insert(Line* l, i32 col, const char* s, i32 n) {
    if (col < 0) col = 0;
    if (col > l->size) col = l->size;

    char* new = realloc(l->chars, l->size + n + 1);
    if (!new) return;

    memmove(new + col + n, new + col, l->size - col + 1); /* +1 = NUL */
    memcpy(new + col, s, n);

    l->chars = new;
    l->size += n;

    line_update_render(l);
}

void line_delete(Line* l, i32 from, i32 to) {
    memmove(l->chars + from, l->chars + to, l->size - to + 1);
    l->size -= (to - from);
    line_update_render(l);
}

void line_truncate(Line* l, i32 col) {
    i32 len = l->size-col;

    l->size -= len;
    l->chars[l->size] = '\0';

    line_update_render(l);
}

i32 line_width(Line* l, i32 upto) {
    i32 i = 0, col = 0, cp;
    while (i < upto && i < l->size) {
        i += utf8_decode(l->chars + i, l->size - i, &cp);
        col += utf8_width(cp);
    }
    return col;
}

i32 line_byte_at(Line* l, i32 target_col) {
    i32 i = 0, col = 0, cp;
    while (i < l->size) {
        i32 n = utf8_decode(l->chars + i, l->size - i, &cp);
        i32 w = utf8_width(cp);
        if (col + w > target_col) break;
        col += w;
        i += n;
    }
    return i;
}

i32 line_cx_to_rx(Line* l, i32 cx) {
    i32 rx = 0;
    i32 i = 0;
    i32 cp;

    while (i < cx && i < l->size) {
        i32 n = utf8_decode(l->chars + i, l->size - i, &cp);

        if (cp == '\t')
            rx += TAB_STOP - (rx % TAB_STOP);
        else
            rx += utf8_width(cp);

        i += n;
    }

    return rx;
}
