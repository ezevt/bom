#include "buffer.h"
#include "utf8.h"

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

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
}

void buffer_free(Buffer *b) {
    if (b->num_lines > 0) {
        for (i32 i = 0; i < b->num_lines; i++) {
            free(b->lines[i].chars);            
        }
    }

    if (b->lines != NULL) {
        free(b->lines);
        b->lines = NULL;
        b->num_lines = 0;
    }

}

static void line_update_render(Line* l) {
    i32 tabs = 0;
    for (i32 i = 0; i < l->size; i++)
        if (l->chars[i] == '\t') tabs++;

    free(l->render);
    l->render = malloc(l->size + tabs * (TAB_STOP - 1) + 1);

    i32 idx = 0;
    for (i32 i = 0; i < l->size; i++) {
        if (l->chars[i] == '\t') {
            l->render[idx++] = ' ';
            while (idx % TAB_STOP != 0) l->render[idx++] = ' ';
        } else {
            l->render[idx++] = l->chars[i];
        }
    }
    l->render[idx] = '\0';
    l->rsize = idx;
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

    l->size = len;
    l->chars = malloc(len + 1);
    memcpy(l->chars, s, len);
    l->chars[len] = '\0';
    l->render = NULL;
    b->num_lines++;

    line_update_render(l);
}

void buffer_insert_text(Buffer* b, i32 line, i32 col, const char* s, i32 n) {
    if (line < 0 || line >= b->num_lines) return;
    Line* l = &b->lines[line];
    if (col < 0) col = 0;
    if (col > l->size) col = l->size;

    char* new = realloc(l->chars, l->size + n + 1);
    if (!new) return;

    memmove(new + col + n, new + col, l->size - col + 1); /* +1 = NUL */
    memcpy(new + col, s, n);

    l->chars = new;
    l->size += n;

    line_update_render(l);
}

void buffer_delete_range(Buffer* b, i32 line, i32 from, i32 to) {
    Line* l = &b->lines[line];
    memmove(l->chars + from, l->chars + to, l->size - to + 1);
    l->size -= (to - from);
    line_update_render(l);
}

void buffer_merge_lines(Buffer* b, i32 line) {
    if (line <= 0 || line >= b->num_lines) return;

    Line* l1 = &b->lines[line-1];
    Line* l2 = &b->lines[line];

    i32 len = l1->size + l2->size;
    char* new = realloc(l1->chars, len + 1);
    if (!new) return;

    memcpy(new + l1->size, l2->chars, l2->size);
    new[len] = '\0';

    l1->chars = new;
    l1->size  = len;

    free(l2->chars);

    memmove(&b->lines[line], &b->lines[line+1],
            (b->num_lines - line - 1) * sizeof(Line));

    b->num_lines--;

    line_update_render(l1);
}

void buffer_split_line(Buffer *b, i32 line, i32 col) {
    if (line < 0 || line >= b->num_lines) return;
    
    Line* l = &b->lines[line];

    if (col < 0 || col > l->size) return;

    i32 len = line_len(b, line)-col;

    buffer_append_line(b, l->chars+col, len, line+1);
    
    l = &b->lines[line];
    l->size -= len;
    l->chars[l->size] = '\0';

    line_update_render(l);
}

i32 line_len(Buffer* b, i32 line) {
    if (line < 0 || line >= b->num_lines) return 0;
    return b->lines[line].size;
}

i32 line_width(Buffer* b, i32 line, i32 upto) {
    Line* l = &b->lines[line];
    i32 i = 0, col = 0, cp;
    while (i < upto && i < l->size) {
        i += utf8_decode(l->chars + i, l->size - i, &cp);
        col += utf8_width(cp);
    }
    return col;
}

i32 line_byte_at(Buffer* b, i32 line, i32 target_col) {
    Line* l = &b->lines[line];
    i32 i = 0, col = 0, cp;
    while (i < l->size) {
        i32 n = utf8_decode(l->chars + i, l->size - i, &cp);
        i32 w = utf8_width(cp);
        if (col + w > target_col) break;
        col += w;
        i += n;
    }
    return i;
}

i32 line_cx_to_rx(Buffer* b, i32 line, i32 cx) {
    Line* l = &b->lines[line];

    i32 rx = 0;
    i32 i = 0;
    i32 cp;

    while (i < cx && i < l->size) {
        i32 n = utf8_decode(l->chars + i, l->size - i, &cp);

        if (cp == '\t')
            rx += TAB_STOP - (rx % TAB_STOP);
        else
            rx += utf8_width(cp);

        i += n;
    }

    return rx;
}

