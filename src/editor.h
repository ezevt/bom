#ifndef EDITOR_H
#define EDITOR_H

#include "defs.h"
#include "term.h"

typedef struct {
    i32 line;
    i32 col;
    i32 goal_col;
} Cursor;

typedef struct {
    i32 size;
    char* chars;
} Line;

typedef struct {
    b8 running;

    Cursor cursor;
    i32 row_offset;
    i32 col_offset;

    i32 rows, cols;

    const char* filename;

    i32 num_lines;
    Line* lines;
} Editor;

void editor_init(Editor* e);
void editor_shutdown(Editor* e);

void editor_open(Editor* e, const char* filepath);

void editor_dispatch(Editor* e, Event ev);
void editor_render(Editor* e);

#endif
