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
    if (at > e->num_lines) at = e->num_lines;

    e->lines = realloc(e->lines, sizeof(Line) * (e->num_lines + 1));

    if (at != e->num_lines)
        memmove(e->lines + at + 1,
                e->lines + at,
                (e->num_lines - at) * sizeof(Line));


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

        editor_append_line(e, line, linelen, e->num_lines);
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
    e->cursor = (Cursor){0, 0, 0};
    e->num_lines = 0;
    e->lines = NULL;

    e->row_offset = 0;
    e->col_offset = 0;

    e->filename = "[No Name]";
    
    term_get_size(&e->rows, &e->cols);

    e->layout = editor_layout(e);
}

void editor_shutdown(Editor* e) {
    if (e->num_lines <= 0) return;

    for (i32 i = 0; i < e->num_lines; i++) {
        free(e->lines[i].chars);            
    }

    free(e->lines);

    e->lines = NULL;
    e->num_lines = 0;
}

static void editor_scroll(Editor* e, Rect view) {
    i32 margin = V_MARGIN;

    if (margin * 2 >= view.h)
        margin = view.h / 2;

    if (e->cursor.line < e->row_offset + margin) {
        e->row_offset = e->cursor.line - margin;
    }

    if (e->cursor.line >= e->row_offset + view.h - margin) {
        e->row_offset = e->cursor.line - view.h + margin + 1;
    }

    if (e->row_offset < 0)
        e->row_offset = 0;

    if (e->cursor.col < e->col_offset) {
        e->col_offset = e->cursor.col;
    }

    if (e->cursor.col >= e->col_offset + view.w) {
        e->col_offset = e->cursor.col - view.w + 1;
    }

    if (e->col_offset < 0) e->col_offset = 0;
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
    memmove(line->chars + col - 1,
            line->chars + col,
            line->size - col + 1);
}

static void line_merge(Editor* e, i32 line) {
    if (line <= 0 || line >= e->num_lines) return;

    Line* l1 = &e->lines[line-1];
    Line* l2 = &e->lines[line];

    i32 len = l1->size + l2->size;
    char* new = realloc(l1->chars, len + 1);
    if (!new) return;

    memcpy(new + l1->size, l2->chars, l2->size);
    new[len] = '\0';

    l1->chars = new;
    l1->size  = len;

    free(l2->chars);

    memmove(&e->lines[line], &e->lines[line+1],
            (e->num_lines - line - 1) * sizeof(Line));

    e->num_lines--;
}

static void insert_char(Editor* e, i32 c) {
    
    i32 row = e->cursor.line;
    i32 col = e->cursor.col;

    if (row >= e->num_lines) {
        while (row >= e->num_lines) editor_append_line(e, "", 0, e->num_lines);
    }
    
    line_insert_char(&e->lines[row], col, c);
    e->cursor.col++;
    e->cursor.goal_col = e->cursor.col;
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
    char* buf = lines_to_string(e->lines, e->num_lines);

    int fd = open(e->filename, O_RDWR|O_CREAT, 0644);

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
            if (e->cursor.line > 0) {
                e->cursor.line--;
                cursor_clamp_col(e);
            }
            break;
        case KEY_DOWN:
            if (e->cursor.line < e->num_lines - 1) {
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
        case 'h':
            if (ev.key.mods != MOD_CTRL) {
                insert_char(e, ev.key.code);
                break;
            }
        case KEY_BACKSPACE:
            if (e->cursor.col > 0) {
                line_remove_char(&e->lines[e->cursor.line], e->cursor.col);
                e->cursor.col--;
            } else if (e->cursor.line > 0) {
                line_merge(e, e->cursor.line);
                e->cursor.line--;
                e->cursor.col = line_len(e, e->cursor.line);
                e->cursor.goal_col = e->cursor.col;
            }
            break;
        case KEY_DEL:
            if (e->cursor.col < line_len(e, e->cursor.line)) {
                line_remove_char(&e->lines[e->cursor.line], e->cursor.col+1);
            } else if (e->cursor.line < e->num_lines - 1) {
                line_merge(e, e->cursor.line+1);
            }
            break;
        case KEY_ENTER:
            editor_append_line(e, "", 0, e->cursor.line+1);
            e->cursor.line++;
            cursor_clamp_col(e);
            break;
        default:
            insert_char(e, ev.key.code);
            break;
    }

    editor_scroll(e, e->layout.text);
}

static void draw_text(Editor* e, Rect view) {
    for (i32 i = 0; i < view.h; i++) {
        term_move_cursor(view.y+i, view.x);

        i32 file_row = i + e->row_offset;

        if (file_row >= e->num_lines) {
            if (e->num_lines == 0 && i == view.h/3) {
                term_writef("BOM - version %s", BOM_VERSION);
            } else {
                term_write("~", 1);
            }
        } else {
            int len = e->lines[file_row].size - e->col_offset;
            if (len > 0) {
                if (len > view.w) len = view.w;
                term_write(e->lines[file_row].chars + e->col_offset, len);
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
    int len = snprintf(status, sizeof(status), "%.20s - %d lines", e->filename, e->num_lines);
    int rlen = snprintf(rstatus, sizeof(rstatus), "%d,%d", e->cursor.line+1, e->num_lines);
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
