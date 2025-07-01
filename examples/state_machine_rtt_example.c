/*
 * Copyright (c) 2013 Andreas Misje
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

/**
 * @file state_machine_rtt_example.c
 * @brief Comprehensive test example for RT-Thread state machine wrapper.
 *
 * This example demonstrates:
 * - Basic initialization and configuration
 * - Synchronous event processing
 * - Asynchronous event processing
 * - Multi-threaded safety
 * - Statistics collection
 * - Error handling
 */

#ifndef RT_THREAD_MOCK
#define RT_THREAD_MOCK  /* Use mock RT-Thread for testing */
#endif

#include "state_machine_rtt.h"
#include <stdio.h>
#include <unistd.h>  /* for sleep */

/* --- Event Definitions --- */
enum {
    EV_POWER_ON = 1,
    EV_START_TASK,
    EV_TASK_COMPLETE,
    EV_POWER_OFF,
    EV_ERROR,
    EV_RESET
};

/* --- Forward Declarations of States --- */
static const SM_State STATE_Off;
static const SM_State STATE_On;
static const SM_State STATE_Idle;
static const SM_State STATE_Running;
static const SM_State STATE_Error;

/* --- User Data Structure --- */
typedef struct {
    int tasks_completed;
    int error_count;
} AppData;

/* --- Action & Guard Functions --- */
static void entry_Off(SM_StateMachine *sm, const SM_Event *event) {
    printf("  (Entry) -> Off\n");
}

static void entry_On(SM_StateMachine *sm, const SM_Event *event) {
    printf("  (Entry) -> On\n");
}

static void exit_On(SM_StateMachine *sm, const SM_Event *event) {
    printf("  (Exit)  -> On\n");
}

static void entry_Idle(SM_StateMachine *sm, const SM_Event *event) {
    printf("    (Entry) -> Idle\n");
}

static void entry_Running(SM_StateMachine *sm, const SM_Event *event) {
    printf("    (Entry) -> Running\n");
}

static void exit_Running(SM_StateMachine *sm, const SM_Event *event) {
    printf("    (Exit)  -> Running\n");
}

static void entry_Error(SM_StateMachine *sm, const SM_Event *event) {
    AppData *data = (AppData *)sm->userData;
    data->error_count++;
    printf("    (Entry) -> Error (count: %d)\n", data->error_count);
}

static void on_power_off(SM_StateMachine *sm, const SM_Event *event) {
    printf("  Action: Shutting down...\n");
}

static void on_task_done(SM_StateMachine *sm, const SM_Event *event) {
    AppData *data = (AppData *)sm->userData;
    data->tasks_completed++;
    printf("  Action: Task finished. Total completed: %d\n", data->tasks_completed);
}

static void on_error_recovery(SM_StateMachine *sm, const SM_Event *event) {
    printf("  Action: Recovering from error...\n");
}

static bool can_start_task(SM_StateMachine *sm, const SM_Event *event) {
    AppData *data = (AppData *)sm->userData;
    bool can_start = (data->tasks_completed < 5);
    printf("  Guard: Can start task? %s (completed: %d/5)\n", 
           can_start ? "Yes" : "No", data->tasks_completed);
    return can_start;
}

/* --- Transition Tables --- */
static const SM_Transition T_Off[] = {
    {EV_POWER_ON, &STATE_Idle, NULL, NULL, SM_TRANSITION_EXTERNAL}
};

static const SM_Transition T_On[] = {
    {EV_POWER_OFF, &STATE_Off, NULL, on_power_off, SM_TRANSITION_EXTERNAL},
    {EV_ERROR, &STATE_Error, NULL, NULL, SM_TRANSITION_EXTERNAL}
};

static const SM_Transition T_Idle[] = {
    {EV_START_TASK, &STATE_Running, can_start_task, NULL, SM_TRANSITION_EXTERNAL}
};

static const SM_Transition T_Running[] = {
    {EV_TASK_COMPLETE, &STATE_Idle, NULL, on_task_done, SM_TRANSITION_EXTERNAL}
};

static const SM_Transition T_Error[] = {
    {EV_RESET, &STATE_Idle, NULL, on_error_recovery, SM_TRANSITION_EXTERNAL}
};

/* --- State Definitions --- */
static const SM_State STATE_Off = {
    .parent = NULL,
    .entryAction = entry_Off,
    .exitAction = NULL,
    .transitions = T_Off,
    .numTransitions = sizeof(T_Off) / sizeof(T_Off[0]),
    .name = "Off"
};

static const SM_State STATE_On = {
    .parent = NULL,
    .entryAction = entry_On,
    .exitAction = exit_On,
    .transitions = T_On,
    .numTransitions = sizeof(T_On) / sizeof(T_On[0]),
    .name = "On"
};

static const SM_State STATE_Idle = {
    .parent = &STATE_On,
    .entryAction = entry_Idle,
    .exitAction = NULL,
    .transitions = T_Idle,
    .numTransitions = sizeof(T_Idle) / sizeof(T_Idle[0]),
    .name = "Idle"
};

