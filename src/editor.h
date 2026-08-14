#ifndef EDITOR_H
#define EDITOR_H

#include "defs.h"
#include "term.h"
#include "buffer.h"
#include "rect.h"
#include "view.h"

typedef struct {
    Rect text;
    Rect status;
    Rect cmd;
} Layout;

typedef struct {
    b8 running;

    i32 rows, cols;
    Layout layout;
    
    Buffer buffer;
    View view;
} Editor;

void editor_init(Editor* e);
void editor_shutdown(Editor* e);

void editor_open(Editor* e, const char* filepath);

void editor_dispatch(Editor* e, Event ev);
void editor_render(Editor* e);

#endif
