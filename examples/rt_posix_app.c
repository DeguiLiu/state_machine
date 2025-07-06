#include "state_machine_rt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <mqueue.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <stdbool.h>

/* State machine thread and queue configuration macros */
#define SM_THREAD_PRIORITY   0   /* Not used in POSIX */
#define SM_THREAD_STACK_SIZE 4096
#define SM_MQ_MAX_MSGS 10
#define SM_MAX_STATE_DEPTH 8
#define SM_MQ_NAME "/sm_posix_app_rt"

// 声明 sm_rt 以便前面函数可用
extern SM_RT_Instance sm_rt;

/* ===================== 1. Event Definitions ===================== */
typedef enum
{
    SM_EVENT_POWER_ON = 1,
    SM_EVENT_POST_STEP_OK,
    SM_EVENT_POST_STEP_FAIL,
    SM_EVENT_POST_RETRY,
    SM_EVENT_POST_DONE,
    SM_EVENT_ENTER_RUN,
    SM_EVENT_RUN_ERROR,
    SM_EVENT_ENTER_MAINT,
    SM_EVENT_EXIT_MAINT,
    SM_EVENT_ENTER_UPGRADE,
    SM_EVENT_UPGRADE_DONE,
    SM_EVENT_RESET,
    SM_EVENT_SHUTDOWN,
    SM_EVENT_FORCE_RECOVER
} sm_event_t;

/* ===================== 2. State Forward Declarations ===================== */
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
typedef struct
{
    int32_t post_step;
    int32_t post_fail_count;
    int32_t run_error_count;
    int32_t upgrade_flag;
} sm_system_data_t;

