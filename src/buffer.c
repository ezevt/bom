#include "buffer.h"
#include "line.h"

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

void buffer_init(Buffer* b) {
    b->filename = NULL;
    b->num_lines = 0;
    b->lines = NULL;
    b->dirty = false;
}

void buffer_open(Buffer* b, const char* filepath) {
    b->filename = filepath;

    FILE* fp = fopen(filepath, "r");
    if (!fp) return;

    char* line = NULL;
    size_t linecap = 0;
    ssize_t linelen;

    while ((linelen = getline(&line, &linecap, fp)) != -1) {
        while (linelen > 0 && (line[linelen - 1] == '\n' ||
                               line[linelen - 1] == '\r'))
            linelen--;

        buffer_append_line(b, line, linelen, b->num_lines);
    }

    free(line);
    fclose(fp);

    b->dirty = false;
}

static char* lines_to_string(Line* lines, i32 num_lines) {
    i32 len = 0;
    for (i32 i = 0; i < num_lines; i++) {
        len += lines[i].size + 1;
    }

    char* buf = malloc(len+1);
    
    i32 idx = 0;
    for (i32 i = 0; i < num_lines; i++) {
        memcpy(buf+idx, lines[i].chars, lines[i].size);
        idx += lines[i].size;
        buf[idx++] = '\n';
    }

    buf[len] = '\0';

    return buf;
}

void buffer_save(Buffer* b) {
    char* buf = lines_to_string(b->lines, b->num_lines);

    int fd = open(b->filename, O_RDWR|O_CREAT, 0644);

    ftruncate(fd, 0);
    write(fd, buf, strlen(buf));

    close(fd);
    free(buf);

    b->dirty = false;
}

void buffer_free(Buffer *b) {
    if (b->num_lines > 0) {
        for (i32 i = 0; i < b->num_lines; i++) {
            line_free(&b->lines[i]);
        }
    }

    if (b->lines != NULL) {
        free(b->lines);
        b->lines = NULL;
        b->num_lines = 0;
    }

}

void buffer_append_line(Buffer* b, char* s, i32 len, i32 at) {
    if (at < 0) at = 0;
    if (at > b->num_lines) at = b->num_lines;

    b->lines = realloc(b->lines, sizeof(Line) * (b->num_lines + 1));

    if (at != b->num_lines)
        memmove(b->lines + at + 1,
                b->lines + at,
                (b->num_lines - at) * sizeof(Line));

    Line* l = &b->lines[at];
    line_init(l);
    line_set(l, s, len);

    b->num_lines++;
    b->dirty = true;
}

void buffer_merge_lines(Buffer* b, i32 line) {
    if (line <= 0 || line >= b->num_lines) return;

    Line* l1 = &b->lines[line-1];
    Line* l2 = &b->lines[line];

    line_insert(l1, l1->size, l2->chars, l2->size);

    line_free(l2);

    memmove(&b->lines[line], &b->lines[line+1],
            (b->num_lines - line - 1) * sizeof(Line));

    b->num_lines--;
    b->dirty = true;
}

void buffer_split_line(Buffer *b, i32 line, i32 col) {
    if (b->lines == NULL) buffer_append_line(b, "", 0, 0);
    if (line < 0 || line >= b->num_lines) return;
    
    Line* l = &b->lines[line];

    if (col < 0 || col > l->size) return;

    i32 len = l->size-col;

    buffer_append_line(b, l->chars+col, len, line+1);

    l = &b->lines[line];
    line_truncate(l, col);

    b->dirty = true;
}

void buffer_insert_text(Buffer* b, i32 line, i32 col, const char* s, i32 n) {
    if (line < 0) return;

    while (line >= b->num_lines) buffer_append_line(b, "", 0, b->num_lines);

    Line* l = &b->lines[line];
    line_insert(l, col, s, n);

    b->dirty = true;
}

void buffer_delete_range(Buffer* b, i32 line, i32 from, i32 to) {
    if (line < 0 || line >= b->num_lines) return;

    Line* l = &b->lines[line];
    line_delete(l, from, to);

    b->dirty = true;
}
