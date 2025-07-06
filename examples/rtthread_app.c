#include <rtthread.h>
#include "state_machine.h"
#include <ulog.h>
#include <stdlib.h>
#include <string.h>

/* State machine thread and queue configuration macros */
#define SM_THREAD_PRIORITY   15
#define SM_THREAD_STACK_SIZE 1024
#define SM_THREAD_TIMESLICE  10
#define SM_MQ_MAX_MSGS 10
#define SM_MAX_STATE_DEPTH 8

/* ===================== 1. Event Definitions ===================== */
/*
 * Enumeration of all events for the system state machine.
 * Covers POST, running, maintenance, upgrade, etc.
 */
typedef enum
{
    SM_EVENT_POWER_ON = 1,      /* Power on event, trigger POST */
    SM_EVENT_POST_STEP_OK,      /* POST step succeeded */
    SM_EVENT_POST_STEP_FAIL,    /* POST step failed */
    SM_EVENT_POST_RETRY,        /* Retry POST step */
    SM_EVENT_POST_DONE,         /* POST finished */
    SM_EVENT_ENTER_RUN,         /* Enter running state */
    SM_EVENT_RUN_ERROR,         /* Running error occurred */
    SM_EVENT_ENTER_MAINT,       /* Enter maintenance mode */
    SM_EVENT_EXIT_MAINT,        /* Exit maintenance mode */
    SM_EVENT_ENTER_UPGRADE,     /* Enter upgrade mode */
    SM_EVENT_UPGRADE_DONE,      /* Upgrade finished */
    SM_EVENT_RESET,             /* System reset */
    SM_EVENT_SHUTDOWN,          /* System shutdown */
    SM_EVENT_FORCE_RECOVER      /* Force recovery from error/fail */
} sm_event_t;

/* ===================== 2. State Forward Declarations ===================== */
/*
 * Forward declarations for all states in the system state machine.
 * Each state is defined as a static const SM_State below.
 */
static const SM_State sm_state_off;
static const SM_State sm_state_power_on;
static const SM_State sm_state_post;
static const SM_State sm_state_post_step;
static const SM_State sm_state_post_retry;
static const SM_State sm_state_post_fail;
static const SM_State sm_state_post_pass;
static const SM_State sm_state_run;
static const SM_State sm_state_run_error;
static const SM_State sm_state_maint;
static const SM_State sm_state_upgrade;
static const SM_State sm_state_upgrade_done;

/* ===================== 3. User Data Structure ===================== */
/*
 * User data for the system state machine.
 * Used for POST and running statistics.
 */
typedef struct
{
    int32_t post_step;         /* Current POST step */
    int32_t post_fail_count;   /* Number of POST failures */
    int32_t run_error_count;   /* Number of running errors */
    int32_t upgrade_flag;      /* Upgrade in progress flag */
} sm_system_data_t;

/* ===================== 4. Actions and Guard Functions ===================== */
/*
 * Generic entry action for states, prints entering state.
 */
static void sm_entry_print(SM_StateMachine *sm, const SM_Event *event)
{
    (void)event;
    log_i("==> Enter %s", SM_GetCurrentStateName(sm));
    /* Automatically transition from PowerOn to POST */
    if (strcmp(SM_GetCurrentStateName(sm), "PowerOn") == 0) {
        SM_Event evt = {SM_EVENT_POST_STEP_OK, NULL};
        SM_Dispatch(sm, &evt);
    }
}
/*
 * Generic exit action for states, prints exiting state.
 */
static void sm_exit_print(SM_StateMachine *sm, const SM_Event *event)
{
    (void)event;
    log_i("<== Exit %s", SM_GetCurrentStateName(sm));
}
/*
 * Entry action for POST state, initializes POST step and fail count.
 */
static void sm_entry_post(SM_StateMachine *sm, const SM_Event *event)
{
    (void)event;
    sm_system_data_t *data = (sm_system_data_t*)sm->userData;
    data->post_step = 0;
    data->post_fail_count = 0;
    log_i("POST: Start self-check sequence.");
}
/*
 * Entry action for POST step state, simulates POST step logic.
 * Odd steps succeed, even steps fail, after 3 steps POST is done.
 */
