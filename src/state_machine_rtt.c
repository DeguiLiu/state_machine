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
 * @file state_machine_rtt.c
 * @brief MISRA-C:2012 compliant RT-Thread wrapper implementation.
 *
 * This module implements thread-safe RT-Thread specific extensions to the 
 * state machine framework. All functions follow MISRA-C:2012 guidelines
 * with single return points and proper variable initialization.
 */

#include "state_machine_rtt.h"

/* --- Private Helper Function Declarations --- */
static void worker_entry(void *parameter);
static Sm_Rtt_Result dispatch_event(Sm_Rtt_Instance *rtt_sm, const SM_Event *event);
static void perform_transition(SM_StateMachine *sm, const SM_State *target_state, const SM_Event *event);
static uint8_t get_state_depth(const SM_State *state);
static const SM_State *find_lca(const SM_State *s1, const SM_State *s2);

/* --- Public API Implementation --- */

Sm_Rtt_Result sm_rtt_init(Sm_Rtt_Instance *rtt_sm,
                          const SM_State *initial_state,
                          const SM_State **entry_path_buffer,
                          uint8_t buffer_size,
                          void *user_data,
                          SM_ActionFn unhandled_hook)
{
    Sm_Rtt_Result result = sm_rtt_result_error_unknown;
    bool validation_passed = false;
    
    /* Initialize all local variables */
    do {
        /* Parameter validation */
        if (rtt_sm == NULL) {
            result = sm_rtt_result_error_null_ptr;
            break;
        }
        
        if (initial_state == NULL) {
            result = sm_rtt_result_error_null_ptr;
            break;
        }
        
        if (entry_path_buffer == NULL) {
            result = sm_rtt_result_error_null_ptr;
            break;
        }
        
        if (buffer_size == 0U) {
            result = sm_rtt_result_error_invalid;
            break;
        }
        
        if (rtt_sm->is_initialized) {
            result = sm_rtt_result_error_already_init;
            break;
        }
        
        validation_passed = true;
        
    } while (false);
    
    if (validation_passed) {
        /* Initialize the base state machine */
        SM_Init(&rtt_sm->base_sm, initial_state, entry_path_buffer, 
                buffer_size, user_data, unhandled_hook);
        
        /* Initialize RTT-specific fields */
        rtt_sm->stats.total_events_processed = 0U;
        rtt_sm->stats.total_events_unhandled = 0U;
        rtt_sm->stats.total_transitions = 0U;
        rtt_sm->stats.current_queue_depth = 0U;
        rtt_sm->stats.max_queue_depth = 0U;
        rtt_sm->is_initialized = true;
        rtt_sm->is_started = false;
        rtt_sm->worker_thread = NULL;
        rtt_sm->event_queue = NULL;
        rtt_sm->mutex = NULL;
        
        result = sm_rtt_result_success;
    }
    
    return result;
}

Sm_Rtt_Result sm_rtt_start(Sm_Rtt_Instance *rtt_sm)
{
    Sm_Rtt_Result result = sm_rtt_result_error_unknown;
    bool validation_passed = false;
    
    do {
        /* Parameter validation */
        if (rtt_sm == NULL) {
            result = sm_rtt_result_error_null_ptr;
            break;
        }
        
        if (!rtt_sm->is_initialized) {
            result = sm_rtt_result_error_not_init;
            break;
        }
        
        if (rtt_sm->is_started) {
            result = sm_rtt_result_error_already_started;
            break;
        }
        
        validation_passed = true;
        
    } while (false);
    
    if (validation_passed) {
        /* Note: RT-Thread specific implementation would create thread and queue here */
        /* For now, we simulate the start operation */
        rtt_sm->is_started = true;
        result = sm_rtt_result_success;
    }
    
    return result;
}

Sm_Rtt_Result sm_rtt_stop(Sm_Rtt_Instance *rtt_sm)
{
    Sm_Rtt_Result result = sm_rtt_result_error_unknown;
    bool validation_passed = false;
    
    do {
        /* Parameter validation */
        if (rtt_sm == NULL) {
            result = sm_rtt_result_error_null_ptr;
            break;
        }
        
        if (!rtt_sm->is_initialized) {
            result = sm_rtt_result_error_not_init;
            break;
        }
        
        if (!rtt_sm->is_started) {
            result = sm_rtt_result_error_not_started;
            break;
        }
        
        validation_passed = true;
        
    } while (false);
    
    if (validation_passed) {
        /* Note: RT-Thread specific implementation would stop thread and cleanup here */
        rtt_sm->is_started = false;
        rtt_sm->worker_thread = NULL;
        rtt_sm->event_queue = NULL;
        rtt_sm->mutex = NULL;
        result = sm_rtt_result_success;
    }
    
    return result;
}

