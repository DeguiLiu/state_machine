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
 * @brief Result codes for RTT state machine operations.
 */
typedef enum {
    sm_rtt_result_success = 0,    /**< Operation completed successfully. */
    sm_rtt_result_error_null_ptr, /**< Null pointer provided as parameter. */
    sm_rtt_result_error_invalid,  /**< Invalid parameter value. */
    sm_rtt_result_error_not_init, /**< State machine not initialized. */
    sm_rtt_result_error_already_init, /**< State machine already initialized. */
    sm_rtt_result_error_not_started,  /**< State machine not started. */
    sm_rtt_result_error_already_started, /**< State machine already started. */
    sm_rtt_result_error_queue_full,   /**< Event queue is full. */
    sm_rtt_result_error_unknown   /**< Unknown error occurred. */
} Sm_Rtt_Result;

/**
 * @brief State machine statistics.
 */
typedef struct {
    uint32_t total_events_processed;    /**< Total number of events processed. */
    uint32_t total_events_unhandled;    /**< Total number of unhandled events. */
    uint32_t total_transitions;         /**< Total number of state transitions. */
    uint32_t current_queue_depth;       /**< Current depth of event queue. */
    uint32_t max_queue_depth;          /**< Maximum depth reached by event queue. */
} Sm_Rtt_Statistics;

/**
 * @brief RTT state machine instance with thread-safe operations.
 */
typedef struct {
    SM_StateMachine     base_sm;        /**< Base state machine instance. */
    Sm_Rtt_Statistics   stats;          /**< Usage statistics. */
    bool                is_initialized; /**< Initialization status flag. */
    bool                is_started;     /**< Started status flag. */
    void               *worker_thread;  /**< RT-Thread worker thread handle. */
    void               *event_queue;    /**< RT-Thread message queue handle. */
    void               *mutex;          /**< RT-Thread mutex for thread safety. */
} Sm_Rtt_Instance;

/* --- Public API --- */

/**
 * @brief Initializes an RTT state machine instance.
 * @param[out] rtt_sm           Pointer to the RTT state machine instance.
 * @param[in]  initial_state    Pointer to the initial state.
 * @param[in]  entry_path_buffer User-provided buffer for transition calculations.
 * @param[in]  buffer_size      Size of the entry path buffer.
 * @param[in]  user_data        Optional pointer to user data.
 * @param[in]  unhandled_hook   Optional hook for unhandled events.
 * @return sm_rtt_result_success on success, error code otherwise.
 */
Sm_Rtt_Result sm_rtt_init(Sm_Rtt_Instance *rtt_sm,
                          const SM_State *initial_state,
                          const SM_State **entry_path_buffer,
                          uint8_t buffer_size,
                          void *user_data,
                          SM_ActionFn unhandled_hook);

/**
 * @brief Starts the RTT state machine worker thread.
 * @param[in,out] rtt_sm Pointer to the RTT state machine instance.
 * @return sm_rtt_result_success on success, error code otherwise.
 */
Sm_Rtt_Result sm_rtt_start(Sm_Rtt_Instance *rtt_sm);

/**
 * @brief Stops the RTT state machine worker thread.
 * @param[in,out] rtt_sm Pointer to the RTT state machine instance.
 * @return sm_rtt_result_success on success, error code otherwise.
 */
Sm_Rtt_Result sm_rtt_stop(Sm_Rtt_Instance *rtt_sm);

/**
 * @brief Posts an event to the RTT state machine queue.
 * @param[in,out] rtt_sm Pointer to the RTT state machine instance.
 * @param[in]     event  Pointer to the event to post.
 * @return sm_rtt_result_success on success, error code otherwise.
 */
Sm_Rtt_Result sm_rtt_post_event(Sm_Rtt_Instance *rtt_sm, const SM_Event *event);

/**
 * @brief Posts an event with ID to the RTT state machine queue.
 * @param[in,out] rtt_sm    Pointer to the RTT state machine instance.
 * @param[in]     event_id  Event identifier.
 * @param[in]     context   Optional event context data.
 * @return sm_rtt_result_success on success, error code otherwise.
 */
Sm_Rtt_Result sm_rtt_post_event_id(Sm_Rtt_Instance *rtt_sm, uint32_t event_id, void *context);

/**
 * @brief Resets the RTT state machine to its initial state.
 * @param[in,out] rtt_sm Pointer to the RTT state machine instance.
 * @return sm_rtt_result_success on success, error code otherwise.
 */
Sm_Rtt_Result sm_rtt_reset(Sm_Rtt_Instance *rtt_sm);

/**
 * @brief Checks if the current state is a specific state or substate.
 * @param[in]  rtt_sm      Pointer to the RTT state machine instance.
 * @param[in]  state       The state to check against.
 * @param[out] is_in_state Pointer to store the result.
 * @return sm_rtt_result_success on success, error code otherwise.
 */
Sm_Rtt_Result sm_rtt_is_in_state(const Sm_Rtt_Instance *rtt_sm, 
                                 const SM_State *state, 
                                 bool *is_in_state);

/**
 * @brief Gets the name of the current state.
 * @param[in]  rtt_sm     Pointer to the RTT state machine instance.
 * @param[out] state_name Pointer to store the state name pointer.
 * @return sm_rtt_result_success on success, error code otherwise.
 */
Sm_Rtt_Result sm_rtt_get_current_state_name(const Sm_Rtt_Instance *rtt_sm, 
                                            const char **state_name);

/**
 * @brief Gets the current statistics of the RTT state machine.
 * @param[in]  rtt_sm Pointer to the RTT state machine instance.
 * @param[out] stats  Pointer to store the statistics.
 * @return sm_rtt_result_success on success, error code otherwise.
 */
Sm_Rtt_Result sm_rtt_get_statistics(const Sm_Rtt_Instance *rtt_sm, 
                                    Sm_Rtt_Statistics *stats);

/**
 * @brief Resets the statistics of the RTT state machine.
 * @param[in,out] rtt_sm Pointer to the RTT state machine instance.
 * @return sm_rtt_result_success on success, error code otherwise.
 */
Sm_Rtt_Result sm_rtt_reset_statistics(Sm_Rtt_Instance *rtt_sm);

#endif /* STATE_MACHINE_RTT_H */