static void sm_entry_post_step(SM_StateMachine *sm, const SM_Event *event)
{
    (void)event;
    sm_system_data_t *data = (sm_system_data_t*)sm->userData;
    data->post_step++;
    log_i("POST: Step %d started.", data->post_step);
    /* Simulate: even steps fail, odd steps succeed */
    if ((data->post_step % 2) == 0) {
        log_w("POST: Step %d failed!", data->post_step);
        SM_Event fail_evt = {SM_EVENT_POST_STEP_FAIL, NULL};
        SM_Dispatch(sm, &fail_evt);
    } else if (data->post_step < 3) {
        log_i("POST: Step %d ok.", data->post_step);
        SM_Event ok_evt = {SM_EVENT_POST_STEP_OK, NULL};
        SM_Dispatch(sm, &ok_evt);
    } else {
        log_i("POST: All steps done.");
        SM_Event done_evt = {SM_EVENT_POST_DONE, NULL};
        SM_Dispatch(sm, &done_evt);
    }
}
/*
 * Entry action for POST retry state, handles retry logic.
 */
static void sm_entry_post_retry(SM_StateMachine *sm, const SM_Event *event)
{
    (void)event;
    sm_system_data_t *data = (sm_system_data_t*)sm->userData;
    data->post_fail_count++;
    log_w("POST: Retry %d", data->post_fail_count);
    if (data->post_fail_count < 2) {
        SM_Event retry_evt = {SM_EVENT_POST_RETRY, NULL};
        SM_Dispatch(sm, &retry_evt);
    } else {
        log_e("POST: Retry failed, enter FAIL.");
        SM_Event fail_evt = {SM_EVENT_POST_STEP_FAIL, NULL};
        SM_Dispatch(sm, &fail_evt);
    }
}
/*
 * Entry action for POST fail state, indicates POST failure.
 */
static void sm_entry_post_fail(SM_StateMachine *sm, const SM_Event *event)
{
    (void)sm;
    (void)event;
    /* Print POST failure */
    log_e("POST: Self-check failed! Wait for manual reset or force recover.");
}
/*
 * Entry action for POST pass state, indicates POST success and triggers running state.
 */
static void sm_entry_post_pass(SM_StateMachine *sm, const SM_Event *event)
{
    (void)event;
    log_i("POST: Self-check passed.");
    SM_Event enter_run = {SM_EVENT_ENTER_RUN, NULL};
    SM_Dispatch(sm, &enter_run);
}
/*
 * Entry action for running state, indicates system is running.
 */
static void sm_entry_run(SM_StateMachine *sm, const SM_Event *event)
{
    (void)sm;
    (void)event;
    log_i("System running normally.");
}
/*
 * Entry action for running error state, increments error count.
 */
static void sm_entry_run_error(SM_StateMachine *sm, const SM_Event *event)
{
    (void)event;
    sm_system_data_t *data = (sm_system_data_t*)sm->userData;
    data->run_error_count++;
    log_e("System running error! Error count: %d", data->run_error_count);
}
/*
 * Entry action for maintenance state.
 */
static void sm_entry_maint(SM_StateMachine *sm, const SM_Event *event)
{
    (void)sm;
    (void)event;
    log_i("Enter maintenance mode.");
}
/*
 * Entry action for upgrade state, sets upgrade flag.
 */
static void sm_entry_upgrade(SM_StateMachine *sm, const SM_Event *event)
{
    (void)event;
    sm_system_data_t *data = (sm_system_data_t*)sm->userData;
    data->upgrade_flag = 1;
    log_i("Enter upgrade mode.");
}
/*
 * Entry action for upgrade done state, clears upgrade flag and triggers reset.
 */
static void sm_entry_upgrade_done(SM_StateMachine *sm, const SM_Event *event)
{
    (void)event;
    sm_system_data_t *data = (sm_system_data_t*)sm->userData;
    data->upgrade_flag = 0;
    log_i("Upgrade finished, system will reset.");
    SM_Event reset_evt = {SM_EVENT_RESET, NULL};
    SM_Dispatch(sm, &reset_evt);
}
/*
 * Guard for POST retry, allows up to 2 retries.
 */
static bool sm_guard_post_retry(SM_StateMachine *sm, const SM_Event *event)
{
    (void)event;
    sm_system_data_t *data = (sm_system_data_t*)sm->userData;
    return data->post_fail_count < 2;
}
/*
 * Guard for running error, allows up to 3 recoveries.
 */
