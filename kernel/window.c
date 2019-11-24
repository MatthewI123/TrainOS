
#include <kernel.h>

#define SCREEN_BASE 0xB8000
#define ROWS 25
#define COLS 80
#define RESOLVE(wnd, x, y) COLS*(wnd->y + y) + wnd->x + x

enum FG_FLAGS
{
    FG_R = 0x01,
    FG_G = 0x02,
    FG_B = 0x04,
    FG_BRIGHT = 0x08,
    FG_BLACK = 0x00,
    FG_WHITE = 0x0F // bright by default
};

enum BG_FLAGS
{
    BG_R = 0x10,
    BG_G = 0x20,
    BG_B = 0x40,
    BG_BLINK = 0x80,
    BG_BLACK = 0x00,
    BG_WHITE = 0x70
};

#define BLANK BG_BLACK | FG_BLACK

typedef struct char_t
{
    char chr;
    char attrib;
} __attribute__((packed)) char_t;

char_t* video_memory = (char_t*)SCREEN_BASE;

static char_t read_char(WINDOW * wnd, int x, int y)
{
    assert(x >= 0 && x < wnd->width);
    assert(y >= 0 && y < wnd->height);
    return video_memory[RESOLVE(wnd, x, y)];
}

static void write_char(WINDOW * wnd, int x, int y, char_t chr)
{
    assert(x >= 0 && x < wnd->width);
    assert(y >= 0 && y < wnd->height);
    video_memory[RESOLVE(wnd, x, y)] = chr;
}

void move_cursor(WINDOW * wnd, int x, int y)
{
    assert(x >= 0 && x < wnd->width);
    assert(y >= 0 && y < wnd->height);
    wnd->cursor_x = x;
    wnd->cursor_y = y;
}


void remove_cursor(WINDOW * wnd)
{
    assert(wnd->cursor_x >= 0 && wnd->cursor_x < wnd->width);
    assert(wnd->cursor_y >= 0 && wnd->cursor_y < wnd->height);
    write_char(wnd, wnd->cursor_x, wnd->cursor_y, (char_t){ .attrib = BLANK });
}


void show_cursor(WINDOW * wnd)
{
    assert(wnd->cursor_x >= 0 && wnd->cursor_x < wnd->width);
    assert(wnd->cursor_y >= 0 && wnd->cursor_y < wnd->height);
    write_char(wnd, wnd->cursor_x, wnd->cursor_y, (char_t){
        .chr = wnd->cursor_char, .attrib = FG_WHITE });
}


void clear_window(WINDOW * wnd)
{
    volatile int saved_if;
    DISABLE_INTR(saved_if);
    for (int i = 0; i < wnd->width; ++i)
        for (int j = 0; j < wnd->height; ++j)
            write_char(wnd, i, j, (char_t){ .attrib = BLANK });
    
    wnd->cursor_x = 0;
    wnd->cursor_y = 0;
    show_cursor(wnd);
    ENABLE_INTR(saved_if);
}

static void prev_line(WINDOW * wnd)
{
    assert(wnd->cursor_x >= 0 && wnd->cursor_x < wnd->width);
    assert(wnd->cursor_y >= 0 && wnd->cursor_y < wnd->height);

    if (wnd->cursor_y > 0) {
        wnd->cursor_x = wnd->width - 1;
        wnd->cursor_y -= 1;
    } else {
        wnd->cursor_x = 0;
    }
}

static void next_line(WINDOW * wnd)
{
    assert(wnd->cursor_y >= 0 && wnd->cursor_y < wnd->height);

    if (wnd->cursor_y == wnd->height - 1) {
        for (int i = 0; i < wnd->width; ++i)
            for (int j = 0; j < wnd->height - 1; ++j)
                write_char(wnd, i, j, read_char(wnd, i, j + 1));
        
        for (int i = 0; i < wnd->width; ++i)
            write_char(wnd, i, wnd->height - 1, (char_t){ .attrib = BLANK });
    } else {
        wnd->cursor_y += 1;
    }

    wnd->cursor_x = 0;
}

void output_char(WINDOW * wnd, unsigned char c)
{
    volatile int saved_if;
    DISABLE_INTR(saved_if);
    assert(wnd->cursor_x >= 0 && wnd->cursor_x < wnd->width);
    assert(wnd->cursor_y >= 0 && wnd->cursor_y < wnd->height);

    switch (c) {
        case '\r':
        case '\n':
            next_line(wnd);
            break;
        case '\b':
            if (wnd->cursor_x == 0)
                prev_line(wnd);
            else
                wnd->cursor_x -= 1;
            break;
        default:
            write_char(wnd, wnd->cursor_x, wnd->cursor_y, (char_t){ .chr = c,
                .attrib = FG_WHITE });
            wnd->cursor_x += 1;
            break;
    }

    if (wnd->cursor_x == wnd->width)
        next_line(wnd);
    show_cursor(wnd);
    ENABLE_INTR(saved_if);
}



void output_string(WINDOW * wnd, const char *str)
{
    while (*str)
        output_char(wnd, *str++);
}



/* 
 * There is not need to make any changes to the code below,
 * however, you are encouraged to at least look at it!
 */
#define MAXBUF (sizeof(long int) * 8)   /* enough for binary */

