/** train.c
 * Train Application
 * Matthew Ibrahim (916771224)
 */

#include <kernel.h>

/// Number of ticks to sleep for after each command.
#define COOLDOWN 15

/// Number of switches.
#define SWITCHES 9

/// Number of contacts.
#define CONTACTS 16

/// Number of attempts to try and locate the Zomboni.
#define ATTEMPTS 15

/// Possible Zomboni states.
typedef enum zomboni_state_t
{
    ZOMBONI_STATE_NONE,
    ZOMBONI_STATE_VALID
} zomboni_state_t;

/// Possible switch states.
typedef enum switch_state_t
{
    SWITCH_STATE_RED = 'R',
    SWITCH_STATE_GREEN = 'G'
} switch_state_t;

/// Possible probe states.
typedef enum probe_state_t
{
    PROBE_STATE_EMPTY,
    PROBE_STATE_OCCUPIED
} probe_state_t;

/// Configuration state.
typedef struct configuration_t
{
    int train_id;
    int wagon_id;
    void (*handler)(int windowID, zomboni_state_t zomboni);
} configuration_t;

static void train_handler_cfg1(int windowID, zomboni_state_t zomboni);
static void train_handler_cfg2(int windowID, zomboni_state_t zomboni);
static void train_handler_cfg3(int windowID, zomboni_state_t zomboni);
static void train_handler_cfg4(int windowID, zomboni_state_t zomboni);

/// All known configurations.
static configuration_t configurations[] = {
    (configuration_t){ 5, 12, train_handler_cfg1 },
    (configuration_t){ 11, 2, train_handler_cfg2 },
    (configuration_t){ 16, 2, train_handler_cfg3 },
    (configuration_t){ 16, 8, train_handler_cfg4 }
};

/**
 * Sends the command to the COM port and sleeps for COOLDOWN ticks.
 */
static void train_send_command(COM_Message message);

/**
 * Changes the trains direction. The train must be stopped.
 */
static void train_change_direction();

/**
 * Changes the trains speed.
 * 
 * @param speed  The new speed, between 0 and 5 (inclusive).
 * @throw Errors if speed is out of range.
 */
static void train_change_speed(int speed);

/**
 * Changes a switch to the given state.
 * 
 * @param id  The switch's ID, between 1 and SWITCHES (inclusive).
 * @param state  The new state.
 * @throw Errors if the switch's ID is out of range.
 */
static void train_change_switch(int id, switch_state_t state);

/**
 * Probes for the status of the given contact.
 * 
 * @param id  The contact's ID, between 1 and CONTACTS (inclusive).
 * @return PROBE_STATE_OCCUPIED if a train is on the contact,
 *         PROBE_STATE_EMPTY otherwise.
 * @throw Errors if the contact's ID is out of range.
 */
static probe_state_t train_probe_contact(int id);

/**
 * Initializes the track by changing all applicable switches to ensure the
 *  Zomboni is on a closed loop.
 */
static void train_initialize_track();

/**
 * Attempts to locate the Zomboni by probing contact 3 up to ATTEMPTS times.
 * 
 * @return ZOMBONI_STATE_VALID if the Zomboni was found,
 *         ZOMBONI_STATE_NONE otherwise.
 */
static zomboni_state_t train_find_zomboni();

/**
 * Continues to probe the given contact until its state is met.
 * 
 * @param id  The contact's ID, between 1 and CONTACTS (inclusive).
 * @param state  The expected state of the contact.
 * @throw Errors if the contact's ID is out of range.
 */
static void train_wait_for_contact(int id, probe_state_t state);

void train_process(PROCESS self, PARAM param)
{
    int window = wm_create(5, 5, 60, 10);
    zomboni_state_t zomboni;
    configuration_t* config = NULL;

    wm_print(window, "Initializing Track...");
    train_initialize_track();
    wm_print(window, " Done.\n");

    wm_print(window, "Locating Zomboni...");
    zomboni = train_find_zomboni();
    wm_print(window, " Done. (%s)\n", zomboni == ZOMBONI_STATE_NONE ? "none" : "found");

    wm_print(window, "Detecting Configuration...");

    for (int idx = 0; idx < 4 && config == NULL; ++idx) {
        configuration_t* current = &configurations[idx]; 

        if (train_probe_contact(current->train_id) == PROBE_STATE_OCCUPIED &&
            train_probe_contact(current->wagon_id) == PROBE_STATE_OCCUPIED)
            config = current;
    }

    assert(config != NULL);

    wm_print(window, " Done. (cfg %d)\n", 1 + ((config - configurations) % 4));
    config->handler(window, zomboni);
    wm_print(window, "Complete!\n");

    become_zombie();
}

void init_train()
{
    create_process(train_process, 1, 0, "Train Process");
}

void train_send_command(COM_Message message)
{
    send(com_port, &message);
    sleep(COOLDOWN);
}

void train_change_direction()
{
    train_send_command((COM_Message){
        .input_buffer = NULL, .len_input_buffer = 0,
        .output_buffer = "L20D\r"
    });
}

void train_change_speed(int speed)
{
    char command[] = "L20S?\r";
    
    assert(speed >= 0 && speed <= 5);
    command[4] = '0' + speed;

    train_send_command((COM_Message){
        .input_buffer = NULL, .len_input_buffer = 0,
        .output_buffer = command
    });
}

