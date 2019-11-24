#include <kernel.h>

static WINDOW error_window = {
    .x = 0, .y = 24,
    .width = 80, .height = 1,
    .cursor_x = 0, .cursor_y = 0,
    .cursor_char = ' '
};

int failed_assertion(const char *ex, const char *file, int line)
{
    asm("cli");
    clear_window(&error_window);
    wprintf(&error_window, "Failed assertion '%s' at line %d of %s",
            ex, line, file);
    halt();
    return 0;
}

void panic_mode(const char *msg, const char *file, int line)
{
    asm("cli");
    clear_window(&error_window);
    wprintf(&error_window, "PANIC: '%s' at line %d of %s",
            msg, line, file);
    halt();
}

void halt()
{
    asm volatile("\
        cli;\
        hlt;\
        jmp halt;");
}
