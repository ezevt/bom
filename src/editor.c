#include "editor.h"
#include "defs.h"
#include "term.h"
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#define V_MARGIN 8

static void editor_append_line(Editor* e, char* s, i32 len, i32 at) {
    if (at < 0) at = 0;
    if (at > e->buffer.num_lines) at = e->buffer.num_lines;

    e->buffer.lines = realloc(e->buffer.lines, sizeof(Line) * (e->buffer.num_lines + 1));

    if (at != e->buffer.num_lines)
        memmove(e->buffer.lines + at + 1,
                e->buffer.lines + at,
                (e->buffer.num_lines - at) * sizeof(Line));


    e->buffer.lines[at].size = len;
    e->buffer.lines[at].chars = malloc(len + 1);
    memcpy(e->buffer.lines[at].chars, s, len);
    e->buffer.lines[at].chars[len] = '\0';
    e->buffer.num_lines++;
}

void editor_open(Editor* e, const char* filepath) {
    e->buffer.filename = filepath;

    FILE* fp = fopen(filepath, "r");
    if (!fp) return;

    char* line = NULL;
    size_t linecap = 0;
    ssize_t linelen;

    while ((linelen = getline(&line, &linecap, fp)) != -1) {
        while (linelen > 0 && (line[linelen - 1] == '\n' ||
                               line[linelen - 1] == '\r'))
            linelen--;

        editor_append_line(e, line, linelen, e->buffer.num_lines);
    }

    free(line);
    fclose(fp);
}

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

    if (e->buffer.num_lines > 0) {
        for (i32 i = 0; i < e->buffer.num_lines; i++) {
            free(e->buffer.lines[i].chars);            
        }
    }

    if (e->buffer.lines != NULL) {
        free(e->buffer.lines);
        e->buffer.lines = NULL;
        e->buffer.num_lines = 0;
    }
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

static void editor_scroll(Editor* e, View* v, Rect r) {
    i32 margin = V_MARGIN;

    if (margin * 2 >= r.h)
        margin = r.h / 2;

    scroll_track(&v->row_offset, v->cursor.line, r.h, margin);
    scroll_track(&v->col_offset, v->cursor.col, r.w, 0);
}

static i32 line_len(Buffer* b, i32 line) {
    if (line < 0 || line >= b->num_lines) return 0;
    return b->lines[line].size;
}

static void cursor_clamp_col(Editor* e) {
    i32 len = line_len(&e->buffer, e->view.cursor.line);
    e->view.cursor.col = e->view.cursor.goal_col;
    if (e->view.cursor.col > len) e->view.cursor.col = len;
    if (e->view.cursor.col < 0)   e->view.cursor.col = 0;
}

static void line_insert_char(Line* line, i32 col, i32 c) {
    if (col > line->size) col = line->size;
    if (col < 0) col = 0;

    char* new = realloc(line->chars, line->size + 2);

    memmove(new + col + 1,
            new + col,
            line->size - col + 1);

    new[col] = c;
    line->chars = new;
    line->size++;
}

static void line_remove_char(Line* line, i32 col) {
    if (col <= 0 || col >= line->size) return;
    memmove(line->chars + col - 1,
            line->chars + col,
            line->size - col + 1);
    line->size--;
}

static void line_merge(Editor* e, i32 line) {
    if (line <= 0 || line >= e->buffer.num_lines) return;

    Line* l1 = &e->buffer.lines[line-1];
    Line* l2 = &e->buffer.lines[line];

    i32 len = l1->size + l2->size;
    char* new = realloc(l1->chars, len + 1);
    if (!new) return;

    memcpy(new + l1->size, l2->chars, l2->size);
    new[len] = '\0';

    l1->chars = new;
    l1->size  = len;

    free(l2->chars);

    memmove(&e->buffer.lines[line], &e->buffer.lines[line+1],
            (e->buffer.num_lines - line - 1) * sizeof(Line));

    e->buffer.num_lines--;
}