static const SM_State STATE_Running = {
    .parent = &STATE_On,
    .entryAction = entry_Running,
    .exitAction = exit_Running,
    .transitions = T_Running,
    .numTransitions = sizeof(T_Running) / sizeof(T_Running[0]),
    .name = "Running"
};

static const SM_State STATE_Error = {
    .parent = &STATE_On,
    .entryAction = entry_Error,
    .exitAction = NULL,
    .transitions = T_Error,
    .numTransitions = sizeof(T_Error) / sizeof(T_Error[0]),
    .name = "Error"
};

/* --- Global Variables --- */
static SM_RTT_Instance rtt_sm;
static AppData app_data;
static const SM_State *path_buffer[8];

/* --- Helper Functions --- */
static void on_unhandled_event(SM_StateMachine *sm, const SM_Event *event) {
    printf("--- Unhandled Event: Event %u received in state '%s' ---\n",
           (unsigned)event->id, SM_GetCurrentStateName(sm));
}

static void print_result(const char *operation, SM_RTT_Result result) {
    const char *result_str = "UNKNOWN";
    
    switch (result) {
        case SM_RTT_RESULT_SUCCESS: result_str = "SUCCESS"; break;
        case SM_RTT_RESULT_ERROR_NULL_PTR: result_str = "NULL_PTR"; break;
        case SM_RTT_RESULT_ERROR_INVALID: result_str = "INVALID"; break;
        case SM_RTT_RESULT_ERROR_NOT_INIT: result_str = "NOT_INIT"; break;
        case SM_RTT_RESULT_ERROR_ALREADY_INIT: result_str = "ALREADY_INIT"; break;
        case SM_RTT_RESULT_ERROR_NOT_STARTED: result_str = "NOT_STARTED"; break;
        case SM_RTT_RESULT_ERROR_ALREADY_STARTED: result_str = "ALREADY_STARTED"; break;
        case SM_RTT_RESULT_ERROR_QUEUE_FULL: result_str = "QUEUE_FULL"; break;
        case SM_RTT_RESULT_ERROR_UNKNOWN: result_str = "UNKNOWN"; break;
    }
    
    printf("%s: %s\n", operation, result_str);
}

static void print_statistics(void) {
    SM_RTT_Statistics stats;
    SM_RTT_Result result = SM_RTT_GetStatistics(&rtt_sm, &stats);
    
    if (result == SM_RTT_RESULT_SUCCESS) {
        printf("\n--- Statistics ---\n");
        printf("Events processed: %u\n", (unsigned)stats.total_events_processed);
        printf("Events unhandled: %u\n", (unsigned)stats.total_events_unhandled);
        printf("State transitions: %u\n", (unsigned)stats.total_transitions);
        printf("Current queue depth: %u\n", (unsigned)stats.current_queue_depth);
        printf("Max queue depth: %u\n", (unsigned)stats.max_queue_depth);
        printf("------------------\n\n");
    } else {
        printf("Failed to get statistics\n");
    }
}

static void dispatch_sync_event(const char *event_name, uint32_t event_id, void *context) {
    SM_Event event = {event_id, context};
    printf("\n--- Synchronous Event: %s ---\n", event_name);
    SM_RTT_Result result = SM_RTT_DispatchSync(&rtt_sm, &event);
    print_result("DispatchSync", result);
    
    const char *state_name = NULL;
    if (SM_RTT_GetCurrentStateName(&rtt_sm, &state_name) == SM_RTT_RESULT_SUCCESS) {
        printf("Current State: %s\n", state_name);
    }
}

static void post_async_event(const char *event_name, uint32_t event_id, void *context) {
    SM_Event event = {event_id, context};
    printf("\n--- Asynchronous Event: %s ---\n", event_name);
    SM_RTT_Result result = SM_RTT_PostEvent(&rtt_sm, &event);
    print_result("PostEvent", result);
}

/* --- Test Functions --- */
static void test_initialization_and_cleanup(void) {
    printf("\n=== Test: Initialization and Cleanup ===\n");
    
    /* Configure the RT-Thread state machine */
    SM_RTT_Config config = {
        .queue_size = 16,
        .thread_stack_size = 2048,
        .thread_priority = 10,
        .thread_timeslice = 20,
        .thread_name = "sm_worker",
        .queue_name = "sm_queue",
        .mutex_name = "sm_mutex"
    };
    
    /* Initialize app data */
    app_data.tasks_completed = 0;
    app_data.error_count = 0;
    
    /* Test initialization */
    SM_RTT_Result result = SM_RTT_Init(&rtt_sm, &config, &STATE_Off, path_buffer, 
                                       sizeof(path_buffer)/sizeof(path_buffer[0]), 
                                       &app_data, on_unhandled_event);
    print_result("Init", result);
    
    /* Check initial state */
    const char *state_name = NULL;
    if (SM_RTT_GetCurrentStateName(&rtt_sm, &state_name) == SM_RTT_RESULT_SUCCESS) {
        printf("Initial State: %s\n", state_name);
    }
    
    /* Test double initialization (should fail) */
    result = SM_RTT_Init(&rtt_sm, &config, &STATE_Off, path_buffer, 
                         sizeof(path_buffer)/sizeof(path_buffer[0]), 
                         &app_data, on_unhandled_event);
    print_result("Double Init (should fail)", result);
}

