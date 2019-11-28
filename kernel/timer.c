#include <kernel.h>

PORT timer_port;

void timer_notifier(PROCESS self, PARAM param)
{
    while (1) {
        wait_for_interrupt(TIMER_IRQ);
        message(timer_port, NULL);
    }
}

void timer_process(PROCESS self, PARAM param)
{
    int counters[MAX_PROCS] = { 0 };
    Timer_Message* msg;
    PROCESS sender;

    create_process(timer_notifier, 7, 0, "Timer notifier");

    while (1) {
        msg = (Timer_Message*)receive(&sender);

        if (msg == NULL) { //invoked by timer interrupt (see timer_notifier)
            for (int i = 0; i < MAX_PROCS; ++i) {
                if (counters[i] == 0) {
                    continue;
                } else if (--counters[i] == 0) {
                    reply(&pcb[i]);
                }
            }
        } else { // from client
            counters[sender - &pcb[0]] = msg->num_of_ticks;
        }
    }
}

void sleep(int ticks)
{
    Timer_Message msg;
    msg.num_of_ticks = ticks;
    send(timer_port, &msg);
}


void init_timer()
{
    timer_port = create_process(timer_process, 6, 0, "Timer process");
    resign();
}
