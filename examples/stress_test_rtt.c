/*
 * Advanced stress test for RT-Thread state machine multi-threading capabilities
 */

#ifndef RT_THREAD_MOCK
#define RT_THREAD_MOCK
#endif

#include "state_machine_rtt.h"
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>

/* Test configuration */
#define NUM_PRODUCER_THREADS 3
#define NUM_EVENTS_PER_THREAD 50
#define TEST_DURATION_SECONDS 5

/* Event definitions */
enum {
    EV_START = 1,
    EV_PROCESS,
    EV_COMPLETE,
    EV_ERROR,
    EV_RESET
};

/* State definitions */
static const SM_State STATE_Idle;
static const SM_State STATE_Working;
static const SM_State STATE_Error;

/* User data */
typedef struct {
    int processed_count;
    int error_count;
    pthread_mutex_t data_mutex;
} StressTestData;

static StressTestData test_data;
static SM_RTT_Instance rtt_sm;
static const SM_State *path_buffer[8];

/* Action functions */
static void entry_idle(SM_StateMachine *sm, const SM_Event *event) {
    printf("-> Idle\n");
}

static void entry_working(SM_StateMachine *sm, const SM_Event *event) {
    StressTestData *data = (StressTestData *)sm->userData;
    pthread_mutex_lock(&data->data_mutex);
    data->processed_count++;
    pthread_mutex_unlock(&data->data_mutex);
    printf("-> Working (processed: %d)\n", data->processed_count);
}

static void entry_error(SM_StateMachine *sm, const SM_Event *event) {
    StressTestData *data = (StressTestData *)sm->userData;
    pthread_mutex_lock(&data->data_mutex);
    data->error_count++;
    pthread_mutex_unlock(&data->data_mutex);
    printf("-> Error (count: %d)\n", data->error_count);
}

static void on_reset(SM_StateMachine *sm, const SM_Event *event) {
    printf("  Action: Resetting...\n");
}

/* Transition tables */
static const SM_Transition T_Idle[] = {
    {EV_START, &STATE_Working, NULL, NULL, SM_TRANSITION_EXTERNAL},
    {EV_ERROR, &STATE_Error, NULL, NULL, SM_TRANSITION_EXTERNAL}
};

static const SM_Transition T_Working[] = {
    {EV_COMPLETE, &STATE_Idle, NULL, NULL, SM_TRANSITION_EXTERNAL},
    {EV_ERROR, &STATE_Error, NULL, NULL, SM_TRANSITION_EXTERNAL}
};

static const SM_Transition T_Error[] = {
    {EV_RESET, &STATE_Idle, NULL, on_reset, SM_TRANSITION_EXTERNAL}
};

/* State definitions */
static const SM_State STATE_Idle = {
    .parent = NULL,
    .entryAction = entry_idle,
    .exitAction = NULL,
    .transitions = T_Idle,
    .numTransitions = sizeof(T_Idle) / sizeof(T_Idle[0]),
    .name = "Idle"
};

static const SM_State STATE_Working = {
    .parent = NULL,
    .entryAction = entry_working,
    .exitAction = NULL,
    .transitions = T_Working,
    .numTransitions = sizeof(T_Working) / sizeof(T_Working[0]),
    .name = "Working"
};

static const SM_State STATE_Error = {
    .parent = NULL,
    .entryAction = entry_error,
    .exitAction = NULL,
    .transitions = T_Error,
    .numTransitions = sizeof(T_Error) / sizeof(T_Error[0]),
    .name = "Error"
};

/* Thread data structure */
typedef struct {
    int thread_id;
    int events_sent;
    SM_RTT_Instance *sm;
} ProducerThreadData;

/* Producer thread function */
static void* producer_thread(void* arg) {
    ProducerThreadData *data = (ProducerThreadData*)arg;
    SM_Event event;
    uint32_t event_types[] = {EV_START, EV_COMPLETE, EV_ERROR, EV_RESET};
    int num_event_types = sizeof(event_types) / sizeof(event_types[0]);
    
    srand(time(NULL) + data->thread_id);
    
    for (int i = 0; i < NUM_EVENTS_PER_THREAD; i++) {
        /* Random event type */
        event.id = event_types[rand() % num_event_types];
        event.context = (void*)(intptr_t)data->thread_id;
        
        /* Post event asynchronously */
        SM_RTT_Result result = SM_RTT_PostEvent(data->sm, &event);
        if (result == SM_RTT_RESULT_SUCCESS) {
            data->events_sent++;
        } else if (result == SM_RTT_RESULT_ERROR_QUEUE_FULL) {
            printf("Thread %d: Queue full, retrying...\n", data->thread_id);
            usleep(1000); /* 1ms delay */
            i--; /* Retry this event */
            continue;
        }
        
        /* Random delay between events */
        usleep((rand() % 10000) + 1000); /* 1-11ms */
    }
    
    printf("Producer thread %d finished, sent %d events\n", data->thread_id, data->events_sent);
    return NULL;
}