char           *printnum(char *b, unsigned int u, int base,
                         BOOL negflag, int length, BOOL ladjust,
                         char padc, BOOL upcase)
{
    char            buf[MAXBUF];        /* build number here */
    char           *p = &buf[MAXBUF - 1];
    int             size;
    char           *digs;
    static char     up_digs[] = "0123456789ABCDEF";
    static char     low_digs[] = "0123456789abcdef";

    digs = upcase ? up_digs : low_digs;
    do {
        *p-- = digs[u % base];
        u /= base;
    } while (u != 0);

    if (negflag)
        *b++ = '-';

    size = &buf[MAXBUF - 1] - p;

    if (size < length && !ladjust) {
        while (length > size) {
            *b++ = padc;
            length--;
        }
    }

    while (++p != &buf[MAXBUF])
        *b++ = *p;

    if (size < length) {
        /* must be ladjust */
        while (length > size) {
            *b++ = padc;
            length--;
        }
    }
    return b;
}


/* 
 *  This version implements therefore following printf features:
 *
 *      %d      decimal conversion
 *      %u      unsigned conversion
 *      %x      hexadecimal conversion
 *      %X      hexadecimal conversion with capital letters
 *      %o      octal conversion
 *      %c      character
 *      %s      string
 *      %m.n    field width, precision
 *      %-m.n   left adjustment
 *      %0m.n   zero-padding
 *      %*.*    width and precision taken from arguments
 *
 *  This version does not implement %f, %e, or %g.  It accepts, but
 *  ignores, an `l' as in %ld, %lo, %lx, and %lu, and therefore will not
 *  work correctly on machines for which sizeof(long) != sizeof(int).
 *  It does not even parse %D, %O, or %U; you should be using %ld, %o and
 *  %lu if you mean long conversion.
 *
 *  This version implements the following nonstandard features:
 *
 *      %b      binary conversion
 *
 */


#define isdigit(d) ((d) >= '0' && (d) <= '9')
#define ctod(c) ((c) - '0')


int vsprintf(char *buf, const char *fmt, va_list argp)
{
    char           *p;
    char           *p2;
    int             length;
    int             prec;
    int             ladjust;
    char            padc;
    int             n;
    unsigned int    u;
    int             negflag;
    char            c;
    char           *start_buf = buf;

    while (*fmt != '\0') {
        if (*fmt != '%') {
            *buf++ = *fmt++;
            continue;
        }
        fmt++;
        if (*fmt == 'l')
            fmt++;              /* need to use it if sizeof(int) <
                                 * sizeof(long) */

        length = 0;
        prec = -1;
        ladjust = FALSE;
        padc = ' ';

        if (*fmt == '-') {
            ladjust = TRUE;
            fmt++;
        }

        if (*fmt == '0') {
            padc = '0';
            fmt++;
        }

        if (isdigit(*fmt)) {
            while (isdigit(*fmt))
                length = 10 * length + ctod(*fmt++);
        } else if (*fmt == '*') {
            length = va_arg(argp, int);
            fmt++;
            if (length < 0) {
                ladjust = !ladjust;
                length = -length;
            }
        }

        if (*fmt == '.') {
            fmt++;
            if (isdigit(*fmt)) {
                prec = 0;
                while (isdigit(*fmt))
                    prec = 10 * prec + ctod(*fmt++);
            } else if (*fmt == '*') {
                prec = va_arg(argp, int);
                fmt++;
            }
        }

        negflag = FALSE;

        switch (*fmt) {
        case 'b':
        case 'B':
            u = va_arg(argp, unsigned int);
            buf = printnum(buf, u, 2, FALSE, length, ladjust, padc, 0);
            break;

        case 'c':
            c = va_arg(argp, int);
            *buf++ = c;
            break;

        case 'd':
        case 'D':
            n = va_arg(argp, int);
            if (n >= 0)
                u = n;
            else {
                u = -n;
                negflag = TRUE;
            }
            buf = printnum(buf, u, 10, negflag, length, ladjust, padc, 0);
            break;

        case 'o':
        case 'O':
            u = va_arg(argp, unsigned int);
            buf = printnum(buf, u, 8, FALSE, length, ladjust, padc, 0);
            break;

        case 's':
            p = va_arg(argp, char *);
            if (p == (char *) 0)
                p = "(NULL)";
            if (length > 0 && !ladjust) {
                n = 0;
                p2 = p;
                for (; *p != '\0' && (prec == -1 || n < prec); p++)
                    n++;
                p = p2;
                while (n < length) {
                    *buf++ = ' ';
                    n++;
                }
            }
            n = 0;
            while (*p != '\0') {
                if (++n > prec && prec != -1)
                    break;
                *buf++ = *p++;
            }
            if (n < length && ladjust) {
                while (n < length) {
                    *buf++ = ' ';
                    n++;
                }
            }
            break;

        case 'u':
        case 'U':
            u = va_arg(argp, unsigned int);
            buf = printnum(buf, u, 10, FALSE, length, ladjust, padc, 0);
            break;

        case 'x':
            u = va_arg(argp, unsigned int);
            buf = printnum(buf, u, 16, FALSE, length, ladjust, padc, 0);
            break;

        case 'X':
            u = va_arg(argp, unsigned int);
            buf = printnum(buf, u, 16, FALSE, length, ladjust, padc, 1);
            break;

        case '\0':
            fmt--;
            break;

        default:
            *buf++ = *fmt;
        }
        fmt++;
    }
    *buf = '\0';
    return buf - start_buf;
}



void wprintf(WINDOW * wnd, const char *fmt, ...)
{
    va_list         argp;
    char            buf[160];

    va_start(argp, fmt);
    vsprintf(buf, fmt, argp);
    output_string(wnd, buf);
    va_end(argp);
}




static WINDOW   kernel_window_def = { 0, 0, 80, 25, 0, 0, ' ' };
WINDOW         *kernel_window = &kernel_window_def;


void kprintf(const char *fmt, ...)
{
    va_list         argp;
    char            buf[160];

    va_start(argp, fmt);
    vsprintf(buf, fmt, argp);
    output_string(kernel_window, buf);
    va_end(argp);
}


