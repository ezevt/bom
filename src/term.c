#include "term.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

static struct termios orig;
static int raw_active = 0;

int term_init(void) {
    if (!isatty(STDIN_FILENO)) return -1;
    if (tcgetattr(STDIN_FILENO, &orig) == -1) return -1;

    struct termios raw = orig;
    
    raw.c_iflag &= ~(ICRNL | IXON | BRKINT | INPCK | ISTRIP);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= CS8;

    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1; // 100ms

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) return -1;
    raw_active = 1;

    atexit(term_shutdown);
    
    return 0;
}

void term_shutdown(void) {
    if (!raw_active) return;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig);
    raw_active = 0;
}

void term_write(const char* s, size_t n) {
    write(STDOUT_FILENO, s, n);
}

void term_clear(void) {
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
}

void term_move_cursor(int row, int col) {
    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", row + 1, col + 1);
    term_write(buf, strlen(buf));
}

void term_cursor_hide(void) {
    term_write("\x1b[?25l", 6);
}

void term_cursor_show(void) {
    term_write("\x1b[?25h", 6);
}