/* Monitor thread function */
static void* monitor_thread(void* arg) {
    SM_RTT_Statistics stats;
    time_t start_time = time(NULL);
    
    while (time(NULL) - start_time < TEST_DURATION_SECONDS) {
        if (SM_RTT_GetStatistics(&rtt_sm, &stats) == SM_RTT_RESULT_SUCCESS) {
            printf("Monitor: Events processed: %u, unhandled: %u, transitions: %u, "
                   "queue depth: %u, max depth: %u\n",
                   (unsigned)stats.total_events_processed,
                   (unsigned)stats.total_events_unhandled,
                   (unsigned)stats.total_transitions,
                   (unsigned)stats.current_queue_depth,
                   (unsigned)stats.max_queue_depth);
        }
        sleep(1);
    }
    
    return NULL;
}

static void on_unhandled_event(SM_StateMachine *sm, const SM_Event *event) {
    printf("Unhandled event %u in state %s\n", 
           (unsigned)event->id, SM_GetCurrentStateName(sm));
}

int main(void) {
    printf("RT-Thread State Machine Stress Test\n");
    printf("===================================\n");
    
    /* Initialize test data */
    test_data.processed_count = 0;
    test_data.error_count = 0;
    pthread_mutex_init(&test_data.data_mutex, NULL);
    
    /* Configure state machine */
    SM_RTT_Config config = {
        .queue_size = 32,  /* Larger queue for stress test */
        .thread_stack_size = 4096,
        .thread_priority = 10,
        .thread_timeslice = 20,
        .thread_name = "stress_sm",
        .queue_name = "stress_queue",
        .mutex_name = "stress_mutex"
    };
    
    /* Initialize state machine */
    SM_RTT_Result result = SM_RTT_Init(&rtt_sm, &config, &STATE_Idle, path_buffer,
                                       sizeof(path_buffer)/sizeof(path_buffer[0]),
                                       &test_data, on_unhandled_event);
    if (result != SM_RTT_RESULT_SUCCESS) {
        printf("Failed to initialize state machine\n");
        return 1;
    }
    
    /* Start worker thread */
    result = SM_RTT_Start(&rtt_sm);
    if (result != SM_RTT_RESULT_SUCCESS) {
        printf("Failed to start state machine\n");
        return 1;
    }
    
    printf("Starting stress test with %d producer threads...\n", NUM_PRODUCER_THREADS);
    
    /* Create producer threads */
    pthread_t producer_threads[NUM_PRODUCER_THREADS];
    ProducerThreadData thread_data[NUM_PRODUCER_THREADS];
    
    for (int i = 0; i < NUM_PRODUCER_THREADS; i++) {
        thread_data[i].thread_id = i + 1;
        thread_data[i].events_sent = 0;
        thread_data[i].sm = &rtt_sm;
        
        if (pthread_create(&producer_threads[i], NULL, producer_thread, &thread_data[i]) != 0) {
            printf("Failed to create producer thread %d\n", i);
            return 1;
        }
    }
    
    /* Create monitor thread */
    pthread_t monitor_tid;
    if (pthread_create(&monitor_tid, NULL, monitor_thread, NULL) != 0) {
        printf("Failed to create monitor thread\n");
        return 1;
    }
    
    /* Wait for all producer threads to complete */
    for (int i = 0; i < NUM_PRODUCER_THREADS; i++) {
        pthread_join(producer_threads[i], NULL);
    }
    
    /* Allow some time for remaining events to be processed */
    sleep(2);
    
    /* Stop monitor thread */
    pthread_cancel(monitor_tid);
    pthread_join(monitor_tid, NULL);
    
    /* Final statistics */
    SM_RTT_Statistics final_stats;
    if (SM_RTT_GetStatistics(&rtt_sm, &final_stats) == SM_RTT_RESULT_SUCCESS) {
        printf("\n=== Final Statistics ===\n");
        printf("Total events processed: %u\n", (unsigned)final_stats.total_events_processed);
        printf("Total events unhandled: %u\n", (unsigned)final_stats.total_events_unhandled);
        printf("Total state transitions: %u\n", (unsigned)final_stats.total_transitions);
        printf("Maximum queue depth: %u\n", (unsigned)final_stats.max_queue_depth);
        printf("Test data - Processed: %d, Errors: %d\n", 
               test_data.processed_count, test_data.error_count);
    }
    
    /* Calculate total events sent */
    int total_events_sent = 0;
    for (int i = 0; i < NUM_PRODUCER_THREADS; i++) {
        total_events_sent += thread_data[i].events_sent;
    }
    printf("Total events sent by producers: %d\n", total_events_sent);
    
    /* Cleanup */
    SM_RTT_Stop(&rtt_sm);
    SM_RTT_Deinit(&rtt_sm);
    pthread_mutex_destroy(&test_data.data_mutex);
    
    printf("\nStress test completed successfully!\n");
    return 0;
}