static bool sm_guard_run_error_limit(SM_StateMachine *sm, const SM_Event *event)
{
    (void)event;
    sm_system_data_t *data = (sm_system_data_t*)sm->userData;
    return data->run_error_count < 3;
}

/* ===================== 5. Transition Tables ===================== */
/*
 * Transition tables for each state, describing event-driven state changes.
 */
static const SM_Transition sm_trans_off[] = {
    {SM_EVENT_POWER_ON, &sm_state_power_on, NULL, NULL, SM_TRANSITION_EXTERNAL}
};
static const SM_Transition sm_trans_power_on[] = {
    {SM_EVENT_POST_STEP_OK, &sm_state_post_step, NULL, NULL, SM_TRANSITION_EXTERNAL},
    {SM_EVENT_POST_STEP_FAIL, &sm_state_post_fail, NULL, NULL, SM_TRANSITION_EXTERNAL},
    {SM_EVENT_POST_DONE, &sm_state_post_pass, NULL, NULL, SM_TRANSITION_EXTERNAL}
};
static const SM_Transition sm_trans_post[] = {
    {SM_EVENT_POST_STEP_OK, &sm_state_post_step, NULL, NULL, SM_TRANSITION_EXTERNAL},
    {SM_EVENT_POST_STEP_FAIL, &sm_state_post_retry, sm_guard_post_retry, NULL, SM_TRANSITION_EXTERNAL},
    {SM_EVENT_POST_STEP_FAIL, &sm_state_post_fail, NULL, NULL, SM_TRANSITION_EXTERNAL},
    {SM_EVENT_POST_DONE, &sm_state_post_pass, NULL, NULL, SM_TRANSITION_EXTERNAL}
};
static const SM_Transition sm_trans_post_step[] = {
    {SM_EVENT_POST_STEP_OK, &sm_state_post_step, NULL, NULL, SM_TRANSITION_EXTERNAL},
    {SM_EVENT_POST_STEP_FAIL, &sm_state_post_retry, sm_guard_post_retry, NULL, SM_TRANSITION_EXTERNAL},
    {SM_EVENT_POST_STEP_FAIL, &sm_state_post_fail, NULL, NULL, SM_TRANSITION_EXTERNAL},
    {SM_EVENT_POST_DONE, &sm_state_post_pass, NULL, NULL, SM_TRANSITION_EXTERNAL}
};
static const SM_Transition sm_trans_post_retry[] = {
    {SM_EVENT_POST_RETRY, &sm_state_post_step, NULL, NULL, SM_TRANSITION_EXTERNAL},
    {SM_EVENT_POST_STEP_FAIL, &sm_state_post_fail, NULL, NULL, SM_TRANSITION_EXTERNAL}
};
static const SM_Transition sm_trans_post_fail[] = {
    {SM_EVENT_RESET, &sm_state_off, NULL, NULL, SM_TRANSITION_EXTERNAL},
    {SM_EVENT_FORCE_RECOVER, &sm_state_post, NULL, NULL, SM_TRANSITION_EXTERNAL}
};
static const SM_Transition sm_trans_post_pass[] = {
    {SM_EVENT_ENTER_RUN, &sm_state_run, NULL, NULL, SM_TRANSITION_EXTERNAL}
};
static const SM_Transition sm_trans_run[] = {
    {SM_EVENT_RUN_ERROR, &sm_state_run_error, NULL, NULL, SM_TRANSITION_EXTERNAL},
    {SM_EVENT_ENTER_MAINT, &sm_state_maint, NULL, NULL, SM_TRANSITION_EXTERNAL},
    {SM_EVENT_ENTER_UPGRADE, &sm_state_upgrade, NULL, NULL, SM_TRANSITION_EXTERNAL},
    {SM_EVENT_SHUTDOWN, &sm_state_off, NULL, NULL, SM_TRANSITION_EXTERNAL}
};
static const SM_Transition sm_trans_run_error[] = {
    {SM_EVENT_FORCE_RECOVER, &sm_state_run, sm_guard_run_error_limit, NULL, SM_TRANSITION_EXTERNAL},
    {SM_EVENT_SHUTDOWN, &sm_state_off, NULL, NULL, SM_TRANSITION_EXTERNAL}
};
static const SM_Transition sm_trans_maint[] = {
    {SM_EVENT_EXIT_MAINT, &sm_state_run, NULL, NULL, SM_TRANSITION_EXTERNAL}
};
static const SM_Transition sm_trans_upgrade[] = {
    {SM_EVENT_UPGRADE_DONE, &sm_state_upgrade_done, NULL, NULL, SM_TRANSITION_EXTERNAL}
};
static const SM_Transition sm_trans_upgrade_done[] = {
    {SM_EVENT_RESET, &sm_state_off, NULL, NULL, SM_TRANSITION_EXTERNAL}
};

