#include "editor.h"

int main(void) {
    Editor e = {0};
    
    term_init();
    editor_init(&e);

    while (1) {
        Event ev = term_poll();

        editor_dispatch(&e, ev);
        editor_render(&e);
        
        term_flush();
    }

    term_shutdown();
    return 0;
}
