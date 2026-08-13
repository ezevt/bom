#ifndef LINE_H
#define LINE_H

#include "defs.h"

#define TAB_STOP 4

typedef struct {
    i32 size;
    char* chars;

    i32 rsize;
    char* render;
} Line;

void line_init(Line* l);
void line_free(Line* l);
void line_set(Line* l, const char* s, i32 len);

void line_insert(Line* l, i32 col, const char* s, i32 n);
void line_delete(Line* l, i32 from, i32 to);
void line_truncate(Line* l, i32 col);

i32 line_width(Line* l, i32 upto);
i32 line_byte_at(Line* l, i32 target_col);
i32 line_cx_to_rx(Line* l, i32 cx);

#endif
