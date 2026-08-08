#ifndef TERM_H
#define TERM_H

#include <stddef.h>

int term_init(void);
void term_shutdown(void);
int term_get_size(int* rows, int* cols);

void term_write(const char* s, size_t n);
void term_clear(void);
void term_move_cursor(int row, int col);
void term_cursor_hide(void);
void term_cursor_show(void);

#endif
