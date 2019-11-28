
#include <kernel.h>


PCB             pcb[MAX_PROCS];
static PROCESS next_free;


PORT create_process(void (*ptr_to_new_proc) (PROCESS, PARAM),
                    int prio, PARAM param, char *name)
{
    PROCESS proc;
    volatile int saved_if;
    DISABLE_INTR(saved_if);
    proc = next_free;

    assert(prio >= 0 && prio < MAX_READY_QUEUES);
    assert(proc);

    next_free = proc->next;
    ENABLE_INTR(saved_if);

    proc->magic = MAGIC_PCB;
    proc->esp = (640*1024) - (16*1024)*(proc - &pcb[0]);
    proc->used = TRUE;  
    proc->state = STATE_READY;
    proc->priority = prio;
    proc->name = name;
    proc->first_port = create_new_port(proc);

    poke_l(proc->esp -= 4, (LONG)param);
    poke_l(proc->esp -= 4, (LONG)proc);
    poke_l(proc->esp -= 4, 0);
    poke_l(proc->esp -= 4, interrupts_initialized == TRUE ? 512 : 0); // EFLAGS
    poke_l(proc->esp -= 4, CODE_SELECTOR);
    poke_l(proc->esp -= 4, (LONG)ptr_to_new_proc);
    poke_l(proc->esp -= 4, 0);
    poke_l(proc->esp -= 4, 0);
    poke_l(proc->esp -= 4, 0);
    poke_l(proc->esp -= 4, 0);
    poke_l(proc->esp -= 4, 0);
    poke_l(proc->esp -= 4, 0);
    poke_l(proc->esp -= 4, 0);
    add_ready_queue(proc);

    return proc->first_port;
}


PROCESS fork()
{
    // Dummy return to make gcc happy
    return (PROCESS) NULL;
}




void print_process(WINDOW * wnd, PROCESS p)
{
    static const char* states[] = {
        "READY           ",
        "ZOMBIE          ",
        "SEND_BLOCKED    ",
        "REPLY_BLOCKED   ",
        "RECEIVE_BLOCKED ",
        "MESSAGE_BLOCKED ",
        "INTR_BLOCKED    "
    };

    assert(p->used == TRUE);
    assert(p->magic == MAGIC_PCB);
    assert(p->priority >= 0 && p->priority < MAX_READY_QUEUES);
    assert(p->state >= 0 && p->state < sizeof(states)/sizeof(const char*));
    output_string(wnd, states[p->state]);
    output_string(wnd, p == active_proc ?
        "*      " :
        "       ");
    output_string(wnd, "   ");
    output_char(wnd, p->priority + '0');
    output_char(wnd, ' ');
    output_string(wnd, p->name);
    output_char(wnd, '\n');
}

void print_all_processes(WINDOW * wnd)
{
    output_string(wnd, "State           Active Prio Name\n");
    output_string(wnd, "------------------------------------------------\n");

    for (int idx = 0; idx < MAX_PROCS; ++idx)
        if (pcb[idx].used == TRUE)
            print_process(wnd, &pcb[idx]);
}



void init_process()
{
    for (int idx = 0; idx < MAX_PROCS; idx++) {
        pcb[idx].magic = 0;
        pcb[idx].used = FALSE;
        pcb[idx].first_port = NULL;
        pcb[idx].next = &pcb[idx + 1];
    }
    
    active_proc = &pcb[0];
    pcb[MAX_PROCS - 1].next = NULL;

    active_proc->magic = MAGIC_PCB;
    active_proc->used = TRUE;
    active_proc->state = STATE_READY;
    active_proc->priority = 1;
    active_proc->first_port = NULL;
    active_proc->name = "Boot process";
    next_free = &pcb[1];
}
