// Matthew I - 916771224
// to-do: window resizing?
// to-do: refactor into multiple files after submission (ideally, implement a subset of the C stdlib)

#include <kernel.h>

#define HISTORY_SIZE 10
#define BUFFER_SIZE 64

static int get_line(int window_id, char* buff, int size);
static int find(const char* buff, int size, char chr);
static int handle_command(int window_id, const char* buff, char* history[HISTORY_SIZE]);
static void print_processes(int wnd);

// shell process, deals with tokenizing input and forwarding each command to handle_command.
// each shell has its own history which can be seen via the `history` command.
void shell_process(PROCESS self, PARAM param)
{
	char* history[HISTORY_SIZE] = { NULL };
	int last_history = 0;
	int window_id = wm_create(5, 5, 70, 15);
	char line[BUFFER_SIZE];
	
	wm_clear(window_id);
	wm_print(window_id, "TOS Shell\nExecute command `help` for help.\n\n");

	while (1) {
		char* current = line;
		int idx = 0;

		wm_print(window_id, "> ");
		get_line(window_id, line, sizeof(line));
		wm_print(window_id, "\n");

		if (last_history == HISTORY_SIZE - 1) {
			for (int idx2 = 0; idx2 < HISTORY_SIZE - 1; ++idx2)
				history[idx2] = history[idx2 + 1];
			free(history[last_history]);
			history[last_history] = malloc(k_strlen(line) + 1);
			k_memcpy(history[last_history], line, k_strlen(line) + 1);
		} else {
			history[last_history] = malloc(k_strlen(line) + 1);
			k_memcpy(history[last_history], line, k_strlen(line) + 1);
			last_history += 1;
		}

		while (current - line < sizeof(line) - 1) {
			idx = find(current, sizeof(line) - idx, ';');

			if (idx != sizeof(line) - idx) 
				current[idx] = 0;
			
			if (!handle_command(window_id, current, history)) {
				wm_print(window_id, "unknown command %s\n", current);
				break;
			}

			current += idx + 1;
		}
	}

	halt();
}

void start_shell() {
	create_process(shell_process, 1, 0, "Shell Process");
}

// reads a line, returns the number of characters read.
int get_line(int window_id, char* dest, int length)
{
	const char* begin = dest;

	while (dest - begin < length - 1) {
		char chr = keyb_get_keystroke(window_id, TRUE);

		if (chr == '\b') {
			if (dest - begin > 0) {
				dest -= 1;
				wm_print(window_id, "%c", chr);
			}
		} else if (chr == '\r' || chr == '\n') {
			break;
		} else if (chr == '\t') {
			continue;
		} else {
			*dest++ = chr;
			wm_print(window_id, "%c", chr);
		}
	}

	*dest = 0;

	return dest - begin;
}

// finds and returns the index of the first occurance of chr in buff
// returns size if no occurance found
int find(const char* buff, int size, char chr)
{
	for (int idx = 0; idx < size; ++idx)
		if (buff[idx] == chr)
			return idx;
	
	return size;
}

// invokes the given command (in buff).
// returns 0 if the command does not exist, 1 otherwise.
int handle_command(int window_id, const char* buff, char* history[HISTORY_SIZE])
{
	if (k_memcmp(buff, "about", sizeof("about")) == 0) {
		wm_print(window_id, "TOS Shell - Matthew I\n");
	} else if (k_memcmp(buff, "help", sizeof("help")) == 0) {
		wm_print(window_id, "TOS Shell - Commands\n");
		wm_print(window_id, "about  Displays information.\n");
		wm_print(window_id, "help  Displays this help message.\n");
		wm_print(window_id, "clear  Clears the console.\n");
		wm_print(window_id, "pong  Opens pong.\n");
        wm_print(window_id, "train  Runs the train application.\n");
		wm_print(window_id, "shell  Opens another shell instance.\n");
		wm_print(window_id, "echo [...]  Prints message.\n");
		wm_print(window_id, "ps  Displays processes.\n");
		wm_print(window_id, "history  Shows recent command history.\n");
		wm_print(window_id, "!<number>  Reexecutes command (see history)\n");
	} else if (k_memcmp(buff, "clear", sizeof("clear")) == 0) {
		wm_clear(window_id);
	} else if (k_memcmp(buff, "pong", sizeof("pong")) == 0) {
		start_pong();
    } else if (k_memcmp(buff, "train", sizeof("train")) == 0) {
        init_train();
	} else if (k_memcmp(buff, "shell", sizeof("shell")) == 0) {
		start_shell();
	} else if (k_memcmp(buff, "echo ", sizeof("echo")) == 0) {
		wm_print(window_id, "%s\n", buff + sizeof("echo"));
	} else if (k_memcmp(buff, "ps", sizeof("ps")) == 0) {
		print_processes(window_id);
	} else if (k_memcmp(buff, "history", sizeof("history")) == 0) {
		for (int idx = 0; idx < HISTORY_SIZE; ++idx) {
			if (history[idx]) {
				wm_print(window_id, "%.2d.  %s\n", idx, history[idx]);
			} else {
				break;
			}
		}
	} else if (buff[0] == '!') {
		int value = 0;
		const char* n = buff + 1;

		while (*n) {
			if (*n >= '0' && *n <= '9') {
				value = (value * 10) + (*n - '0');
				n++;
			} else {
				wm_print(window_id, "malformed integer.\n");
				return 1;
			}
		}

		if (value >= 0 && value < HISTORY_SIZE && history[value])
			return handle_command(window_id, history[value], history);
		
		wm_print(window_id, "bad history index.\n");
		return 0;
	} else {
		return 0;
	}

	return 1;
}

// credit: Arno Puder (from process.c), altered to accept a window id.
static void print_process_heading(int wnd)
{
    wm_print(wnd, "State           Active Prio Name\n");
    wm_print(wnd, "------------------------------------------------\n");
}

// credit: Arno Puder (from process.c), altered to accept a window id.
static void print_process_details(int wnd, PROCESS p)
{
    static const char *state[] = { "READY          ",
        "ZOMBIE         ",
        "SEND_BLOCKED   ",
        "REPLY_BLOCKED  ",
        "RECEIVE_BLOCKED",
        "MESSAGE_BLOCKED",
        "INTR_BLOCKED   "
    };
    if (!p->used) {
        wm_print(wnd, "PCB slot unused!\n");
        return;
    }
    /* State */
    wm_print(wnd, state[p->state]);
    /* Check for active_proc */
    if (p == active_proc)
        wm_print(wnd, " *      ");
    else
        wm_print(wnd, "        ");
    /* Priority */
    wm_print(wnd, "  %2d", p->priority);
    /* Name */
    wm_print(wnd, " %s\n", p->name);
}

// credit: Arno Puder (from process.c), altered to accept a window id.
void print_processes(int wnd)
{
    int             i;
    PCB            *p = pcb;

    print_process_heading(wnd);
    for (i = 0; i < MAX_PROCS; i++, p++) {
        if (!p->used)
            continue;
        print_process_details(wnd, p);
    }
}