/* ===================== 4. Actions and Guard Functions ===================== */
static const char *sm_get_state_name(SM_StateMachine *sm)
{
    const char *name = NULL;
    SM_RT_GetCurrentStateName((const SM_RT_Instance *)((char*)sm - offsetof(SM_RT_Instance, base_sm)), &name);
    return name ? name : "Unknown";
}
static void sm_entry_print(SM_StateMachine *sm, const SM_Event *event)
{
    (void)event;
    printf("==> Enter %s\n", sm_get_state_name(sm));
    // 如果进入PowerOn状态，自动触发POST流程
    if (strcmp(sm_get_state_name(sm), "PowerOn") == 0) {
        SM_Event evt = {SM_EVENT_POST_STEP_OK, NULL};
        SM_RT_PostEvent(&sm_rt, &evt);
    }
}
static void sm_exit_print(SM_StateMachine *sm, const SM_Event *event)
{
    (void)event;
    printf("<== Exit %s\n", sm_get_state_name(sm));
}
static void sm_entry_post(SM_StateMachine *sm, const SM_Event *event)
{
    (void)event;
    sm_system_data_t *data = (sm_system_data_t*)sm->userData;
    data->post_step = 0;
    data->post_fail_count = 0;
    printf("POST: Start self-check sequence.\n");
}
static void sm_entry_post_step(SM_StateMachine *sm, const SM_Event *event)
{
    (void)event;
    sm_system_data_t *data = (sm_system_data_t*)sm->userData;
    data->post_step++;
    printf("POST: Step %d started.\n", data->post_step);
    if ((data->post_step % 2) == 0) {
        printf("POST: Step %d failed!\n", data->post_step);
        SM_Event fail_evt = {SM_EVENT_POST_STEP_FAIL, NULL};
        SM_RT_PostEvent(&sm_rt, &fail_evt);
    } else if (data->post_step < 3) {
        printf("POST: Step %d ok.\n", data->post_step);
        SM_Event ok_evt = {SM_EVENT_POST_STEP_OK, NULL};
        SM_RT_PostEvent(&sm_rt, &ok_evt);
    } else {
        printf("POST: All steps done.\n");
        SM_Event done_evt = {SM_EVENT_POST_DONE, NULL};
        SM_RT_PostEvent(&sm_rt, &done_evt);
    }
}
static void sm_entry_post_retry(SM_StateMachine *sm, const SM_Event *event)
{
    (void)event;
    sm_system_data_t *data = (sm_system_data_t*)sm->userData;
    data->post_fail_count++;
    printf("POST: Retry %d\n", data->post_fail_count);
    if (data->post_fail_count < 2) {
        SM_Event retry_evt = {SM_EVENT_POST_RETRY, NULL};
        SM_RT_PostEvent(&sm_rt, &retry_evt);
    } else {
        printf("POST: Retry failed, enter FAIL.\n");
        SM_Event fail_evt = {SM_EVENT_POST_STEP_FAIL, NULL};
        SM_RT_PostEvent(&sm_rt, &fail_evt);
    }
}
static void sm_entry_post_fail(SM_StateMachine *sm, const SM_Event *event)
{
    (void)sm;
    (void)event;
    printf("POST: Self-check failed! Wait for manual reset or force recover.\n");
}
static void sm_entry_post_pass(SM_StateMachine *sm, const SM_Event *event)
{
    (void)sm;
    (void)event;
    printf("POST: Self-check passed.\n");
    SM_Event enter_run = {SM_EVENT_ENTER_RUN, NULL};
    SM_RT_PostEvent(&sm_rt, &enter_run);
}
static void sm_entry_run(SM_StateMachine *sm, const SM_Event *event)
{
    (void)sm;
    (void)event;
    printf("System running normally.\n");
}
static void sm_entry_run_error(SM_StateMachine *sm, const SM_Event *event)
{
    (void)event;
    sm_system_data_t *data = (sm_system_data_t*)sm->userData;
    data->run_error_count++;
    printf("System running error! Error count: %d\n", data->run_error_count);
}
static void sm_entry_maint(SM_StateMachine *sm, const SM_Event *event)
{
    (void)sm;
    (void)event;
    printf("Enter maintenance mode.\n");
}
static void sm_entry_upgrade(SM_StateMachine *sm, const SM_Event *event)
{
    (void)event;
    sm_system_data_t *data = (sm_system_data_t*)sm->userData;
    data->upgrade_flag = 1;
    printf("Enter upgrade mode.\n");
}
static void sm_entry_upgrade_done(SM_StateMachine *sm, const SM_Event *event)
{
    (void)event;
    sm_system_data_t *data = (sm_system_data_t*)sm->userData;
    data->upgrade_flag = 0;
    printf("Upgrade finished, system will reset.\n");
    SM_Event reset_evt = {SM_EVENT_RESET, NULL};
    SM_RT_PostEvent(&sm_rt, &reset_evt);
}
static bool sm_guard_post_retry(SM_StateMachine *sm, const SM_Event *event)
{
    (void)event;
    sm_system_data_t *data = (sm_system_data_t*)sm->userData;
    return data->post_fail_count < 2;
}
static bool sm_guard_run_error_limit(SM_StateMachine *sm, const SM_Event *event)
{
    (void)event;
    sm_system_data_t *data = (sm_system_data_t*)sm->userData;
    return data->run_error_count < 3;
}

/* ===================== 5. Transition Tables ===================== */
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

/* ===================== 7. POSIX Resources ===================== */
static pthread_t sm_thread;
static mqd_t sm_mq = (mqd_t)-1;
SM_RT_Instance sm_rt;
static sm_system_data_t sm_sys_data;
static const SM_State *sm_entry_path_buffer[SM_MAX_STATE_DEPTH];
static pthread_mutex_t sm_mutex = PTHREAD_MUTEX_INITIALIZER;
// 如需消除未使用警告，可加如下行：
// (void)sm_entry_path_buffer;

/* ===================== 8. Unhandled Event Hook ===================== */
static void sm_on_unhandled_event(SM_StateMachine *sm, const SM_Event *event)
{
    (void)event;
    printf("--- Unhandled Event: Event %u received in state '%s' ---\n",
           (unsigned)event->id, sm_get_state_name(sm));
}

