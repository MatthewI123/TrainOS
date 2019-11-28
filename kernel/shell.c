#include <kernel.h>

// Reads keyboard input up until a newline character. 
// The newline character is discarded.
int get_line(int window_id, char* dest, int length)
{
    int read;

    for (read = 0; read < length - 1; ++read) {
        char ch = keyb_get_keystroke(window_id, TRUE);

        if (ch == '\n')
            break;
        else
            *(dest++) = ch;

        wm_print(window_id, "%c", ch);
    }

    dest[read] = 0;
    return read;
}

void start_shell()
{
    int window = wm_create(20, 5, 40, 15);
    char line[64];
    
    wm_clear(window);
    wm_print(window, "TOS Shell\nExecute command `help` for help.\n\n");

    while (1) {
        wm_print(window, "> ");
        get_line(window, line, sizeof(line));
        wm_print(window, "\n");
        
        if (k_memcmp(line, "help", 5) == 0) {
            wm_print(window, "help\n");
        } else {
            wm_print(window, "unknown command.\n");
        }
    }

    halt();
}
