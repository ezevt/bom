#ifndef BUFFER_H
#define BUFFER_H

#include "defs.h"
#include "line.h"

typedef struct {
    i32 num_lines;
    Line* lines;

    b8 dirty;
    const char* filename;
} Buffer;

void buffer_init(Buffer* b);
void buffer_open(Buffer* b, const char* filename);
void buffer_save(Buffer* b);

void buffer_free(Buffer* b);

void buffer_append_line(Buffer* b, char* s, i32 len, i32 at);
void buffer_merge_lines(Buffer* b, i32 line);
void buffer_split_line(Buffer* b, i32 line, i32 col);
void buffer_insert_text(Buffer* b, i32 line, i32 col, const char* s, i32 n);
void buffer_delete_range(Buffer* b, i32 line, i32 from, i32 to);

#endif