/* ===================== 9. State Machine Thread Entry ===================== */
static void* sm_state_machine_thread_entry(void* parameter)
{
    (void)parameter;
    SM_RT_Init(&sm_rt, &sm_state_off, sm_entry_path_buffer, SM_MAX_STATE_DEPTH, &sm_sys_data, sm_on_unhandled_event);
    SM_RT_Start(&sm_rt);
    printf("Complex RT State machine initialized. Initial State: %s\n", sm_get_state_name(&sm_rt.base_sm));

    while (1)
    {
        SM_Event event_buffer;
        ssize_t n = mq_receive(sm_mq, (char*)&event_buffer, sizeof(SM_Event), NULL);
        if (n == sizeof(SM_Event))
        {
            pthread_mutex_lock(&sm_mutex);
            printf("\n--- Event received: %d, dispatching to state machine ---\n", event_buffer.id);
            SM_RT_PostEvent(&sm_rt, &event_buffer);
            pthread_mutex_unlock(&sm_mutex);
            printf("Current State: %s\n", sm_get_state_name(&sm_rt.base_sm));
        }
        else
        {
            sleep(1); // Sleep to avoid busy waiting
        }
    }
    return NULL;
}

/* ===================== 10. Event Posting Interface ===================== */
int sm_post_event(uint32_t event_id, void* context)
{
    SM_Event event_to_send;
    event_to_send.id = event_id;
    event_to_send.context = context;
    return mq_send(sm_mq, (const char*)&event_to_send, sizeof(SM_Event), 0);
}

/* ===================== 11. Initialization Function ===================== */
int sm_app_init(void)
{
    struct mq_attr attr;
    memset(&attr, 0, sizeof(attr));
    attr.mq_flags = 0;
    attr.mq_maxmsg = SM_MQ_MAX_MSGS;
    attr.mq_msgsize = sizeof(SM_Event);

    mq_unlink(SM_MQ_NAME);
    sm_mq = mq_open(SM_MQ_NAME, O_CREAT | O_RDWR, 0666, &attr);
    if (sm_mq == (mqd_t)-1)
    {
        perror("Failed to initialize state machine message queue");
        return -1;
    }

    pthread_mutex_init(&sm_mutex, NULL);

    if (pthread_create(&sm_thread, NULL, sm_state_machine_thread_entry, NULL) != 0)
    {
        perror("Failed to create state machine thread");
        mq_close(sm_mq);
        mq_unlink(SM_MQ_NAME);
        return -1;
    }

    return 0;
}

/* ===================== 12. Command Line Interface ===================== */
static void print_help(void)
{
    printf("Usage: <cmd> [event]\n");
    printf("Events:\n");
    printf("  poweron stepok stepfail retry done run runerr maint exitmaint upgrade upgradedone reset shutdown recover demo\n");
    printf("Example: ./rt_posix_app poweron\n");
}

