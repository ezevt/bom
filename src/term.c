#include "term.h"

#include <sys/ioctl.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

static struct termios orig;
static i32 raw_active = 0;

static char* abuf;
static i32 alen;

i32 term_init(void) {
    if (!isatty(STDIN_FILENO)) return -1;
    if (tcgetattr(STDIN_FILENO, &orig) == -1) return -1;

    struct termios raw = orig;
    
    raw.c_iflag &= ~(ICRNL | IXON | BRKINT | INPCK | ISTRIP);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= CS8;

    raw.c_cc[VTIME] = 1; // 100ms

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) return -1;
    raw_active = 1;

    abuf = 0;
    alen = 0;

    atexit(term_shutdown);
    term_writef("\x1b[?1049h");
    
    return 0;
}

void term_shutdown(void) {
    term_flush();
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

static u8 ibuf[32];
static i32 ilen = 0, ipos = 0;

static i32 next_byte(void) {
    if (ipos < ilen) return ibuf[ipos++];
    
    i32 n = read(STDIN_FILENO, ibuf, sizeof(ibuf));
    if (n <= 0) {
        ilen = 0;
        ipos = 0;
        return -1;
    }
    
    ilen = n;
    ipos = 0;
    
    return ibuf[ipos++];
}

static i32 peek_byte(void) {
    if (ipos < ilen) return ibuf[ipos];
    
    i32 n = read(STDIN_FILENO, ibuf, sizeof(ibuf));
    if (n <= 0) {
        ilen = 0;
        ipos = 0;
        return -1;
    }

    ilen = n;
    ipos = 0;

    return ibuf[ipos];
}

static Event parse_csi(void) {
    i32 params[4] = {0}, np = 0;
    i32 c;
    for (;;) {
        c = next_byte();
        if (c < 0) return none_event();
        if (c >= '0' && c <= '9') {
            if (np == 0) np = 1;
            params[np-1] = params[np-1] * 10 + (c - '0');
        } else if (c == ';') {
            if (np < 4) np++;
        } else break;
    }

    b8 mods = 0;
    if (np >= 2 && params[1] > 0) {
        i32 m = params[1] - 1;
        if (m & 1) mods |= MOD_SHIFT;
        if (m & 2) mods |= MOD_ALT;
        if (m & 4) mods |= MOD_CTRL;
    }

    switch (c) {
    case 'A': return key_event(KEY_UP, mods);
    case 'B': return key_event(KEY_DOWN, mods);
    case 'C': return key_event(KEY_RIGHT, mods);
    case 'D': return key_event(KEY_LEFT, mods);
    case 'H': return key_event(KEY_HOME, mods);
    case 'F': return key_event(KEY_END, mods);
    case '~':
        switch (params[0]) {
        case 1: case 7: return key_event(KEY_HOME, mods);
        case 4: case 8: return key_event(KEY_END, mods);
        case 3: return key_event(KEY_DEL, mods);
        case 5: return key_event(KEY_PGUP, mods);
        case 6: return key_event(KEY_PGDN, mods);
        }
    }

    return none_event();
}

static Event decode_utf8(i32 first) {
    i32 len, cp;
    if      ((first & 0x80) == 0)    return key_event(first, 0);
    else if ((first & 0xE0) == 0xC0) { len = 1; cp = first & 0x1F; }
    else if ((first & 0xF0) == 0xE0) { len = 2; cp = first & 0x0F; }
    else if ((first & 0xF8) == 0xF0) { len = 3; cp = first & 0x07; }
    else return none_event();

    for (i32 i = 0; i < len; i++) {
        i32 b = next_byte();
        if (b < 0 || (b & 0xC0) != 0x80) return none_event();
        cp = (cp << 6) | (b & 0x3F);
    }
    return key_event(cp, 0);
}

Event term_poll(void) {
    i32 c = next_byte();
    if (c < 0) return none_event();

    if (c == 0x1b) {
        i32 n = peek_byte();
        
        if (n < 0) return key_event(KEY_ESC, 0);

        if (n == '[') {
            ipos++;
            return parse_csi();
        }

        ipos++;
        Event e = decode_utf8(n);

        if (e.type == EV_KEY) e.key.mods |= MOD_ALT;

        return e;
    }

    if (c == KEY_TAB || c == KEY_ENTER) return key_event(c, 0);
    if (c == KEY_BACKSPACE) return key_event(KEY_BACKSPACE, 0);
    if (c < 0x20) return key_event(c + 'a' - 1, MOD_CTRL);

    return decode_utf8(c);
}

static void abuf_append(const char* s, i32 len) {
    char* new = realloc(abuf, alen + len);

    if (new == NULL) return;
    memcpy(&new[alen], s, len);
    abuf = new;
    alen += len;
}

void term_write(const char* s, i32 n) {
    abuf_append(s, n);
}

void term_writef(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);

    char tmp[128];
    int n = vsnprintf(tmp, sizeof(tmp), fmt, ap);
    term_write(tmp, (size_t)n < sizeof(tmp) ? (size_t)n : sizeof tmp - 1);

    va_end(ap);
}

void term_flush(void) {
    write(STDOUT_FILENO, abuf, alen);
    free(abuf);
    abuf = NULL;
    alen = 0;
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
