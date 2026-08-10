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
    e->filename = filepath;

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
    e->cursor = (Cursor){0, 0, 0};
    e->num_lines = 0;
    e->lines = NULL;

    e->row_offset = 0;
    e->col_offset = 0;
    
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

    if (e->cursor.line < e->row_offset + margin) {
        e->row_offset = e->cursor.line - margin;
    }

    if (e->cursor.line >= e->row_offset + e->rows - margin) {
        e->row_offset = e->cursor.line - e->rows + margin + 1;
    }

    if (e->row_offset < 0)
        e->row_offset = 0;

    if (e->cursor.col < e->col_offset) {
        e->col_offset = e->cursor.col;
    }

    if (e->cursor.col >= e->col_offset + e->cols) {
        e->col_offset = e->cursor.col - e->cols + 1;
    }

    if (e->col_offset < 0)
        e->col_offset = 0;
}

static i32 line_len(Editor* e, i32 line) {
    if (line < 0 || line >= e->num_lines) return 0;
    return e->lines[line].size;
}

static void cursor_clamp_col(Editor* e) {
    i32 len = line_len(e, e->cursor.line);
    e->cursor.col = e->cursor.goal_col;
    if (e->cursor.col > len) e->cursor.col = len;
    if (e->cursor.col < 0)   e->cursor.col = 0;
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
            if (e->cursor.line > 0) {
                e->cursor.line--;
                cursor_clamp_col(e);
            }
            break;
        case KEY_DOWN:
            if (e->cursor.line < e->num_lines) {
                e->cursor.line++;
                cursor_clamp_col(e);
            }
            break;
        case KEY_LEFT:
            if (e->cursor.col > 0) {
                e->cursor.col--;
            } else if (e->cursor.line > 0) {
                e->cursor.line--;
                e->cursor.col = line_len(e, e->cursor.line);
            }
            e->cursor.goal_col = e->cursor.col;
            break;
        case KEY_RIGHT: {
            i32 len = line_len(e, e->cursor.line);
            if (e->cursor.col < len) {
                e->cursor.col++;
            } else if (e->cursor.line < e->num_lines - 1) {
                e->cursor.line++;
                e->cursor.col = 0;
            }
            e->cursor.goal_col = e->cursor.col;
            break;
        }
    }

    editor_scroll(e);
}

static void draw_rows(Editor* e) {

    for (i32 i = 0; i < e->rows; i++) {
        i32 file_row = i + e->row_offset;

        if (file_row >= e->num_lines) {
            if (e->num_lines == 0 && i == e->rows/3) {
                term_writef("BOM - version %s", BOM_VERSION);
            } else {
                term_write("~", 1);
            }
        } else {
            int len = e->lines[file_row].size - e->col_offset;
            if (len > 0) {
                if (len > e->cols) len = e->cols;
                term_write(e->lines[file_row].chars + e->col_offset, len);
            }
        }

        term_writef("\x1b[K");
        term_writef("\r\n");
    }

    term_writef("\x1b[0K");
    term_writef("\x1b[7m");
    char status[80], rstatus[80];
    int len = snprintf(status, sizeof(status), "%.20s - %d lines", e->filename, e->num_lines);
    int rlen = snprintf(rstatus, sizeof(rstatus), "%d,%d", e->row_offset+e->cursor.line+1, e->num_lines);
    if (len > e->cols) len = e->cols;
    term_write(status, len);
    while(len < e->cols) {
        if (e->cols - len == rlen) {
            term_write(rstatus, rlen);
            break;
        } else {
            term_write(" ", 1);
            len++;
        }
    }
    term_writef("\x1b[0m\r\n");
}

void editor_render(Editor* e) {
    term_cursor_hide();
    term_move_cursor(0, 0);

    draw_rows(e);

    i32 screen_y = e->cursor.line - e->row_offset;

    if (screen_y < 0)
        screen_y = 0;

    if (screen_y >= e->rows)
        screen_y = e->rows - 1;

    i32 screen_x = e->cursor.col - e->col_offset;

    if (screen_x < 0)
        screen_x = 0;

    if (screen_x >= e->cols)
        screen_x = e->cols - 1;

    term_move_cursor(screen_y, screen_x);
    term_cursor_show();
}
