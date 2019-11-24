
#include <kernel.h>
#include <lock.h>

PORT_DEF ports[MAX_PORTS];
static PORT next_free;

PORT create_port()
{
    return create_new_port(active_proc);
}

PORT create_new_port(PROCESS owner)
{
    PORT port;
    CPU_LOCK();
    port = next_free;

    assert(owner->magic == MAGIC_PCB);
    assert(port);

    next_free = port->next;
    port->magic = MAGIC_PORT;
    port->used = TRUE;
    port->open = TRUE;
    port->owner = owner;
    port->blocked_list_head = NULL;
    port->blocked_list_tail = NULL;

    if (owner->first_port)
        port->next = owner->first_port;
    else
        port->next = NULL;
    
    owner->first_port = port;

    CPU_UNLOCK();
    return port;
}

void open_port(PORT port)
{
    assert(port->magic == MAGIC_PORT);
    port->open = TRUE;
}

void close_port(PORT port)
{
    assert(port->magic == MAGIC_PORT);
    port->open = FALSE;
}

static void send_block(PORT port, PROCESS invoker)
{
    assert(port->magic == MAGIC_PORT);
    assert(invoker->magic == MAGIC_PCB);

    if (port->blocked_list_head)
        port->blocked_list_tail->next_blocked = invoker;
    else
        port->blocked_list_head = invoker;
    
    port->blocked_list_tail = invoker;
    invoker->next_blocked = NULL;
}

void send(PORT dest_port, void *data) 
{
    PROCESS owner;
    CPU_LOCK();
    owner = dest_port->owner;

    assert(dest_port->magic == MAGIC_PORT);
    assert(owner->magic == MAGIC_PCB);
    assert(active_proc->magic == MAGIC_PCB);
    assert(active_proc->state == STATE_READY);

    if (dest_port->open && owner->state == STATE_RECEIVE_BLOCKED) {
        // port is open and owner is waiting via receive()
        owner->param_proc = active_proc;
        owner->param_data = data;
        active_proc->state = STATE_REPLY_BLOCKED;
        add_ready_queue(owner);
    } else {
        // port is closed or owner is not waiting via receive()
        active_proc->state = STATE_SEND_BLOCKED;
        active_proc->param_data = data;
        send_block(dest_port, active_proc);
    }

    active_proc->param_data = data;
    remove_ready_queue(active_proc);
    resign();
    CPU_UNLOCK();
}

void message(PORT dest_port, void *data)
{
    PROCESS owner;
    CPU_LOCK();
    owner = dest_port->owner;

    assert(dest_port->magic == MAGIC_PORT);
    assert(owner->magic == MAGIC_PCB);
    assert(active_proc->magic == MAGIC_PCB);
    assert(active_proc->state == STATE_READY);

    if (dest_port->open && owner->state == STATE_RECEIVE_BLOCKED) {
        // port is open and owner is waiting via receive()
        owner->param_proc = active_proc;
        owner->param_data = data;
        add_ready_queue(owner);
    } else {
        // port is closed or owner is not waiting via receive
        active_proc->state = STATE_MESSAGE_BLOCKED;
        active_proc->param_data = data;
        remove_ready_queue(active_proc);
        send_block(dest_port, active_proc);
    }

    resign();
    CPU_UNLOCK();
}

void* receive(PROCESS * sender)
{
    PORT port;
    void* data;
    CPU_LOCK();
    port = active_proc->first_port;

    assert(active_proc->magic == MAGIC_PCB);
    assert(active_proc->state == STATE_READY);
    assert(port);

    // get first open port with a message
    for (; port; port = port->next) {
        assert(port->magic == MAGIC_PORT);

        if (port->open && port->blocked_list_head)
            break;
    }

    if (port) { // message on port
        PROCESS invoker = port->blocked_list_head;
        assert(invoker->magic == MAGIC_PCB);

        *sender = invoker;
        port->blocked_list_head = invoker->next_blocked;

        if (!port->blocked_list_head)
            port->blocked_list_tail = NULL;
        
        if (invoker->state == STATE_SEND_BLOCKED) {
            invoker->state = STATE_REPLY_BLOCKED;
            data = invoker->param_data;
            CPU_UNLOCK();
            return data;
        } else if (invoker->state == STATE_MESSAGE_BLOCKED) {
            add_ready_queue(invoker);
            data = invoker->param_data;
            CPU_UNLOCK();
            return data;
        } else {
            assert(0);
        }
    } else { // no messages, block
        remove_ready_queue(active_proc);
        active_proc->param_data = NULL;
        active_proc->state = STATE_RECEIVE_BLOCKED;
        resign(); // resumed on message/send
        *sender = active_proc->param_proc;
        data = active_proc->param_data;
        CPU_UNLOCK();
        return data;
    }
}

void reply(PROCESS sender)
{
    CPU_LOCK();
    assert(sender->magic == MAGIC_PCB);
    assert(sender->state == STATE_REPLY_BLOCKED);
    add_ready_queue(sender);
    resign();
    CPU_UNLOCK();
}

void init_ipc()
{
    for (int idx = 0; idx < MAX_PORTS; idx++) {
        ports[idx].magic = 0;
        ports[idx].used = FALSE;
        ports[idx].open = FALSE;
        ports[idx].next = &ports[idx + 1];
    }

    ports[MAX_PORTS - 1].next = NULL;
    next_free = &ports[0];
}
