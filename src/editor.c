#include "editor.h"
#include "defs.h"
#include "term.h"
#include <stdio.h>
#include <string.h>

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
}

void editor_shutdown(Editor* e) {
    if (e->num_lines <= 0) return;

    printf("freeing %d lines \n", e->num_lines);

    for (i32 i = 0; i < e->num_lines; i++) {
        free(e->lines[i].chars);
    }

    free(e->lines);
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
            e->cursor.y--;
            break;
        case KEY_DOWN:
            e->cursor.y++;
            break;
        case KEY_LEFT:
            e->cursor.x--;
            break;
        case KEY_RIGHT:
            e->cursor.x++;
            break;
    }
}

static void draw_rows(Editor* e) {
    i32 rows, cols;
    term_get_size(&rows, &cols);

    for (i32 i = 0; i < rows; i++) {
        if (i >= e->num_lines) {
            if (e->num_lines == 0 && i == rows/3) {
                term_writef("BOM - version %s", BOM_VERSION);
            } else {
                term_write("~", 1);
            }
        } else {
            int len = e->lines[i].size;
            if (len > cols) len = cols;
            term_write(e->lines[i].chars, e->lines[i].size);
        }

        term_write("\x1b[K", 3);
        if (i < rows - 1) {
            term_write("\r\n", 2);
        }
    }
}

void editor_render(Editor* e) {
    term_cursor_hide();
    term_move_cursor(0, 0);

    draw_rows(e);

    term_move_cursor(e->cursor.y, e->cursor.x);
    term_cursor_show();
}
