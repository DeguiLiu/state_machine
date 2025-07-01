# RT-Thread State Machine Implementation

## Overview
This implementation provides a complete multi-threaded, thread-safe state machine wrapper for RT-Thread RTOS. It supports both synchronous and asynchronous event processing with comprehensive statistics tracking and error handling.

## Features

### Multi-Thread Safety
- **Mutex Protection**: All state machine operations are protected by RT-Thread mutexes
- **Message Queue**: Thread-safe event queue for asynchronous processing  
- **Worker Thread**: Dedicated thread for processing queued events
- **Resource Management**: Proper initialization and cleanup of RT-Thread resources

### Dual Mode Event Processing
- **Synchronous Mode**: `SM_RTT_DispatchSync()` - Direct processing in calling thread
- **Asynchronous Mode**: `SM_RTT_PostEvent()` - Event queued for worker thread processing
- **Thread-Safe Statistics**: Real-time tracking of events, transitions, and queue status

### Configuration
- Configurable queue size, thread stack size, priority, and timeslice
- Named RT-Thread objects (thread, queue, mutex) for debugging
- Flexible initialization with user data and event hooks

## API Reference

### Initialization and Management
```c
SM_RTT_Result SM_RTT_Init(SM_RTT_Instance *rtt_sm,
                          const SM_RTT_Config *config,
                          const SM_State *initial_state,
                          const SM_State **entry_path_buffer,
                          uint8_t buffer_size,
                          void *user_data,
                          SM_ActionFn unhandled_hook);

SM_RTT_Result SM_RTT_Deinit(SM_RTT_Instance *rtt_sm);
SM_RTT_Result SM_RTT_Start(SM_RTT_Instance *rtt_sm);
SM_RTT_Result SM_RTT_Stop(SM_RTT_Instance *rtt_sm);
```

### Event Processing
```c
SM_RTT_Result SM_RTT_DispatchSync(SM_RTT_Instance *rtt_sm, const SM_Event *event);
SM_RTT_Result SM_RTT_PostEvent(SM_RTT_Instance *rtt_sm, const SM_Event *event);
SM_RTT_Result SM_RTT_PostEventId(SM_RTT_Instance *rtt_sm, uint32_t event_id, void *context);
```

### State Queries and Statistics
```c
SM_RTT_Result SM_RTT_IsInState(const SM_RTT_Instance *rtt_sm, const SM_State *state, bool *is_in_state);
SM_RTT_Result SM_RTT_GetCurrentStateName(const SM_RTT_Instance *rtt_sm, const char **state_name);
SM_RTT_Result SM_RTT_GetStatistics(const SM_RTT_Instance *rtt_sm, SM_RTT_Statistics *stats);
SM_RTT_Result SM_RTT_ResetStatistics(SM_RTT_Instance *rtt_sm);
```

## Configuration Structure
```c
typedef struct {
    uint32_t queue_size;         /**< Maximum number of events in queue */
    uint32_t thread_stack_size;  /**< Worker thread stack size in bytes */
    uint8_t  thread_priority;    /**< Worker thread priority */
    uint32_t thread_timeslice;   /**< Worker thread time slice */
    const char* thread_name;     /**< Worker thread name */
    const char* queue_name;      /**< Message queue name */
    const char* mutex_name;      /**< Mutex name */
} SM_RTT_Config;
```

## Statistics Structure
```c
typedef struct {
    uint32_t total_events_processed;    /**< Total number of events processed */
    uint32_t total_events_unhandled;    /**< Total number of unhandled events */
    uint32_t total_transitions;         /**< Total number of state transitions */
    uint32_t current_queue_depth;       /**< Current depth of event queue */
    uint32_t max_queue_depth;          /**< Maximum depth reached by event queue */
} SM_RTT_Statistics;
```

## Usage Example

### Basic Setup
```c
#include "state_machine_rtt.h"

SM_RTT_Instance rtt_sm;
const SM_State *path_buffer[8];
AppData app_data;

// Configure the state machine
SM_RTT_Config config = {
    .queue_size = 16,
    .thread_stack_size = 2048,
    .thread_priority = 10,
    .thread_timeslice = 20,
    .thread_name = "sm_worker",
    .queue_name = "sm_queue",
    .mutex_name = "sm_mutex"
};

// Initialize
SM_RTT_Init(&rtt_sm, &config, &STATE_Initial, path_buffer, 
            sizeof(path_buffer)/sizeof(path_buffer[0]), 
            &app_data, on_unhandled_event);

// Start worker thread
SM_RTT_Start(&rtt_sm);
```

### Synchronous Event Processing
```c
SM_Event event = {EV_START, NULL};
SM_RTT_DispatchSync(&rtt_sm, &event);
```

### Asynchronous Event Processing
```c
SM_Event event = {EV_PROCESS, &some_data};
SM_RTT_PostEvent(&rtt_sm, &event);

// Or using event ID
SM_RTT_PostEventId(&rtt_sm, EV_COMPLETE, &result_data);
```

### Statistics Monitoring
```c
SM_RTT_Statistics stats;
if (SM_RTT_GetStatistics(&rtt_sm, &stats) == SM_RTT_RESULT_SUCCESS) {
    printf("Events processed: %u, Queue depth: %u\\n", 
           stats.total_events_processed, stats.current_queue_depth);
}
```

### Cleanup
```c
SM_RTT_Stop(&rtt_sm);
SM_RTT_Deinit(&rtt_sm);
```

## Error Handling
All functions return `SM_RTT_Result` codes:
- `SM_RTT_RESULT_SUCCESS` - Operation completed successfully
- `SM_RTT_RESULT_ERROR_NULL_PTR` - Null pointer parameter
- `SM_RTT_RESULT_ERROR_INVALID` - Invalid parameter value
- `SM_RTT_RESULT_ERROR_NOT_INIT` - State machine not initialized
- `SM_RTT_RESULT_ERROR_ALREADY_INIT` - Already initialized
- `SM_RTT_RESULT_ERROR_NOT_STARTED` - Worker thread not started
- `SM_RTT_RESULT_ERROR_ALREADY_STARTED` - Already started
- `SM_RTT_RESULT_ERROR_QUEUE_FULL` - Event queue is full

## Thread Safety
- All state machine operations are protected by mutexes
- Statistics are updated atomically
- Queue operations are inherently thread-safe in RT-Thread
- Multiple threads can safely call `SM_RTT_PostEvent()` and `SM_RTT_DispatchSync()`

## Performance Considerations
- Synchronous events bypass the queue for lower latency
- Asynchronous events allow non-blocking operation but add queue overhead
- Queue size should be sized based on expected event burst rates
- Worker thread priority should be set appropriately for system requirements

## Testing
- Basic functionality tested in `state_machine_rtt_example.c`
- Multi-threading stress tested in `stress_test_rtt.c` 
- Mock RT-Thread implementation allows testing without full RT-Thread environment
- Comprehensive error handling validation included

## Files
- `inc/state_machine_rtt.h` - Main header file
- `src/state_machine_rtt.c` - Implementation
- `examples/state_machine_rtt_example.c` - Comprehensive test example
- `examples/stress_test_rtt.c` - Multi-threading stress test
- `inc/rt_thread_mock.h` - RT-Thread API mock for testing
- `src/rt_thread_mock.c` - Mock implementation