#ifndef TERM_H
#define TERM_H

#include "defs.h"

enum {
    KEY_NONE = -1,
    KEY_SPECIAL = 0x110000,
    KEY_ESC, KEY_TAB, KEY_ENTER, KEY_BACKSPACE, KEY_DEL,
    KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT,
    KEY_HOME, KEY_END, KEY_PGUP, KEY_PGDN,

};

enum { MOD_CTRL = 1, MOD_ALT = 2, MOD_SHIFT = 4 };

typedef enum { EV_NONE, EV_KEY } EventType;

typedef struct {
    b32 code; // rune or KEY_*
    b8 mods;
} Key;

typedef struct {
    EventType type;
    Key key;
} Event;

i32  term_init(void);
void term_shutdown(void);
i32  term_get_size(i32* rows, i32* cols);

Event term_poll(void);

void term_write(const char* s, i32 n);
void term_writef(const char* fmt, ...);
void term_flush(void);
void term_clear(void);
void term_move_cursor(i32 row, i32 col);
void term_cursor_hide(void);
void term_cursor_show(void);

#endif
