#ifndef BUFFER_H
#define BUFFER_H

#include "defs.h"

typedef struct {
    i32 size;
    char* chars;
} Line;

typedef struct {
    i32 num_lines;
    Line* lines;

    const char* filename;
} Buffer;

void buffer_open(Buffer* b, const char* filename);
void buffer_save(Buffer* b);

void buffer_free(Buffer* b);

void buffer_append_line(Buffer* b, char* s, i32 len, i32 at);
void buffer_insert_char(Buffer* b, i32 line, i32 col, i32 c);
void buffer_remove_char(Buffer* b, i32 line, i32 col);
void buffer_merge_lines(Buffer* b, i32 line);
void buffer_split_line(Buffer* b, i32 line, i32 col);

i32 line_len(Buffer* b, i32 line);




#endif