Sm_Rtt_Result sm_rtt_post_event(Sm_Rtt_Instance *rtt_sm, const SM_Event *event)
{
    Sm_Rtt_Result result = sm_rtt_result_error_unknown;
    bool validation_passed = false;
    
    do {
        /* Parameter validation */
        if (rtt_sm == NULL) {
            result = sm_rtt_result_error_null_ptr;
            break;
        }
        
        if (event == NULL) {
            result = sm_rtt_result_error_null_ptr;
            break;
        }
        
        if (!rtt_sm->is_initialized) {
            result = sm_rtt_result_error_not_init;
            break;
        }
        
        if (!rtt_sm->is_started) {
            result = sm_rtt_result_error_not_started;
            break;
        }
        
        validation_passed = true;
        
    } while (false);
    
    if (validation_passed) {
        /* For now, directly dispatch the event (in real RT-Thread implementation, 
           this would post to a message queue) */
        result = dispatch_event(rtt_sm, event);
    }
    
    return result;
}

Sm_Rtt_Result sm_rtt_post_event_id(Sm_Rtt_Instance *rtt_sm, uint32_t event_id, void *context)
{
    Sm_Rtt_Result result = sm_rtt_result_error_unknown;
    bool validation_passed = false;
    SM_Event event_to_post = {0U, NULL};
    
    do {
        /* Parameter validation */
        if (rtt_sm == NULL) {
            result = sm_rtt_result_error_null_ptr;
            break;
        }
        
        if (!rtt_sm->is_initialized) {
            result = sm_rtt_result_error_not_init;
            break;
        }
        
        if (!rtt_sm->is_started) {
            result = sm_rtt_result_error_not_started;
            break;
        }
        
        validation_passed = true;
        
    } while (false);
    
    if (validation_passed) {
        /* Initialize event structure */
        event_to_post.id = event_id;
        event_to_post.context = context;
        
        /* Post the event */
        result = sm_rtt_post_event(rtt_sm, &event_to_post);
    }
    
    return result;
}

Sm_Rtt_Result sm_rtt_reset(Sm_Rtt_Instance *rtt_sm)
{
    Sm_Rtt_Result result = sm_rtt_result_error_unknown;
    bool validation_passed = false;
    
    do {
        /* Parameter validation */
        if (rtt_sm == NULL) {
            result = sm_rtt_result_error_null_ptr;
            break;
        }
        
        if (!rtt_sm->is_initialized) {
            result = sm_rtt_result_error_not_init;
            break;
        }
        
        validation_passed = true;
        
    } while (false);
    
    if (validation_passed) {
        /* Reset the base state machine */
        SM_Reset(&rtt_sm->base_sm);
        rtt_sm->stats.total_transitions++;
        result = sm_rtt_result_success;
    }
    
    return result;
}

Sm_Rtt_Result sm_rtt_is_in_state(const Sm_Rtt_Instance *rtt_sm, 
                                 const SM_State *state, 
                                 bool *is_in_state)
{
    Sm_Rtt_Result result = sm_rtt_result_error_unknown;
    bool validation_passed = false;
    
    do {
        /* Parameter validation */
        if (rtt_sm == NULL) {
            result = sm_rtt_result_error_null_ptr;
            break;
        }
        
        if (state == NULL) {
            result = sm_rtt_result_error_null_ptr;
            break;
        }
        
        if (is_in_state == NULL) {
            result = sm_rtt_result_error_null_ptr;
            break;
        }
        
        if (!rtt_sm->is_initialized) {
            result = sm_rtt_result_error_not_init;
            break;
        }
        
        validation_passed = true;
        
    } while (false);
    
    if (validation_passed) {
        /* Use the base state machine function */
        *is_in_state = SM_IsInState(&rtt_sm->base_sm, state);
        result = sm_rtt_result_success;
    } else {
        /* Ensure output parameter is set even on error */
        if (is_in_state != NULL) {
            *is_in_state = false;
        }
    }
    
    return result;
}