int main(int argc, char **argv)
{
    if (sm_app_init() != 0)
    {
        printf("State Machine Application Initialization Failed!\n");
        return -1;
    }

    if (argc < 2)
    {
        print_help();
        pthread_join(sm_thread, NULL);
        mq_close(sm_mq);
        mq_unlink(SM_MQ_NAME);
        pthread_mutex_destroy(&sm_mutex);
        return 0;
    }

    if (strcmp(argv[1], "demo") == 0)
    {
        printf("Demo: run a full POST + RUN + ERROR + MAINT + UPGRADE + RESET flow\n");
        sm_post_event(SM_EVENT_POWER_ON, NULL);
        sleep(1);
        sm_post_event(SM_EVENT_RUN_ERROR, NULL);
        sleep(1);
        sm_post_event(SM_EVENT_FORCE_RECOVER, NULL);
        sleep(1);
        sm_post_event(SM_EVENT_ENTER_MAINT, NULL);
        sleep(1);
        sm_post_event(SM_EVENT_EXIT_MAINT, NULL);
        sleep(1);
        sm_post_event(SM_EVENT_ENTER_UPGRADE, NULL);
        sleep(1);
        sm_post_event(SM_EVENT_UPGRADE_DONE, NULL);
        sleep(1);
        sm_post_event(SM_EVENT_SHUTDOWN, NULL);
        sleep(2);
    }
    else
    {
        uint32_t event = 0;
        if (!strcmp(argv[1], "poweron"))
            event = SM_EVENT_POWER_ON;
        else if (!strcmp(argv[1], "stepok"))
            event = SM_EVENT_POST_STEP_OK;
        else if (!strcmp(argv[1], "stepfail"))
            event = SM_EVENT_POST_STEP_FAIL;
        else if (!strcmp(argv[1], "retry"))
            event = SM_EVENT_POST_RETRY;
        else if (!strcmp(argv[1], "done"))
            event = SM_EVENT_POST_DONE;
        else if (!strcmp(argv[1], "run"))
            event = SM_EVENT_ENTER_RUN;
        else if (!strcmp(argv[1], "runerr"))
            event = SM_EVENT_RUN_ERROR;
        else if (!strcmp(argv[1], "maint"))
            event = SM_EVENT_ENTER_MAINT;
        else if (!strcmp(argv[1], "exitmaint"))
            event = SM_EVENT_EXIT_MAINT;
        else if (!strcmp(argv[1], "upgrade"))
            event = SM_EVENT_ENTER_UPGRADE;
        else if (!strcmp(argv[1], "upgradedone"))
            event = SM_EVENT_UPGRADE_DONE;
        else if (!strcmp(argv[1], "reset"))
            event = SM_EVENT_RESET;
        else if (!strcmp(argv[1], "shutdown"))
            event = SM_EVENT_SHUTDOWN;
        else if (!strcmp(argv[1], "recover"))
            event = SM_EVENT_FORCE_RECOVER;
        else
        {
            print_help();
            pthread_join(sm_thread, NULL);
            mq_close(sm_mq);
            mq_unlink(SM_MQ_NAME);
            pthread_mutex_destroy(&sm_mutex);
            return 0;
        }

        sm_post_event(event, NULL);

        sleep(1);
    }

    pthread_cancel(sm_thread);
    pthread_join(sm_thread, NULL);
    mq_close(sm_mq);
    mq_unlink(SM_MQ_NAME);
    pthread_mutex_destroy(&sm_mutex);

    return 0;
}

/*
test@pi:~/state_machine$ ./rt_posix_app demo
Demo: run a full POST + RUN + ERROR + MAINT + UPGRADE + RESET flow
==> Enter Unknown
Complex RT State machine initialized. Initial State: Off

--- Event received: 1, dispatching to state machine ---
<== Exit Off
==> Enter PowerOn
POST: Start self-check sequence.
POST: Step 1 started.
POST: Step 1 ok.
<== Exit PostStep
POST: Step 2 started.
POST: Step 2 failed!
<== Exit PostStep
POST: Retry 1
<== Exit PostRetry
POST: Step 3 started.
POST: All steps done.
<== Exit PostStep
POST: Self-check passed.
<== Exit PostPass
<== Exit PostPass
<== Exit PostPass
System running normally.
Current State: Run

--- Event received: 7, dispatching to state machine ---
System running error! Error count: 1
Current State: RunError

--- Event received: 14, dispatching to state machine ---
<== Exit RunError
Current State: Run

--- Event received: 8, dispatching to state machine ---
<== Exit Run
Enter maintenance mode.
Current State: Maint

--- Event received: 9, dispatching to state machine ---
<== Exit Maint
System running normally.
Current State: Run

--- Event received: 10, dispatching to state machine ---
<== Exit Run
Enter upgrade mode.
Current State: Upgrade

--- Event received: 11, dispatching to state machine ---
Upgrade finished, system will reset.
<== Exit UpgradeDone
<== Exit UpgradeDone
==> Enter Off
Current State: Off

--- Event received: 13, dispatching to state machine ---
--- Unhandled Event: Event 13 received in state 'Off' ---
Current State: Off

*/