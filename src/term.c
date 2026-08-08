#include "term.h"

#include <errno.h>
#include <sys/ioctl.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

static struct termios orig;
static i32 raw_active = 0;

i32 term_init(void) {
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
    term_writef("\x1b[?1049h");
    
    return 0;
}

void term_shutdown(void) {
    if (!raw_active) return;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig);
    raw_active = 0;
}

static i32 get_cursor_position(int* rows, int* cols) {
    char buf[32];
    unsigned int i = 0;

    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;

    while (i < sizeof(buf) - 1) {
        if (read(STDIN_FILENO, &buf[i], 1) != 1) break;
        if (buf[i] == 'R') break;
        i++;
    }

    buf[i] = '\0';

    printf("\r\n&buf[1]: '%s'\r\n", &buf[1]);

    if (buf[0] != '\x1b' || buf[1] != '[') return -1;
    if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;

    return 0;
}

i32 term_get_size(i32* rows, i32* cols) {
    struct winsize ws;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
        return get_cursor_position(rows, cols);
    } else {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
}

static Event none_event(void) {
    return (Event){0};
}

static Event key_event(i32 code, b8 mods) {
    return (Event){
        .type = EV_KEY,
        .key = {
            .code = code,
            .mods = mods
        }
    };
}

Event term_poll(void) {
    i32 nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN) { 
            return none_event();
        }
    }

    if (c < 0x20) return key_event(c + 'a' - 1, MOD_CTRL);

    return key_event(c, 0);
}

void term_write(const char* s, size_t n) {
    write(STDOUT_FILENO, s, n);
}

void term_writef(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);

    char tmp[128];
    int n = vsnprintf(tmp, sizeof(tmp), fmt, ap);
    term_write(tmp, (size_t)n < sizeof(tmp) ? (size_t)n : sizeof tmp - 1);
}

void term_clear(void) {
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
}

void term_move_cursor(i32 row, i32 col) {
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