/* ===================== 6. State Definitions ===================== */
/*
 * State definitions, including parent, entry/exit actions, transitions, and state name.
 */
static const SM_State sm_state_off = {
    .parent = NULL,
    .entryAction = sm_entry_print,
    .exitAction = sm_exit_print,
    .transitions = sm_trans_off,
    .numTransitions = sizeof(sm_trans_off) / sizeof(sm_trans_off[0]),
    .name = "Off"
};
static const SM_State sm_state_power_on = {
    .parent = NULL,
    .entryAction = sm_entry_print,
    .exitAction = sm_exit_print,
    .transitions = sm_trans_power_on,
    .numTransitions = sizeof(sm_trans_power_on) / sizeof(sm_trans_power_on[0]),
    .name = "PowerOn"
};
static const SM_State sm_state_post = {
    .parent = &sm_state_power_on,
    .entryAction = sm_entry_post,
    .exitAction = sm_exit_print,
    .transitions = sm_trans_post,
    .numTransitions = sizeof(sm_trans_post) / sizeof(sm_trans_post[0]),
    .name = "Post"
};
static const SM_State sm_state_post_step = {
    .parent = &sm_state_post,
    .entryAction = sm_entry_post_step,
    .exitAction = sm_exit_print,
    .transitions = sm_trans_post_step,
    .numTransitions = sizeof(sm_trans_post_step) / sizeof(sm_trans_post_step[0]),
    .name = "PostStep"
};
static const SM_State sm_state_post_retry = {
    .parent = &sm_state_post,
    .entryAction = sm_entry_post_retry,
    .exitAction = sm_exit_print,
    .transitions = sm_trans_post_retry,
    .numTransitions = sizeof(sm_trans_post_retry) / sizeof(sm_trans_post_retry[0]),
    .name = "PostRetry"
};
static const SM_State sm_state_post_fail = {
    .parent = &sm_state_post,
    .entryAction = sm_entry_post_fail,
    .exitAction = sm_exit_print,
    .transitions = sm_trans_post_fail,
    .numTransitions = sizeof(sm_trans_post_fail) / sizeof(sm_trans_post_fail[0]),
    .name = "PostFail"
};
static const SM_State sm_state_post_pass = {
    .parent = &sm_state_post,
    .entryAction = sm_entry_post_pass,
    .exitAction = sm_exit_print,
    .transitions = sm_trans_post_pass,
    .numTransitions = sizeof(sm_trans_post_pass) / sizeof(sm_trans_post_pass[0]),
    .name = "PostPass"
};
static const SM_State sm_state_run = {
    .parent = NULL,
    .entryAction = sm_entry_run,
    .exitAction = sm_exit_print,
    .transitions = sm_trans_run,
    .numTransitions = sizeof(sm_trans_run) / sizeof(sm_trans_run[0]),
    .name = "Run"
};
static const SM_State sm_state_run_error = {
    .parent = &sm_state_run,
    .entryAction = sm_entry_run_error,
    .exitAction = sm_exit_print,
    .transitions = sm_trans_run_error,
    .numTransitions = sizeof(sm_trans_run_error) / sizeof(sm_trans_run_error[0]),
    .name = "RunError"
};
static const SM_State sm_state_maint = {
    .parent = NULL,
    .entryAction = sm_entry_maint,
    .exitAction = sm_exit_print,
    .transitions = sm_trans_maint,
    .numTransitions = sizeof(sm_trans_maint) / sizeof(sm_trans_maint[0]),
    .name = "Maint"
};
static const SM_State sm_state_upgrade = {
    .parent = NULL,
    .entryAction = sm_entry_upgrade,
    .exitAction = sm_exit_print,
    .transitions = sm_trans_upgrade,
    .numTransitions = sizeof(sm_trans_upgrade) / sizeof(sm_trans_upgrade[0]),
    .name = "Upgrade"
};
static const SM_State sm_state_upgrade_done = {
    .parent = &sm_state_upgrade,
    .entryAction = sm_entry_upgrade_done,
    .exitAction = sm_exit_print,
    .transitions = sm_trans_upgrade_done,
    .numTransitions = sizeof(sm_trans_upgrade_done) / sizeof(sm_trans_upgrade_done[0]),
    .name = "UpgradeDone"
};

