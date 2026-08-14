#ifndef VIEW_H
#define VIEW_H

#include "defs.h"
#include "buffer.h"
#include "term.h"
#include "rect.h"

#define V_MARGIN 8

typedef struct {
    i32 line;
    i32 col;
    i32 goal_col;
} Cursor;

typedef struct {
    Buffer* buf;

    Cursor cursor;
    i32 row_offset;
    i32 col_offset;
} View;


void view_init(View* v);
void view_set_buffer(View* v, Buffer* b);
void view_dispatch(View* v, Event ev);
void view_draw(View* v, Rect r);


#endif
