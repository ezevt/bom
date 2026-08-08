#include "term.h"

int main(void) {
    term_init();

    while (1) {
        Event ev = term_poll();

        int w, h;
        term_get_size(&h, &w);
        term_writef("gola");

        term_writef("w: %d, h: %d \r\n", w, h);

        if (ev.type == EV_KEY) {
            if (ev.key.code == 'q' && ev.key.mods == MOD_CTRL) break;
        }
    }

    term_shutdown();
    return 0;
}