/* ===================== 7. RT-Thread Resources ===================== */
/*
 * RT-Thread thread, message queue, state machine instance, user data, and entry path buffer.
 */
static rt_thread_t sm_thread;
static rt_mq_t sm_mq;
static SM_StateMachine sm;
static sm_system_data_t sm_sys_data;
static const SM_State *sm_entry_path_buffer[SM_MAX_STATE_DEPTH];

/* ===================== 8. Unhandled Event Hook ===================== */
/*
 * Hook function for unhandled events, prints warning log.
 */
static void sm_on_unhandled_event(SM_StateMachine *sm, const SM_Event *event)
{
    /* Print warning for unhandled event */
    log_w("--- Unhandled Event: Event %u received in state '%s' ---",
          (unsigned)event->id, SM_GetCurrentStateName(sm));
}

/* ===================== 9. State Machine Thread Entry ===================== */
/*
 * RT-Thread thread entry for state machine event loop.
 * Receives events from message queue and dispatches to state machine.
 */
static void sm_state_machine_thread_entry(void* parameter)
{
    /* Initialize the state machine */
    SM_Init(&sm, &sm_state_off, sm_entry_path_buffer, SM_MAX_STATE_DEPTH, &sm_sys_data, sm_on_unhandled_event);
    log_i("Complex State machine initialized. Initial State: %s", SM_GetCurrentStateName(&sm));

    while (1)
    {
        SM_Event event_buffer;
        /* Wait for event from message queue */
        rt_err_t result = rt_mq_recv(sm_mq, &event_buffer, sizeof(SM_Event), RT_WAITING_FOREVER);

        if (result == RT_EOK)
        {
            log_i("\n--- Event received: %d, dispatching to state machine ---", event_buffer.id);
            bool handled = SM_Dispatch(&sm, &event_buffer);
            if (!handled) {
                log_w("Event %d was not handled.", event_buffer.id);
            }
            log_i("Current State: %s", SM_GetCurrentStateName(&sm));
        }
    }
}

/* ===================== 10. Event Posting Interface ===================== */
/*
 * Post an event to the state machine message queue.
 * @param event_id Event ID
 * @param context  Event context pointer
 * @return RT_EOK on success, error code otherwise
 */
rt_err_t sm_post_event(uint32_t event_id, void* context)
{
    SM_Event event_to_send;
    event_to_send.id = event_id;
    event_to_send.context = context;
    /* Send event to message queue */
    return rt_mq_send_wait(sm_mq, &event_to_send, sizeof(SM_Event), 0);
}

/* ===================== 11. Initialization Function ===================== */
/*
 * Initialize the state machine, message queue, and thread.
 * @return 0 on success, -1 on failure
 */
int sm_app_init(void)
{
    static struct rt_messagequeue static_sm_mq;
    static rt_uint8_t sm_mq_pool[SM_MQ_MAX_MSGS * sizeof(SM_Event)];

    /* Initialize message queue */
    sm_mq = (rt_mq_t)&static_sm_mq;
    rt_err_t result = rt_mq_init(sm_mq,
                                 "sm_mq",
                                 &sm_mq_pool[0],
                                 sizeof(SM_Event),
                                 sizeof(sm_mq_pool),
                                 RT_IPC_FLAG_FIFO);
    if (result != RT_EOK)
    {
        log_e("Failed to initialize state machine message queue.");
        return -1;
    }

    /* Initialize state machine thread */
    static struct rt_thread static_sm_thread;
    static rt_uint8_t sm_thread_stack[SM_THREAD_STACK_SIZE];

    sm_thread = &static_sm_thread;
    rt_thread_init(sm_thread,
                   "sm_thread",
                   sm_state_machine_thread_entry,
                   RT_NULL,
                   &sm_thread_stack[0],
                   sizeof(sm_thread_stack),
                   SM_THREAD_PRIORITY,
                   SM_THREAD_TIMESLICE);

    rt_thread_startup(sm_thread);

    return 0;
}
INIT_APP_EXPORT(sm_app_init);

