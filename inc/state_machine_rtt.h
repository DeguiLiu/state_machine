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
 * @file state_machine_rtt.h
 * @brief MISRA-C:2012 compliant RT-Thread wrapper for the hierarchical state machine framework.
 *
 * This module provides RT-Thread specific extensions to the state machine framework,
 * including thread-safe operations, statistics collection, and RT-Thread integration.
 * All functions comply with MISRA-C:2012 requirements including single return points
 * and proper variable initialization.
 */

#ifndef STATE_MACHINE_RTT_H
#define STATE_MACHINE_RTT_H

#include "state_machine.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* --- RTT-specific Types --- */

/**
 * @brief RT-Thread state machine configuration.
 */
typedef struct {
    uint32_t queue_size;         /**< Maximum number of events in queue. */
    uint32_t thread_stack_size;  /**< Worker thread stack size in bytes. */
    uint8_t  thread_priority;    /**< Worker thread priority. */
    uint32_t thread_timeslice;   /**< Worker thread time slice. */
    const char* thread_name;     /**< Worker thread name. */
    const char* queue_name;      /**< Message queue name. */
    const char* mutex_name;      /**< Mutex name. */
} SM_RTT_Config;

/**
 * @brief Result codes for RTT state machine operations.
 */
typedef enum {
    SM_RTT_RESULT_SUCCESS = 0,    /**< Operation completed successfully. */
    SM_RTT_RESULT_ERROR_NULL_PTR, /**< Null pointer provided as parameter. */
    SM_RTT_RESULT_ERROR_INVALID,  /**< Invalid parameter value. */
    SM_RTT_RESULT_ERROR_NOT_INIT, /**< State machine not initialized. */
    SM_RTT_RESULT_ERROR_ALREADY_INIT, /**< State machine already initialized. */
    SM_RTT_RESULT_ERROR_NOT_STARTED,  /**< State machine not started. */
    SM_RTT_RESULT_ERROR_ALREADY_STARTED, /**< State machine already started. */
    SM_RTT_RESULT_ERROR_QUEUE_FULL,   /**< Event queue is full. */
    SM_RTT_RESULT_ERROR_UNKNOWN   /**< Unknown error occurred. */
} SM_RTT_Result;

/**
 * @brief State machine statistics.
 */
typedef struct {
    uint32_t total_events_processed;    /**< Total number of events processed. */
    uint32_t total_events_unhandled;    /**< Total number of unhandled events. */
    uint32_t total_transitions;         /**< Total number of state transitions. */
    uint32_t current_queue_depth;       /**< Current depth of event queue. */
    uint32_t max_queue_depth;          /**< Maximum depth reached by event queue. */
} SM_RTT_Statistics;

/**
 * @brief RTT state machine instance with thread-safe operations.
 */
typedef struct {
    SM_StateMachine     base_sm;        /**< Base state machine instance. */
    SM_RTT_Statistics   stats;          /**< Usage statistics. */
    SM_RTT_Config       config;         /**< Configuration settings. */
    bool                is_initialized; /**< Initialization status flag. */
    bool                is_started;     /**< Started status flag. */
    bool                stop_requested; /**< Stop request flag for worker thread. */
    void               *worker_thread;  /**< RT-Thread worker thread handle. */
    void               *event_queue;    /**< RT-Thread message queue handle. */
    void               *mutex;          /**< RT-Thread mutex for thread safety. */
} SM_RTT_Instance;

/* --- Public API --- */

/**
 * @brief Initializes an RTT state machine instance.
 * @param[out] rtt_sm           Pointer to the RTT state machine instance.
 * @param[in]  config           Pointer to the configuration settings.
 * @param[in]  initial_state    Pointer to the initial state.
 * @param[in]  entry_path_buffer User-provided buffer for transition calculations.
 * @param[in]  buffer_size      Size of the entry path buffer.
 * @param[in]  user_data        Optional pointer to user data.
 * @param[in]  unhandled_hook   Optional hook for unhandled events.
 * @return SM_RTT_RESULT_SUCCESS on success, error code otherwise.
 */
SM_RTT_Result SM_RTT_Init(SM_RTT_Instance *rtt_sm,
                          const SM_RTT_Config *config,
                          const SM_State *initial_state,
                          const SM_State **entry_path_buffer,
                          uint8_t buffer_size,
                          void *user_data,
                          SM_ActionFn unhandled_hook);

