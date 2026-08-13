#ifndef BUFFER_H
#define BUFFER_H

#include "defs.h"

#define TAB_STOP 4

typedef struct {
    i32 size;
    char* chars;

    i32 rsize;
    char* render;
} Line;

typedef struct {
    i32 num_lines;
    Line* lines;

    b8 dirty;
    const char* filename;
} Buffer;

void buffer_open(Buffer* b, const char* filename);
void buffer_save(Buffer* b);

void buffer_free(Buffer* b);

void buffer_append_line(Buffer* b, char* s, i32 len, i32 at);
void buffer_insert_text(Buffer* b, i32 line, i32 col, const char* s, i32 n);
void buffer_delete_range(Buffer* b, i32 line, i32 from, i32 to);
void buffer_merge_lines(Buffer* b, i32 line);
void buffer_split_line(Buffer* b, i32 line, i32 col);

i32 line_len(Buffer* b, i32 line);
i32 line_width(Buffer* b, i32 line, i32 upto);
i32 line_byte_at(Buffer* b, i32 line, i32 target_col);
i32 line_cx_to_rx(Buffer* b, i32 line, i32 cx);

#endif
