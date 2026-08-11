#include "editor.h"
#include "buffer.h"
#include "defs.h"
#include "term.h"
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#define V_MARGIN 8

static Layout editor_layout(Editor* e) {
    Rect full = { 0, 0, e->cols, e->rows };
    Layout l;
    l.cmd = rect_cut_bottom(&full, 1);
    l.status = rect_cut_bottom(&full, 1);
    l.text = full;
    return l;
}

void editor_init(Editor* e) {
    e->running = true;

    e->buffer = (Buffer) {
        .filename = "[No Name]",
        .num_lines = 0,
        .lines = NULL,
    };

    e->view = (View){
        .buf = &e->buffer,
        .cursor = (Cursor){0, 0, 0},
        .row_offset = 0,
        .col_offset = 0,
    };

    term_get_size(&e->rows, &e->cols);
    e->layout = editor_layout(e);
}

void editor_shutdown(Editor* e) {
    buffer_free(&e->buffer);
}

void editor_open(Editor* e, const char* filepath) {
    buffer_open(&e->buffer, filepath);
}

static void scroll_track(i32* offset, i32 cursor, i32 size, i32 margin) {
    if (cursor < *offset + margin) {
        *offset = cursor - margin;
    }

    if (cursor >= *offset + size - margin) {
        *offset = cursor - size + margin + 1;
    }

    if (*offset < 0)
        *offset = 0;
}

static void view_scroll(View* v, Rect r) {
    i32 margin = V_MARGIN;

    if (margin * 2 >= r.h)
        margin = r.h / 2;

    scroll_track(&v->row_offset, v->cursor.line, r.h, margin);
    scroll_track(&v->col_offset, v->cursor.col, r.w, 0);
}

static void cursor_clamp_col(View* v) {
    i32 len = line_len(v->buf, v->cursor.line);
    v->cursor.col = v->cursor.goal_col;
    if (v->cursor.col > len) v->cursor.col = len;
    if (v->cursor.col < 0)   v->cursor.col = 0;
}

static void view_dispatch(View* v, Event ev, Rect r) {
    if (ev.type != EV_KEY) return;
    if (ev.key.mods == MOD_CTRL) {
        if (ev.key.code == 'w') {
            buffer_save(v->buf);
            return;
        }
    }

    switch (ev.key.code) {
        case KEY_UP:
            if (v->cursor.line > 0) {
                v->cursor.line--;
                cursor_clamp_col(v);
            }
            break;
        case KEY_DOWN:
            if (v->cursor.line < v->buf->num_lines - 1) {
                v->cursor.line++;
                cursor_clamp_col(v);
            }
            break;
        case KEY_LEFT:
            if (v->cursor.col > 0) {
                v->cursor.col--;
            } else if (v->cursor.line > 0) {
                v->cursor.line--;
                v->cursor.col = line_len(v->buf, v->cursor.line);
            }
            v->cursor.goal_col = v->cursor.col;
            break;
        case KEY_RIGHT: {
            i32 len = line_len(v->buf, v->cursor.line);
            if (v->cursor.col < len) {
                v->cursor.col++;
            } else if (v->cursor.line < v->buf->num_lines - 1) {
                v->cursor.line++;
                v->cursor.col = 0;
            }
            v->cursor.goal_col = v->cursor.col;
            break;
        }
        case 'h':
            if (ev.key.mods != MOD_CTRL) {
                buffer_insert_char(v->buf, v->cursor.line, v->cursor.col, ev.key.code);
                v->cursor.col++;
                break;
            }
            /* fall through */
        case KEY_BACKSPACE:
            if (v->cursor.col > 0) {
                buffer_remove_char(v->buf, v->cursor.line, v->cursor.col);
                v->cursor.col--;
            } else if (v->cursor.line > 0) {
                v->cursor.line--;
                v->cursor.col = line_len(v->buf, v->cursor.line);
                v->cursor.goal_col = v->cursor.col;
                buffer_merge_lines(v->buf, v->cursor.line+1);
            }
            break;
        case KEY_DEL:
            if (v->cursor.col < line_len(v->buf, v->cursor.line)) {
                buffer_remove_char(v->buf, v->cursor.line, v->cursor.col+1);
            } else if (v->cursor.line < v->buf->num_lines - 1) {
                buffer_merge_lines(v->buf, v->cursor.line+1);
            }
            break;
        case KEY_ENTER:
            buffer_split_line(v->buf, v->cursor.line, v->cursor.col);
            v->cursor.line++;
            v->cursor.col = 0;
            v->cursor.goal_col = v->cursor.col;
            break;
        default:
            buffer_insert_char(v->buf, v->cursor.line, v->cursor.col, ev.key.code);
            v->cursor.col++;
            break;
    }

    view_scroll(v, r);
}

void editor_dispatch(Editor* e, Event ev) {
    if (ev.type     == EV_KEY   &&
        ev.key.mods == MOD_CTRL &&
        ev.key.code == 'q'
        ) {
        e->running = false;
        return;
    }

    view_dispatch(&e->view, ev, e->layout.text);
}

static void draw_text(View* v, Rect view) {
    for (i32 i = 0; i < view.h; i++) {
        term_move_cursor(view.y+i, view.x);

        i32 file_row = i + v->row_offset;

        if (file_row >= v->buf->num_lines) {
            if (v->buf->num_lines == 0 && i == view.h/3) {
                term_writef("BOM - version %s", BOM_VERSION);
            } else {
                term_write("~", 1);
            }
        } else {
            int len = v->buf->lines[file_row].size - v->col_offset;
            if (len > 0) {
                if (len > view.w) len = view.w;
                term_write(v->buf->lines[file_row].chars + v->col_offset, len);
            }
        }

        term_write("\x1b[39m",5);
        term_write("\x1b[0K",4);
        term_write("\r\n",2);
    }
}

static void draw_status(Editor* e, Rect r) {
    term_move_cursor(r.y, r.x);
    term_writef("\x1b[0K");
    term_writef("\x1b[7m");
    char status[80], rstatus[80];
    int len = snprintf(status, sizeof(status), "%.20s - %d lines", e->buffer.filename, e->buffer.num_lines);
    int rlen = snprintf(rstatus, sizeof(rstatus), "%d,%d", e->view.cursor.line+1, e->buffer.num_lines);
    if (len > r.w) len = r.w;
    term_write(status, len);
    while(len < r.w) {
        if (e->cols - len == rlen) {
            term_write(rstatus, rlen);
            break;
        } else {
            term_write(" ", 1);
            len++;
        }
    }
    term_writef("\x1b[0m");
}

void editor_render(Editor* e) {
    term_cursor_hide();

    draw_text(&e->view, e->layout.text);
    draw_status(e, e->layout.status);

    i32 screen_y = e->view.cursor.line - e->view.row_offset;

    if (screen_y < 0)
        screen_y = 0;

    if (screen_y >= e->rows)
        screen_y = e->rows - 1;

    i32 screen_x = e->view.cursor.col - e->view.col_offset;

    if (screen_x < 0)
        screen_x = 0;

    if (screen_x >= e->cols)
        screen_x = e->cols - 1;

    term_move_cursor(screen_y, screen_x);
    term_cursor_show();
}