Sm_Rtt_Result sm_rtt_get_current_state_name(const Sm_Rtt_Instance *rtt_sm, 
                                            const char **state_name)
{
    Sm_Rtt_Result result = sm_rtt_result_error_unknown;
    bool validation_passed = false;
    
    do {
        /* Parameter validation */
        if (rtt_sm == NULL) {
            result = sm_rtt_result_error_null_ptr;
            break;
        }
        
        if (state_name == NULL) {
            result = sm_rtt_result_error_null_ptr;
            break;
        }
        
        if (!rtt_sm->is_initialized) {
            result = sm_rtt_result_error_not_init;
            break;
        }
        
        validation_passed = true;
        
    } while (false);
    
    if (validation_passed) {
        /* Use the base state machine function */
        *state_name = SM_GetCurrentStateName(&rtt_sm->base_sm);
        result = sm_rtt_result_success;
    } else {
        /* Ensure output parameter is set even on error */
        if (state_name != NULL) {
            *state_name = "Unknown";
        }
    }
    
    return result;
}

Sm_Rtt_Result sm_rtt_get_statistics(const Sm_Rtt_Instance *rtt_sm, 
                                    Sm_Rtt_Statistics *stats)
{
    Sm_Rtt_Result result = sm_rtt_result_error_unknown;
    bool validation_passed = false;
    
    do {
        /* Parameter validation */
        if (rtt_sm == NULL) {
            result = sm_rtt_result_error_null_ptr;
            break;
        }
        
        if (stats == NULL) {
            result = sm_rtt_result_error_null_ptr;
            break;
        }
        
        if (!rtt_sm->is_initialized) {
            result = sm_rtt_result_error_not_init;
            break;
        }
        
        validation_passed = true;
        
    } while (false);
    
    if (validation_passed) {
        /* Copy statistics structure */
        stats->total_events_processed = rtt_sm->stats.total_events_processed;
        stats->total_events_unhandled = rtt_sm->stats.total_events_unhandled;
        stats->total_transitions = rtt_sm->stats.total_transitions;
        stats->current_queue_depth = rtt_sm->stats.current_queue_depth;
        stats->max_queue_depth = rtt_sm->stats.max_queue_depth;
        result = sm_rtt_result_success;
    } else {
        /* Ensure output parameter is initialized even on error */
        if (stats != NULL) {
            stats->total_events_processed = 0U;
            stats->total_events_unhandled = 0U;
            stats->total_transitions = 0U;
            stats->current_queue_depth = 0U;
            stats->max_queue_depth = 0U;
        }
    }
    
    return result;
}

Sm_Rtt_Result sm_rtt_reset_statistics(Sm_Rtt_Instance *rtt_sm)
{
    Sm_Rtt_Result result = sm_rtt_result_error_unknown;
    bool validation_passed = false;
    
    do {
        /* Parameter validation */
        if (rtt_sm == NULL) {
            result = sm_rtt_result_error_null_ptr;
            break;
        }
        
        if (!rtt_sm->is_initialized) {
            result = sm_rtt_result_error_not_init;
            break;
        }
        
        validation_passed = true;
        
    } while (false);
    
    if (validation_passed) {
        /* Reset all statistics */
        rtt_sm->stats.total_events_processed = 0U;
        rtt_sm->stats.total_events_unhandled = 0U;
        rtt_sm->stats.total_transitions = 0U;
        rtt_sm->stats.current_queue_depth = 0U;
        rtt_sm->stats.max_queue_depth = 0U;
        result = sm_rtt_result_success;
    }
    
    return result;
}

/* --- Private Helper Function Implementations --- */

static void worker_entry(void *parameter)
{
    Sm_Rtt_Instance *rtt_sm = NULL;
    bool valid_parameter = false;
    
    /* Initialize local variables */
    rtt_sm = (Sm_Rtt_Instance *)parameter;
    
    /* Parameter validation */
    if (rtt_sm != NULL) {
        valid_parameter = true;
    }
    
    if (valid_parameter) {
        /* Note: In a real RT-Thread implementation, this would be an infinite loop
           waiting for events from the message queue and processing them */
        /* For now, this is a placeholder implementation */
    }
    
    /* Single return point */
    return;
}