static void insert_char(Editor* e, i32 c) {
    
    i32 row = e->view.cursor.line;
    i32 col = e->view.cursor.col;

    if (row >= e->buffer.num_lines) {
        while (row >= e->buffer.num_lines) editor_append_line(e, "", 0, e->buffer.num_lines);
    }
    
    line_insert_char(&e->buffer.lines[row], col, c);
    e->view.cursor.col++;
    e->view.cursor.goal_col = e->view.cursor.col;
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

static void save_file(Editor* e) {
    char* buf = lines_to_string(e->buffer.lines, e->buffer.num_lines);

    int fd = open(e->buffer.filename, O_RDWR|O_CREAT, 0644);

    ftruncate(fd, 0);
    write(fd, buf, strlen(buf));

    close(fd);
    free(buf);
}

void editor_dispatch(Editor* e, Event ev) {
    if (ev.type != EV_KEY) return;
    if (ev.key.mods == MOD_CTRL) {
        if (ev.key.code == 'q') {
            e->running = false;
            return;
        }

        if (ev.key.code == 'w') {
            save_file(e);
            return;
        }
    }

    switch (ev.key.code) {
        case KEY_UP:
            if (e->view.cursor.line > 0) {
                e->view.cursor.line--;
                cursor_clamp_col(e);
            }
            break;
        case KEY_DOWN:
            if (e->view.cursor.line < e->buffer.num_lines - 1) {
                e->view.cursor.line++;
                cursor_clamp_col(e);
            }
            break;
        case KEY_LEFT:
            if (e->view.cursor.col > 0) {
                e->view.cursor.col--;
            } else if (e->view.cursor.line > 0) {
                e->view.cursor.line--;
                e->view.cursor.col = line_len(&e->buffer, e->view.cursor.line);
            }
            e->view.cursor.goal_col = e->view.cursor.col;
            break;
        case KEY_RIGHT: {
            i32 len = line_len(&e->buffer, e->view.cursor.line);
            if (e->view.cursor.col < len) {
                e->view.cursor.col++;
            } else if (e->view.cursor.line < e->buffer.num_lines - 1) {
                e->view.cursor.line++;
                e->view.cursor.col = 0;
            }
            e->view.cursor.goal_col = e->view.cursor.col;
            break;
        }
        case 'h':
            if (ev.key.mods != MOD_CTRL) {
                insert_char(e, ev.key.code);
                break;
            }
        case KEY_BACKSPACE:
            if (e->view.cursor.col > 0) {
                line_remove_char(&e->buffer.lines[e->view.cursor.line], e->view.cursor.col);
                e->view.cursor.col--;
            } else if (e->view.cursor.line > 0) {
                e->view.cursor.line--;
                e->view.cursor.col = line_len(&e->buffer, e->view.cursor.line);
                e->view.cursor.goal_col = e->view.cursor.col;
                line_merge(e, e->view.cursor.line+1);
            }
            break;
        case KEY_DEL:
            if (e->view.cursor.col < line_len(&e->buffer, e->view.cursor.line)) {
                line_remove_char(&e->buffer.lines[e->view.cursor.line], e->view.cursor.col+1);
            } else if (e->view.cursor.line < e->buffer.num_lines - 1) {
                line_merge(e, e->view.cursor.line+1);
            }
            break;
        case KEY_ENTER:
            editor_append_line(e, "", 0, e->view.cursor.line+1);
            e->view.cursor.line++;
            cursor_clamp_col(e);
            break;
        default:
            insert_char(e, ev.key.code);
            break;
    }

    editor_scroll(e, &e->view, e->layout.text);
}

static void draw_text(Editor* e, Rect view) {
    for (i32 i = 0; i < view.h; i++) {
        term_move_cursor(view.y+i, view.x);

        i32 file_row = i + e->view.row_offset;

        if (file_row >= e->buffer.num_lines) {
            if (e->buffer.num_lines == 0 && i == view.h/3) {
                term_writef("BOM - version %s", BOM_VERSION);
            } else {
                term_write("~", 1);
            }
        } else {
            int len = e->buffer.lines[file_row].size - e->view.col_offset;
            if (len > 0) {
                if (len > view.w) len = view.w;
                term_write(e->buffer.lines[file_row].chars + e->view.col_offset, len);
            }
        }

        term_write("\x1b[39m",5);
        term_write("\x1b[0K",4);
        term_write("\r\n",2);
    }
}

static void draw_status(Editor* e, Rect view) {
    term_move_cursor(view.y, view.x);
    term_writef("\x1b[0K");
    term_writef("\x1b[7m");
    char status[80], rstatus[80];
    int len = snprintf(status, sizeof(status), "%.20s - %d lines", e->buffer.filename, e->buffer.num_lines);
    int rlen = snprintf(rstatus, sizeof(rstatus), "%d,%d", e->view.cursor.line+1, e->buffer.num_lines);
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
    term_writef("\x1b[0m");
}

void editor_render(Editor* e) {
    term_cursor_hide();

    draw_text(e, e->layout.text);
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
