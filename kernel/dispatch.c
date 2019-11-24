#include <kernel.h>
#include <lock.h>

PROCESS active_proc;
PCB* ready_queue[MAX_READY_QUEUES];

void add_ready_queue(PROCESS proc)
{
    CPU_LOCK();

    assert(proc->used == TRUE);
    assert(proc->magic == MAGIC_PCB);
    assert(proc->priority >= 1 && proc->priority < MAX_READY_QUEUES);

    if (ready_queue[proc->priority]) {
        PROCESS first = ready_queue[proc->priority];
        PROCESS last = first->prev;

        first->prev = proc;
        last->next = proc;
        proc->prev = last;
        proc->next = first;
    } else {
        ready_queue[proc->priority] = proc;
        proc->prev = proc;
        proc->next = proc;
    }

    proc->state = STATE_READY;

    CPU_UNLOCK();
}

void remove_ready_queue(PROCESS proc)
{
    CPU_LOCK();

    assert(proc->used == TRUE);
    assert(proc->magic == MAGIC_PCB);
    assert(proc->priority >= 1 && proc->priority < MAX_READY_QUEUES);
    
    if (proc->next != proc) {
        proc->prev->next = proc->next;
        proc->next->prev = proc->prev;
        ready_queue[proc->priority] = proc->next;
    } else {
        ready_queue[proc->priority] = NULL;
    }
    
    CPU_UNLOCK();
}

void become_zombie()
{
    assert(active_proc->used == TRUE);
    assert(active_proc->magic == MAGIC_PCB);
    assert(active_proc->priority >= 1 && active_proc->priority < MAX_READY_QUEUES);
    active_proc->state = STATE_ZOMBIE;
    remove_ready_queue(active_proc);
    resign();
    assert(0);
}

PROCESS dispatcher()
{
    PROCESS proc;
    int prio;

    CPU_LOCK();

    for (prio = MAX_READY_QUEUES - 1; prio >= 1; --prio)
        if ((proc = ready_queue[prio]))
            break;

    assert(prio >= 1);

    if (prio == active_proc->priority)
        proc = active_proc->next;

    CPU_UNLOCK();

    return proc;
}

void resign()
{
    asm volatile("\
        pushfl;\
        cli;\
        pop %%eax;\
        xchg (%%esp), %%eax;\
        pushl %%cs;\
        pushl %%eax;\
        pushl %%eax;\
        pushl %%ecx;\
        pushl %%edx;\
        pushl %%ebx;\
        pushl %%ebp;\
        pushl %%esi;\
        pushl %%edi;\
        movl %%esp,%0;": "=r"(active_proc->esp):);

    active_proc = dispatcher();

    // restore context of new proc
    asm volatile("\
        movl %0, %%esp;\
        popl %%edi;\
        popl %%esi;\
        popl %%ebp;\
        popl %%ebx;\
        popl %%edx;\
        popl %%ecx;\
        popl %%eax;\
        iret;": :"r"(active_proc->esp));
}

void init_dispatcher()
{
    for (int idx = 0; idx < MAX_READY_QUEUES; idx++)
        ready_queue[idx] = NULL;

    add_ready_queue(active_proc);
}