/**
 * @brief Deinitializes an RTT state machine instance.
 * @param[in,out] rtt_sm Pointer to the RTT state machine instance.
 * @return SM_RTT_RESULT_SUCCESS on success, error code otherwise.
 */
SM_RTT_Result SM_RTT_Deinit(SM_RTT_Instance *rtt_sm);

/**
 * @brief Starts the RTT state machine worker thread.
 * @param[in,out] rtt_sm Pointer to the RTT state machine instance.
 * @return SM_RTT_RESULT_SUCCESS on success, error code otherwise.
 */
SM_RTT_Result SM_RTT_Start(SM_RTT_Instance *rtt_sm);

/**
 * @brief Stops the RTT state machine worker thread.
 * @param[in,out] rtt_sm Pointer to the RTT state machine instance.
 * @return SM_RTT_RESULT_SUCCESS on success, error code otherwise.
 */
SM_RTT_Result SM_RTT_Stop(SM_RTT_Instance *rtt_sm);

/**
 * @brief Dispatches an event synchronously (directly in calling thread).
 * @param[in,out] rtt_sm Pointer to the RTT state machine instance.
 * @param[in]     event  Pointer to the event to dispatch.
 * @return SM_RTT_RESULT_SUCCESS on success, error code otherwise.
 */
SM_RTT_Result SM_RTT_DispatchSync(SM_RTT_Instance *rtt_sm, const SM_Event *event);

/**
 * @brief Posts an event to the RTT state machine queue.
 * @param[in,out] rtt_sm Pointer to the RTT state machine instance.
 * @param[in]     event  Pointer to the event to post.
 * @return SM_RTT_RESULT_SUCCESS on success, error code otherwise.
 */
SM_RTT_Result SM_RTT_PostEvent(SM_RTT_Instance *rtt_sm, const SM_Event *event);

/**
 * @brief Posts an event with ID to the RTT state machine queue.
 * @param[in,out] rtt_sm    Pointer to the RTT state machine instance.
 * @param[in]     event_id  Event identifier.
 * @param[in]     context   Optional event context data.
 * @return SM_RTT_RESULT_SUCCESS on success, error code otherwise.
 */
SM_RTT_Result SM_RTT_PostEventId(SM_RTT_Instance *rtt_sm, uint32_t event_id, void *context);

/**
 * @brief Resets the RTT state machine to its initial state.
 * @param[in,out] rtt_sm Pointer to the RTT state machine instance.
 * @return SM_RTT_RESULT_SUCCESS on success, error code otherwise.
 */
SM_RTT_Result SM_RTT_Reset(SM_RTT_Instance *rtt_sm);

/**
 * @brief Checks if the current state is a specific state or substate.
 * @param[in]  rtt_sm      Pointer to the RTT state machine instance.
 * @param[in]  state       The state to check against.
 * @param[out] is_in_state Pointer to store the result.
 * @return SM_RTT_RESULT_SUCCESS on success, error code otherwise.
 */
SM_RTT_Result SM_RTT_IsInState(const SM_RTT_Instance *rtt_sm, 
                               const SM_State *state, 
                               bool *is_in_state);

/**
 * @brief Gets the name of the current state.
 * @param[in]  rtt_sm     Pointer to the RTT state machine instance.
 * @param[out] state_name Pointer to store the state name pointer.
 * @return SM_RTT_RESULT_SUCCESS on success, error code otherwise.
 */
SM_RTT_Result SM_RTT_GetCurrentStateName(const SM_RTT_Instance *rtt_sm, 
                                         const char **state_name);

/**
 * @brief Gets the current statistics of the RTT state machine.
 * @param[in]  rtt_sm Pointer to the RTT state machine instance.
 * @param[out] stats  Pointer to store the statistics.
 * @return SM_RTT_RESULT_SUCCESS on success, error code otherwise.
 */
SM_RTT_Result SM_RTT_GetStatistics(const SM_RTT_Instance *rtt_sm, 
                                   SM_RTT_Statistics *stats);

/**
 * @brief Resets the statistics of the RTT state machine.
 * @param[in,out] rtt_sm Pointer to the RTT state machine instance.
 * @return SM_RTT_RESULT_SUCCESS on success, error code otherwise.
 */
SM_RTT_Result SM_RTT_ResetStatistics(SM_RTT_Instance *rtt_sm);

#endif /* STATE_MACHINE_RTT_H */