/* ===================== 12. Command Line Interface ===================== */
#ifdef FINSH_USING_MSH
#include <finsh.h>
/*
 * Command to post an event to the state machine.
 * Usage: sm_event_set <event>
 */
static void sm_event_set(int argc, char **argv)
{
    if (argc < 2)
    {
        rt_kprintf("Usage: sm_event_set <event>\n");
        return;
    }
    uint32_t event = 0;
    /* Map string to event id */
    if (!rt_strcmp(argv[1], "poweron"))
        event = SM_EVENT_POWER_ON;
    else if (!rt_strcmp(argv[1], "stepok"))
        event = SM_EVENT_POST_STEP_OK;
    else if (!rt_strcmp(argv[1], "stepfail"))
        event = SM_EVENT_POST_STEP_FAIL;
    else if (!rt_strcmp(argv[1], "retry"))
        event = SM_EVENT_POST_RETRY;
    else if (!rt_strcmp(argv[1], "done"))
        event = SM_EVENT_POST_DONE;
    else if (!rt_strcmp(argv[1], "run"))
        event = SM_EVENT_ENTER_RUN;
    else if (!rt_strcmp(argv[1], "runerr"))
        event = SM_EVENT_RUN_ERROR;
    else if (!rt_strcmp(argv[1], "maint"))
        event = SM_EVENT_ENTER_MAINT;
    else if (!rt_strcmp(argv[1], "exitmaint"))
        event = SM_EVENT_EXIT_MAINT;
    else if (!rt_strcmp(argv[1], "upgrade"))
        event = SM_EVENT_ENTER_UPGRADE;
    else if (!rt_strcmp(argv[1], "upgradedone"))
        event = SM_EVENT_UPGRADE_DONE;
    else if (!rt_strcmp(argv[1], "reset"))
        event = SM_EVENT_RESET;
    else if (!rt_strcmp(argv[1], "shutdown"))
        event = SM_EVENT_SHUTDOWN;
    else if (!rt_strcmp(argv[1], "recover"))
        event = SM_EVENT_FORCE_RECOVER;
    else if (!rt_strcmp(argv[1], "demo"))
    {
        /* run a full demo flow */
        rt_kprintf("Demo: run a full POST + RUN + ERROR + MAINT + UPGRADE + RESET flow\n");
        sm_post_event(SM_EVENT_POWER_ON, NULL);
        rt_thread_mdelay(1000);
        /* POST automatically advances to PostPass->Run */
        sm_post_event(SM_EVENT_RUN_ERROR, NULL);
        rt_thread_mdelay(1000);
        sm_post_event(SM_EVENT_FORCE_RECOVER, NULL);
        rt_thread_mdelay(1000);
        sm_post_event(SM_EVENT_ENTER_MAINT, NULL);
        rt_thread_mdelay(1000);
        sm_post_event(SM_EVENT_EXIT_MAINT, NULL);
        rt_thread_mdelay(1000);
        sm_post_event(SM_EVENT_ENTER_UPGRADE, NULL);
        rt_thread_mdelay(1000);
        sm_post_event(SM_EVENT_UPGRADE_DONE, NULL);
        rt_thread_mdelay(1000);
        sm_post_event(SM_EVENT_SHUTDOWN, NULL);
        rt_thread_mdelay(2000);
        return;
    }
    else
    {
        rt_kprintf("Unknown event: %s\n", argv[1]);
        return;
    }
    /* Post event to state machine */
    sm_post_event(event, NULL);
}
MSH_CMD_EXPORT(sm_event_set, sm_event_set <event>);

/*
 * Command to get the current state name.
 */
static void sm_current_get(int argc, char **argv)
{
    /* Print current state name */
    rt_kprintf("Current state is %s\n", SM_GetCurrentStateName(&sm));
}
MSH_CMD_EXPORT(sm_current_get, get current state);
#endif
