#include "editor.h"
#include "buffer.h"
#include "defs.h"
#include "term.h"
#include "view.h"

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>

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

    buffer_init(&e->buffer);

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

void editor_dispatch(Editor* e, Event ev) {
    if (ev.type     == EV_KEY   &&
        ev.key.mods == MOD_CTRL &&
        ev.key.code == 'q'
        ) {
        e->running = false;
        return;
    }

    view_dispatch(&e->view, ev);
}

static void status_draw(Editor* e, Rect r) {
    term_move_cursor(r.y, r.x);
    term_writef("\x1b[0K");
    term_writef("\x1b[7m");
    char status[80], rstatus[80];
    int len = snprintf(status, sizeof(status), "%.20s%s",
            e->view.buf->filename != NULL ? e->view.buf->filename : "[No Name]",
            e->view.buf->dirty ? " *" : "");
    View* v = &e->view;
    i32 col = line_cx_to_rx(&v->buf->lines[v->cursor.line], v->cursor.col);
    int rlen = snprintf(rstatus, sizeof(rstatus), "%d,%d - %d lines",
            v->cursor.line+1,
            col+1,
            e->view.buf->num_lines);
    if (len > r.w) len = r.w;
    term_write(status, len);
    while(len < r.w) {
        if (r.w - len == rlen) {
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

    view_draw(&e->view, e->layout.text);
    status_draw(e, e->layout.status);

    Rect r = e->layout.text;
    Cursor* c = &e->view.cursor;

    i32 cur_col = (e->buffer.num_lines > 0)
        ? line_cx_to_rx(&e->buffer.lines[c->line], c->col)
        : 0;

    i32 screen_y = r.y + c->line - e->view.row_offset;
    i32 screen_x = r.x + cur_col - e->view.col_offset;

    if (screen_y < r.y) screen_y = r.y;
    if (screen_y >= r.y + r.h) screen_y = r.y + r.h - 1;
    if (screen_x < r.x) screen_x = r.x;
    if (screen_x >= r.x + r.w) screen_x = r.x + r.w - 1;

    term_move_cursor(screen_y, screen_x);
    term_cursor_show();
}
