#include "state_machine.h"
#include <stdio.h> // For printf

// --- Event Definitions ---
enum
{
    EV_POWER_ON,
    EV_START_TASK,
    EV_TASK_COMPLETE,
    EV_POWER_OFF
};

// --- Forward Declarations of States ---
static const SM_State STATE_Off;
static const SM_State STATE_On;
static const SM_State STATE_Idle;
static const SM_State STATE_Running;

// --- State Machine User Data (Optional) ---
typedef struct
{
    int tasks_completed;
} AppData;

// --- Action & Guard Functions ---
void entry_On(SM_StateMachine *sm, const SM_Event *event) {
    (void)sm;
    (void)event;
    printf("  (Entry)-> On\n");
}
void exit_On(SM_StateMachine *sm, const SM_Event *event) {
    (void)sm;
    (void)event;
    printf("  (Exit) -> On\n");
}
void entry_Idle(SM_StateMachine *sm, const SM_Event *event) {
    (void)sm;
    (void)event;
    printf("    (Entry)-> Idle\n");
}
void entry_Running(SM_StateMachine *sm, const SM_Event *event) {
    (void)sm;
    (void)event;
    printf("    (Entry)-> Running\n");
}
void exit_Running(SM_StateMachine *sm, const SM_Event *event) {
    (void)sm;
    (void)event;
    printf("    (Exit) -> Running\n");
}
void on_power_off(SM_StateMachine *sm, const SM_Event *event) {
    (void)sm;
    (void)event;
    printf("  Action: Shutting down...\n");
}
void on_task_done(SM_StateMachine *sm, const SM_Event *event)
{
    AppData *data = (AppData *)sm->userData;
    data->tasks_completed++;
    (void)event;
    printf("  Action: Task finished. Total completed: %d\n", data->tasks_completed);
}
bool can_start_task(SM_StateMachine *sm, const SM_Event *event)
{
    AppData *data = (AppData *)sm->userData;
    (void)event;
    printf("  Guard: Checking if tasks completed < 3... (%s)\n", (data->tasks_completed < 3) ? "Yes" : "No");
    return data->tasks_completed < 3;
}

// --- Transition Tables ---
const SM_Transition T_Off[] = {
    {EV_POWER_ON, &STATE_Idle, NULL, NULL, SM_TRANSITION_EXTERNAL} // Power On -> Go directly to Idle
};
const SM_Transition T_On[] = { // Parent state can handle common events
    {EV_POWER_OFF, &STATE_Off, NULL, on_power_off, SM_TRANSITION_EXTERNAL}};
const SM_Transition T_Idle[] = {
    {EV_START_TASK, &STATE_Running, can_start_task, NULL, SM_TRANSITION_EXTERNAL}};
const SM_Transition T_Running[] = {
    {EV_TASK_COMPLETE, &STATE_Idle, NULL, on_task_done, SM_TRANSITION_EXTERNAL}};

// --- State Definitions ---
static const SM_State STATE_Off = {
    .parent = NULL, // Top-level state
    .entryAction = NULL,
    .exitAction = NULL,
    .transitions = T_Off,
    .numTransitions = sizeof(T_Off) / sizeof(T_Off[0]),
    .name = "Off"};
static const SM_State STATE_On = {
    .parent = NULL, // Top-level state
    .entryAction = entry_On,
    .exitAction = exit_On,
    .transitions = T_On,
    .numTransitions = sizeof(T_On) / sizeof(T_On[0]),
    .name = "On"};
static const SM_State STATE_Idle = {
    .parent = &STATE_On, // Child of 'On'
    .entryAction = entry_Idle,
    .exitAction = NULL,
    .transitions = T_Idle,
    .numTransitions = sizeof(T_Idle) / sizeof(T_Idle[0]),
    .name = "Idle"};
static const SM_State STATE_Running = {
    .parent = &STATE_On, // Child of 'On'
    .entryAction = entry_Running,
    .exitAction = exit_Running,
    .transitions = T_Running,
    .numTransitions = sizeof(T_Running) / sizeof(T_Running[0]),
    .name = "Running"};

// --- Main Application Logic ---
void run_sm_test(SM_StateMachine *sm, const char *event_name, uint32_t event_id)
{
    printf("\n--- Dispatching Event: %s ---\n", event_name);
    SM_Event event = {event_id, NULL};
    if (!SM_Dispatch(sm, &event))
    {
        printf("Event %s was not handled.\n", event_name);
    }
    printf("Current State: %s\n", SM_GetCurrentStateName(sm));
}

// NEW: Hook for unhandled events
void on_unhandled_event(SM_StateMachine *sm, const SM_Event *event)
{
    printf("--- Unhandled Event Hook: Event %u received in state '%s' ---\n",
           (unsigned)event->id, SM_GetCurrentStateName(sm));
}

void setup_and_run_sm(void)
{
    SM_StateMachine sm;
    AppData app_data = {0};

#define MAX_STATE_DEPTH 8
    const SM_State *path_buffer[MAX_STATE_DEPTH];

    // NEW: Updated SM_Init call
    SM_Init(&sm, &STATE_Off, path_buffer, MAX_STATE_DEPTH, &app_data, on_unhandled_event);

    printf("Initial state: %s\n", SM_GetCurrentStateName(&sm));
    printf("Is in state 'On'? %s\n", SM_IsInState(&sm, &STATE_On) ? "Yes" : "No");

    // --- Dispatch events ---
    SM_Event ev_power_on = {EV_POWER_ON, NULL};
    SM_Dispatch(&sm, &ev_power_on);
    printf("Current state: %s\n", SM_GetCurrentStateName(&sm));
    printf("Is in state 'On'? %s\n", SM_IsInState(&sm, &STATE_On) ? "Yes" : "No");
    printf("Is in state 'Idle'? %s\n", SM_IsInState(&sm, &STATE_Idle) ? "Yes" : "No");

    // --- Dispatch an unhandled event to test the hook ---
    SM_Event ev_unhandled = {99, NULL};
    SM_Dispatch(&sm, &ev_unhandled);

    SM_Reset(&sm);
    printf("After reset, current state: %s\n", SM_GetCurrentStateName(&sm));

    SM_Deinit(&sm);
    printf("After deinit, current state name: %s\n", SM_GetCurrentStateName(&sm));
}

int main(void)
{
    setup_and_run_sm();
    return 0;
}

/*
$ ./test_state_machine                                                                                                                                                                                                                ✭master
Initial state: Off
Is in state 'On'? No
  (Entry)-> On
    (Entry)-> Idle
Current state: Idle
Is in state 'On'? Yes
Is in state 'Idle'? Yes
--- Unhandled Event Hook: Event 99 received in state 'Idle' ---
  (Exit) -> On
After reset, current state: Off
After deinit, current state name: Unknown
*/