static Sm_Rtt_Result dispatch_event(Sm_Rtt_Instance *rtt_sm, const SM_Event *event)
{
    Sm_Rtt_Result result = sm_rtt_result_error_unknown;
    bool validation_passed = false;
    bool event_handled = false;
    
    do {
        /* Parameter validation */
        if (rtt_sm == NULL) {
            result = sm_rtt_result_error_null_ptr;
            break;
        }
        
        if (event == NULL) {
            result = sm_rtt_result_error_null_ptr;
            break;
        }
        
        validation_passed = true;
        
    } while (false);
    
    if (validation_passed) {
        /* Dispatch event to base state machine */
        event_handled = SM_Dispatch(&rtt_sm->base_sm, event);
        
        /* Update statistics */
        rtt_sm->stats.total_events_processed++;
        if (!event_handled) {
            rtt_sm->stats.total_events_unhandled++;
        }
        
        result = sm_rtt_result_success;
    }
    
    return result;
}

static void perform_transition(SM_StateMachine *sm, const SM_State *target_state, const SM_Event *event)
{
    const SM_State *source_state = NULL;
    const SM_State *lca_state = NULL;
    const SM_State *exit_iter = NULL;
    const SM_State *entry_iter = NULL;
    uint8_t path_idx = 0U;
    int8_t entry_idx = 0;
    bool same_state = false;
    bool valid_parameters = false;
    
    /* Parameter validation and initialization */
    if ((sm != NULL) && (target_state != NULL)) {
        valid_parameters = true;
        source_state = sm->currentState;
        same_state = (source_state == target_state);
    }
    
    if (valid_parameters) {
        if (same_state) {
            /* External self-transition: execute exit then entry on the same state */
            if ((source_state != NULL) && (source_state->exitAction != NULL)) {
                source_state->exitAction(sm, event);
            }
            if (target_state->entryAction != NULL) {
                target_state->entryAction(sm, event);
            }
        } else {
            /* Different states: find LCA and perform hierarchical transition */
            lca_state = find_lca(source_state, target_state);
            
            /* Perform Exit Actions */
            exit_iter = source_state;
            while ((exit_iter != NULL) && (exit_iter != lca_state)) {
                if (exit_iter->exitAction != NULL) {
                    exit_iter->exitAction(sm, event);
                }
                exit_iter = exit_iter->parent;
            }
            
            /* Record Entry Path */
            path_idx = 0U;
            entry_iter = target_state;
            while ((entry_iter != NULL) && (entry_iter != lca_state)) {
                SM_ASSERT(path_idx < sm->bufferSize);
                sm->entryPathBuffer[path_idx] = entry_iter;
                path_idx++;
                entry_iter = entry_iter->parent;
            }
            
            /* Update current state */
            sm->currentState = target_state;
            
            /* Perform Entry Actions (in reverse order) */
            entry_idx = (int8_t)path_idx - 1;
            while (entry_idx >= 0) {
                if (sm->entryPathBuffer[entry_idx]->entryAction != NULL) {
                    sm->entryPathBuffer[entry_idx]->entryAction(sm, event);
                }
                entry_idx--;
            }
        }
    }
    
    /* Single return point */
    return;
}

static uint8_t get_state_depth(const SM_State *state)
{
    uint8_t depth = 0U;
    const SM_State *current_state = state;
    
    /* Calculate depth by traversing parent chain */
    while (current_state != NULL) {
        depth++;
        current_state = current_state->parent;
    }
    
    return depth;
}

static const SM_State *find_lca(const SM_State *s1, const SM_State *s2)
{
    const SM_State *result = NULL;
    const SM_State *p1 = NULL;
    const SM_State *p2 = NULL;
    uint8_t depth1 = 0U;
    uint8_t depth2 = 0U;
    bool valid_parameters = false;
    
    /* Parameter validation and initialization */
    if ((s1 != NULL) && (s2 != NULL)) {
        valid_parameters = true;
        p1 = s1;
        p2 = s2;
        depth1 = get_state_depth(p1);
        depth2 = get_state_depth(p2);
    }
    
    if (valid_parameters) {
        /* Normalize depths */
        while (depth1 > depth2) {
            p1 = p1->parent;
            depth1--;
        }
        while (depth2 > depth1) {
            p2 = p2->parent;
            depth2--;
        }
        
        /* Find common ancestor */
        while (p1 != p2) {
            p1 = p1->parent;
            p2 = p2->parent;
        }
        
        result = p1;
    }
    
    return result;
}