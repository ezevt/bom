#ifndef TERM_H
#define TERM_H

#include <stddef.h>
#include <stdint.h>

enum {
    KEY_NONE = -1,
    KEY_SPECIAL = 0x110000,
    KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT,
};

enum { MOD_CTRL = 1, MOD_ALT = 2, MOD_SHIFT = 4 };

typedef enum { EV_NONE, EV_KEY } EventType;

typedef struct {
    int32_t code; // rune or KEY_*
    uint8_t mods;
} Key;

typedef struct {
    EventType type;
    Key key;
} Event;

int  term_init(void);
void term_shutdown(void);
int  term_get_size(int* rows, int* cols);

Event term_poll(void);

void term_write(const char* s, size_t n);
void term_writef(const char* fmt, ...);
void term_clear(void);
void term_move_cursor(int row, int col);
void term_cursor_hide(void);
void term_cursor_show(void);

#endif
