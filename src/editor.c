#include "editor.h"
#include "defs.h"
#include "term.h"
#include <stdio.h>
#include <string.h>

#define V_MARGIN 8

static void editor_append_line(Editor* e, char* s, size_t len) {
    e->lines = realloc(e->lines, sizeof(Line) * (e->num_lines + 1));

    int at = e->num_lines;
    e->lines[at].size = len;
    e->lines[at].chars = malloc(len + 1);
    memcpy(e->lines[at].chars, s, len);
    e->lines[at].chars[len] = '\0';
    e->num_lines++;
}

void editor_open(Editor* e, const char* filepath) {
    FILE* fp = fopen(filepath, "r");
    if (!fp) return;

    char* line = NULL;
    size_t linecap = 0;
    ssize_t linelen;

    while ((linelen = getline(&line, &linecap, fp)) != -1) {
        while (linelen > 0 && (line[linelen - 1] == '\n' ||
                               line[linelen - 1] == '\r'))
            linelen--;

        editor_append_line(e, line, linelen);
    }

    free(line);
    fclose(fp);
}

void editor_init(Editor* e) {
    e->running = true;
    e->cursor = (Cursor){0, 0};
    e->num_lines = 0;
    e->lines = NULL;

    e->offset = 0;
    
    term_get_size(&e->rows, &e->cols);
}

void editor_shutdown(Editor* e) {
    if (e->num_lines <= 0) return;

    for (i32 i = 0; i < e->num_lines; i++) {
        free(e->lines[i].chars);
    }

    free(e->lines);
}

static void editor_scroll(Editor* e) {
    i32 margin = V_MARGIN;

    if (margin * 2 >= e->rows)
        margin = e->rows / 2;

    if (e->cursor.y < e->offset + margin) {
        e->offset = e->cursor.y - margin;
    }

    if (e->cursor.y >= e->offset + e->rows - margin) {
        e->offset = e->cursor.y - e->rows + margin + 1;
    }

    if (e->offset < 0)
        e->offset = 0;
}

void editor_dispatch(Editor* e, Event ev) {
    if (ev.type != EV_KEY) return;
    if (ev.key.mods == MOD_CTRL) {
        if (ev.key.code == 'q') {
            e->running = false;
        }
        
        return;
    }

    switch (ev.key.code) {
        case KEY_UP:
            if (e->cursor.y > 0)
                e->cursor.y--;
            break;
        case KEY_DOWN:
            if (e->cursor.y < e->num_lines)
                e->cursor.y++;
            break;
        case KEY_LEFT:
            e->cursor.x--;
            break;
        case KEY_RIGHT:
            e->cursor.x++;
            break;
    }

    editor_scroll(e);
}

static void draw_rows(Editor* e) {

    for (i32 i = 0; i < e->rows; i++) {
        i32 file_row = i + e->offset;
        if (file_row >= e->num_lines) {
            if (e->num_lines == 0 && i == e->rows/3) {
                term_writef("BOM - version %s", BOM_VERSION);
            } else {
                term_write("~", 1);
            }
        } else {
            int len = e->lines[file_row].size;
            if (len > e->cols) len = e->cols;
            term_write(e->lines[file_row].chars, e->lines[file_row].size);
        }

        term_write("\x1b[K", 3);
        if (i < e->rows - 1) {
            term_write("\r\n", 2);
        }
    }
}

void editor_render(Editor* e) {
    term_cursor_hide();
    term_move_cursor(0, 0);

    draw_rows(e);

    i32 screen_y = e->cursor.y - e->offset;

    if (screen_y < 0)
        screen_y = 0;

    if (screen_y >= e->rows)
        screen_y = e->rows - 1;

    i32 screen_x = e->cursor.x;

    if (screen_x < 0)
        screen_x = 0;

    if (screen_x >= e->cols)
        screen_x = e->cols - 1;

    term_move_cursor(screen_y, screen_x);
    term_cursor_show();
}
