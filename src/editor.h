#ifndef EDITOR_H
#define EDITOR_H

#include "defs.h"
#include "term.h"

typedef struct {
    i32 x, y;
} Cursor;

typedef struct {
    i32 size;
    char* chars;
} Line;

typedef struct {
    b8 running;

    Cursor cursor;
    i32 offset;

    i32 num_lines;
    Line* lines;
} Editor;

void editor_init(Editor* e);
void editor_shutdown(Editor* e);

void editor_open(Editor* e, const char* filepath);

void editor_dispatch(Editor* e, Event ev);
void editor_render(Editor* e);

#endif