static void test_synchronous_processing(void) {
    printf("\n=== Test: Synchronous Event Processing ===\n");
    
    /* Test basic state transitions */
    dispatch_sync_event("POWER_ON", EV_POWER_ON, NULL);
    dispatch_sync_event("START_TASK", EV_START_TASK, NULL);
    dispatch_sync_event("TASK_COMPLETE", EV_TASK_COMPLETE, NULL);
    dispatch_sync_event("ERROR", EV_ERROR, NULL);
    dispatch_sync_event("RESET", EV_RESET, NULL);
    
    /* Test unhandled event */
    dispatch_sync_event("INVALID_EVENT", 999, NULL);
    
    print_statistics();
}

static void test_asynchronous_processing(void) {
    printf("\n=== Test: Asynchronous Event Processing ===\n");
    
    /* Start the worker thread */
    SM_RTT_Result result = SM_RTT_Start(&rtt_sm);
    print_result("Start", result);
    
    if (result == SM_RTT_RESULT_SUCCESS) {
        /* Post several events to the queue */
        post_async_event("START_TASK", EV_START_TASK, NULL);
        usleep(100000);  /* 100ms delay to allow processing */
        
        post_async_event("TASK_COMPLETE", EV_TASK_COMPLETE, NULL);
        usleep(100000);
        
        post_async_event("START_TASK", EV_START_TASK, NULL);
        usleep(100000);
        
        post_async_event("TASK_COMPLETE", EV_TASK_COMPLETE, NULL);
        usleep(100000);
        
        /* Allow some time for processing */
        sleep(1);
        
        print_statistics();
        
        /* Stop the worker thread */
        result = SM_RTT_Stop(&rtt_sm);
        print_result("Stop", result);
    }
}

static void test_error_handling(void) {
    printf("\n=== Test: Error Handling ===\n");
    
    /* Test operations on uninitialized state machine */
    SM_RTT_Instance uninit_sm = {0};
    SM_Event dummy_event = {EV_POWER_ON, NULL};
    
    SM_RTT_Result result = SM_RTT_DispatchSync(&uninit_sm, &dummy_event);
    print_result("DispatchSync on uninitialized SM", result);
    
    result = SM_RTT_Start(&uninit_sm);
    print_result("Start on uninitialized SM", result);
    
    /* Test NULL parameter handling */
    result = SM_RTT_DispatchSync(NULL, &dummy_event);
    print_result("DispatchSync with NULL SM", result);
    
    result = SM_RTT_DispatchSync(&rtt_sm, NULL);
    print_result("DispatchSync with NULL event", result);
    
    /* Test operations before start */
    result = SM_RTT_PostEvent(&rtt_sm, &dummy_event);
    print_result("PostEvent before start", result);
}

static void test_multithreaded_safety(void) {
    printf("\n=== Test: Multi-threaded Safety ===\n");
    
    /* Start the worker thread */
    SM_RTT_Result result = SM_RTT_Start(&rtt_sm);
    print_result("Start for MT test", result);
    
    if (result == SM_RTT_RESULT_SUCCESS) {
        /* Mix synchronous and asynchronous events */
        for (int i = 0; i < 3; i++) {
            post_async_event("START_TASK (async)", EV_START_TASK, NULL);
            usleep(50000);  /* 50ms */
            
            dispatch_sync_event("TASK_COMPLETE (sync)", EV_TASK_COMPLETE, NULL);
            usleep(50000);
        }
        
        /* Allow processing to complete */
        sleep(1);
        
        print_statistics();
        
        /* Stop the worker thread */
        result = SM_RTT_Stop(&rtt_sm);
        print_result("Stop after MT test", result);
    }
}

static void test_cleanup(void) {
    printf("\n=== Test: Cleanup ===\n");
    
    /* Test statistics reset */
    SM_RTT_Result result = SM_RTT_ResetStatistics(&rtt_sm);
    print_result("ResetStatistics", result);
    print_statistics();
    
    /* Test deinitialization */
    result = SM_RTT_Deinit(&rtt_sm);
    print_result("Deinit", result);
    
    /* Test double deinitialization */
    result = SM_RTT_Deinit(&rtt_sm);
    print_result("Double Deinit (should fail)", result);
}

/* --- Main Function --- */
int main(void) {
    printf("RT-Thread State Machine Test Example\n");
    printf("====================================\n");
    
    test_initialization_and_cleanup();
    test_synchronous_processing();
    test_asynchronous_processing();
    test_error_handling();
    test_multithreaded_safety();
    test_cleanup();
    
    printf("\n=== Test Complete ===\n");
    printf("Final app data - Tasks completed: %d, Errors: %d\n", 
           app_data.tasks_completed, app_data.error_count);
    
    return 0;
}