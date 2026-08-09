#include "editor.h"
#include "defs.h"
#include "term.h"

void editor_init(Editor* e) {
    e->cursor = (Cursor){0, 0};
}

void editor_shutdown(Editor* e) {}

void editor_dispatch(Editor* e, Event ev) {
    if (ev.type == EV_KEY) {
        if (ev.key.code == 'q' && ev.key.mods == MOD_CTRL) {
            term_clear();
            exit(0);
        }
    }
}

static void draw_rows(Editor* e) {
    i32 rows, cols;
    term_get_size(&rows, &cols);

    for (i32 i = 0; i < rows; i++) {
        if (i == rows/3) {
            term_writef("BOM - version %s", BOM_VERSION);
        } else {
            term_write("~", 1);
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
