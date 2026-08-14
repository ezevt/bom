#include "view.h"
#include "utf8.h"

void view_init(View* v) {
    *v = (View){0};
    v->cursor = (Cursor){0, 0, 0};
}

void view_set_buffer(View *v, Buffer *b) {
    v->buf = b;
    v->cursor = (Cursor){0, 0, 0};
    v->col_offset = v->row_offset = 0;
}

static void view_scroll(View* v, Rect r) {
    i32 margin = V_MARGIN;
    if (margin * 2 >= r.h) margin = r.h / 2;

    scroll_track(&v->row_offset, v->cursor.line, r.h, margin);

    Line* l = &v->buf->lines[v->cursor.line];
    i32 col = line_width(l, v->cursor.col);
    scroll_track(&v->col_offset, col, r.w, 0);
}

static void cursor_clamp_col(View* v) {
    Line* l = &v->buf->lines[v->cursor.line];
    v->cursor.col = line_byte_at(l, v->cursor.goal_col);
}

static void cursor_sync_goal(View* v) {
    Line* l = &v->buf->lines[v->cursor.line];
    v->cursor.goal_col = line_cx_to_rx(l, v->cursor.col);
}

void view_dispatch(View *v, Event ev) {
    if (ev.type != EV_KEY) return;
    if (ev.key.mods == MOD_CTRL) {
        if (ev.key.code == 'w') {
            buffer_save(v->buf);
            return;
        }
    }

    Line* l = v->buf->lines != NULL
        ? &v->buf->lines[v->cursor.line]
        : NULL;

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
                v->cursor.col = utf8_prev(l->chars, v->cursor.col);
            } else if (v->cursor.line > 0) {
                v->cursor.line--;
                v->cursor.col = v->buf->lines[v->cursor.line].size;
            }
            cursor_sync_goal(v);
            break;
        case KEY_RIGHT: {
            i32 len = v->buf->lines[v->cursor.line].size;
            if (v->cursor.col < len) {
                v->cursor.col = utf8_next(l->chars, l->size, v->cursor.col);
            } else if (v->cursor.line < v->buf->num_lines - 1) {
                v->cursor.line++;
                v->cursor.col = 0;
            }
            cursor_sync_goal(v);
            break;
        }
        case KEY_BACKSPACE:
            if (v->cursor.col > 0) {
                i32 prev = utf8_prev(l->chars, v->cursor.col);
                buffer_delete_range(v->buf, v->cursor.line, prev, v->cursor.col);
                v->cursor.col = prev;
            } else if (v->cursor.line > 0) {
                v->cursor.line--;
                v->cursor.col = v->buf->lines[v->cursor.line].size;
                buffer_merge_lines(v->buf, v->cursor.line + 1);
            }
            cursor_sync_goal(v);
            break;
        case KEY_DEL:
            if (v->cursor.col < v->buf->lines[v->cursor.line].size) {
                i32 next = utf8_next(l->chars, l->size, v->cursor.col);
                buffer_delete_range(v->buf, v->cursor.line, v->cursor.col, next);
            } else if (v->cursor.line < v->buf->num_lines - 1) {
                buffer_merge_lines(v->buf, v->cursor.line+1);
            }
            break;
        case KEY_ENTER:
            buffer_split_line(v->buf, v->cursor.line, v->cursor.col);
            v->cursor.line++;
            v->cursor.col = 0;
            v->cursor.goal_col = 0;
            break;
        default: {
            char tmp[4];
            i32 n = utf8_encode(tmp, ev.key.code);
            buffer_insert_text(v->buf, v->cursor.line, v->cursor.col, tmp, n);
            v->cursor.col += n;
            cursor_sync_goal(v);
            break;
        }
    }
}

void view_draw(View* v, Rect r) {
    view_scroll(v, r);

    for (i32 i = 0; i < r.h; i++) {
        term_move_cursor(r.y+i, r.x);

        i32 file_row = i + v->row_offset;

        if (file_row >= v->buf->num_lines) {
            if (v->buf->num_lines == 0 && i == r.h/3) {
                term_writef("BOM - version %s", BOM_VERSION);
            } else {
                term_write("~", 1);
            }
        } else {
            int len = v->buf->lines[file_row].rsize - v->col_offset;
            if (len > 0) {
                if (len > r.w) len = r.w;
                term_write(v->buf->lines[file_row].render + v->col_offset, len);
            }
        }

        term_write("\x1b[39m",5);
        term_write("\x1b[0K",4);
        term_write("\r\n",2);
    }
}
