#ifndef EDITOR_H
#define EDITOR_H

#include "defs.h"
#include "term.h"

typedef struct {
    i32 x, y;
} Cursor;

typedef struct {
    Cursor cursor;
} Editor;

void editor_init(Editor* e);
void editor_shutdown(Editor* e);

void editor_dispatch(Editor* e, Event ev);
void editor_render(Editor* e);

#endif