void train_change_switch(int id, switch_state_t state)
{
    char command[] = "M??\r";

    assert(id >= 1 && id <= SWITCHES);
    command[1] = '0' + id;
    command[2] = state;

    train_send_command((COM_Message){
        .input_buffer = NULL, .len_input_buffer = 0,
        .output_buffer = command
    });
}

probe_state_t train_probe_contact(int id)
{
    char data[] = "C??\r";
    char result[3];

    assert(id >= 1 && id <= CONTACTS);
    data[1] = '0' + (id / 10);
    data[2] = '0' + (id % 10);

    // clear buffer
    train_send_command((COM_Message){
        .input_buffer = NULL, .len_input_buffer = 0,
        .output_buffer = "R\r"
    });

    // get result
    train_send_command((COM_Message){
        .input_buffer = result, .len_input_buffer = sizeof(result),
        .output_buffer = data
    });

    return result[1] == '1';
}

void train_initialize_track()
{
    train_change_switch(1, SWITCH_STATE_GREEN);
    train_change_switch(4, SWITCH_STATE_GREEN);
    train_change_switch(5, SWITCH_STATE_GREEN);
    train_change_switch(8, SWITCH_STATE_GREEN);
    train_change_switch(9, SWITCH_STATE_RED);
}

zomboni_state_t train_find_zomboni()
{
    // Poll for Zomboni.
    for (int idx = 0; idx < ATTEMPTS; ++idx)
        if (train_probe_contact(3) == PROBE_STATE_OCCUPIED)
            return ZOMBONI_STATE_VALID;
    
    return ZOMBONI_STATE_NONE;
}

void train_wait_for_contact(int id, probe_state_t state)
{
    while (train_probe_contact(id) != state)
        ;
}

void train_handler_cfg1(int windowID, zomboni_state_t zomboni)
{
    if (zomboni == ZOMBONI_STATE_VALID) {
        wm_print(windowID, "> waiting for zomboni...\n");
        train_wait_for_contact(10, PROBE_STATE_OCCUPIED);
    }

    train_change_speed(5);
    train_change_switch(2, SWITCH_STATE_RED);
    train_change_switch(5, SWITCH_STATE_RED);
    train_change_switch(6, SWITCH_STATE_GREEN);
    train_change_switch(7, SWITCH_STATE_RED);
    train_wait_for_contact(7, PROBE_STATE_OCCUPIED);
    train_wait_for_contact(7, PROBE_STATE_EMPTY);
    sleep(5);
    train_change_switch(5, SWITCH_STATE_GREEN);
    train_wait_for_contact(12, PROBE_STATE_OCCUPIED);
    sleep(15);
    train_change_speed(0);
    train_change_direction();

    if (zomboni == ZOMBONI_STATE_VALID) {
        wm_print(windowID, "> waiting for zamboni...\n");
        train_wait_for_contact(10, PROBE_STATE_OCCUPIED);
    }

    train_change_speed(5);

    if (zomboni == ZOMBONI_STATE_VALID) {
        wm_print(windowID, "> waiting for zamboni...\n");
        train_wait_for_contact(7, PROBE_STATE_OCCUPIED);
    }

    train_wait_for_contact(6, PROBE_STATE_OCCUPIED);
    train_change_speed(0);
    train_change_switch(3, SWITCH_STATE_RED);
    train_change_switch(4, SWITCH_STATE_RED);
    train_change_direction();
    train_change_speed(5);
    sleep(100);
    train_change_speed(0);
}


void train_handler_cfg2(int windowID, zomboni_state_t zomboni)
{
    if (zomboni == ZOMBONI_STATE_VALID) {
        wm_print(windowID, "> waiting for zamboni...\n");
        train_wait_for_contact(14, PROBE_STATE_OCCUPIED);
    }

    train_change_speed(5);

    if (zomboni == ZOMBONI_STATE_VALID) {
        wm_print(windowID, "> waiting for zamboni...\n");
        train_wait_for_contact(6, PROBE_STATE_OCCUPIED);
    }

    train_wait_for_contact(7, PROBE_STATE_OCCUPIED);
    train_change_switch(3, SWITCH_STATE_GREEN);
    train_change_switch(4, SWITCH_STATE_RED);
    train_wait_for_contact(1, PROBE_STATE_OCCUPIED);
    train_change_speed(0);
    train_change_switch(4, SWITCH_STATE_GREEN);


    if (zomboni == ZOMBONI_STATE_VALID) {
        wm_print(windowID, "> waiting for zamboni...\n");
        train_wait_for_contact(14, PROBE_STATE_OCCUPIED);
    }

    train_change_speed(5);
    train_wait_for_contact(10, PROBE_STATE_OCCUPIED);
    train_change_switch(8, SWITCH_STATE_RED);
    train_wait_for_contact(11, PROBE_STATE_OCCUPIED);
    train_change_speed(0);
    train_change_switch(8, SWITCH_STATE_GREEN);
}

void train_handler_cfg3(int windowID, zomboni_state_t zomboni)
{
}

void train_handler_cfg4(int windowID, zomboni_state_t zomboni)
{
}
