#include "editor.h"

int main(i32 argc, const char** argv) {
    Editor e = {0};

    term_init();
    editor_init(&e);

    if (argc >= 2) {
        editor_open(&e, argv[1]);
    }

    while (e.running) {
        editor_render(&e);
        term_flush();

        Event ev = term_poll();

        editor_dispatch(&e, ev);
        
    }

    editor_shutdown(&e);
    term_shutdown();
    return 0;
}
