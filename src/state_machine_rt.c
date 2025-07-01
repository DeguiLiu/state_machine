/**
 * @file state_machine_rtt.c
 * @brief RTOS wrapper implementation.
 *
 * This module implements thread-safe extensions to the state machine framework. 
 */

#include "state_machine_rt.h"

/* --- Private Helper Function Declarations --- */
static void worker_entry(void *parameter);
static SM_RT_Result dispatch_event(SM_RT_Instance *rt_sm, const SM_Event *event);
static void perform_transition(SM_StateMachine *sm, const SM_State *target_state, const SM_Event *event);
static uint8_t get_state_depth(const SM_State *state);
static const SM_State *find_lca(const SM_State *s1, const SM_State *s2);

/* --- Public API Implementation --- */

SM_RT_Result SM_RT_Init(SM_RT_Instance *rt_sm,
                          const SM_State *initial_state,
                          const SM_State **entry_path_buffer,
                          uint8_t buffer_size,
                          void *user_data,
                          SM_ActionFn unhandled_hook)
{
    SM_RT_Result result = SM_RT_RESULT_ERROR_UNKNOWN;
    bool validation_passed = false;
    
    /* Initialize all local variables */
    do {
        /* Parameter validation */
        if (rt_sm == NULL) {
            result = SM_RT_RESULT_ERROR_NULL_PTR;
            break;
        }
        
        if (initial_state == NULL) {
            result = SM_RT_RESULT_ERROR_NULL_PTR;
            break;
        }
        
        if (entry_path_buffer == NULL) {
            result = SM_RT_RESULT_ERROR_NULL_PTR;
            break;
        }
        
        if (buffer_size == 0U) {
            result = SM_RT_RESULT_ERROR_INVALID;
            break;
        }
        
        if (rt_sm->is_initialized) {
            result = SM_RT_RESULT_ERROR_ALREADY_INIT;
            break;
        }
        
        validation_passed = true;
        
    } while (false);
    
    if (validation_passed) {
        /* Initialize the base state machine */
        SM_Init(&rt_sm->base_sm, initial_state, entry_path_buffer, 
                buffer_size, user_data, unhandled_hook);
        
        /* Initialize RTT-specific fields */
        rt_sm->stats.total_events_processed = 0U;
        rt_sm->stats.total_events_unhandled = 0U;
        rt_sm->stats.total_transitions = 0U;
        rt_sm->stats.current_queue_depth = 0U;
        rt_sm->stats.max_queue_depth = 0U;
        rt_sm->is_initialized = true;
        rt_sm->is_started = false;
        rt_sm->worker_thread = NULL;
        rt_sm->event_queue = NULL;
        rt_sm->mutex = NULL;
        
        result = SM_RT_RESULT_SUCCESS;
    }
    
    return result;
}

SM_RT_Result SM_RT_Start(SM_RT_Instance *rt_sm)
{
    SM_RT_Result result = SM_RT_RESULT_ERROR_UNKNOWN;
    bool validation_passed = false;
    
    do {
        /* Parameter validation */
        if (rt_sm == NULL) {
            result = SM_RT_RESULT_ERROR_NULL_PTR;
            break;
        }
        
        if (!rt_sm->is_initialized) {
            result = SM_RT_RESULT_ERROR_NOT_INIT;
            break;
        }
        
        if (rt_sm->is_started) {
            result = SM_RT_RESULT_ERROR_ALREADY_STARTED;
            break;
        }
        
        validation_passed = true;
        
    } while (false);
    
    if (validation_passed) {
        /* Note: RTOS specific implementation would create thread and queue here */
        /* For now, we simulate the start operation */
        rt_sm->is_started = true;
        result = SM_RT_RESULT_SUCCESS;
    }
    
    return result;
}

SM_RT_Result SM_RT_Stop(SM_RT_Instance *rt_sm)
{
    SM_RT_Result result = SM_RT_RESULT_ERROR_UNKNOWN;
    bool validation_passed = false;
    
    do {
        /* Parameter validation */
        if (rt_sm == NULL) {
            result = SM_RT_RESULT_ERROR_NULL_PTR;
            break;
        }
        
        if (!rt_sm->is_initialized) {
            result = SM_RT_RESULT_ERROR_NOT_INIT;
            break;
        }
        
        if (!rt_sm->is_started) {
            result = SM_RT_RESULT_ERROR_NOT_STARTED;
            break;
        }
        
        validation_passed = true;
        
    } while (false);
    
    if (validation_passed) {
        /* Note: RTOS specific implementation would stop thread and cleanup here */
        rt_sm->is_started = false;
        rt_sm->worker_thread = NULL;
        rt_sm->event_queue = NULL;
        rt_sm->mutex = NULL;
        result = SM_RT_RESULT_SUCCESS;
    }
    
    return result;
}

SM_RT_Result SM_RT_PostEvent(SM_RT_Instance *rt_sm, const SM_Event *event)
{
    SM_RT_Result result = SM_RT_RESULT_ERROR_UNKNOWN;
    bool validation_passed = false;
    
    do {
        /* Parameter validation */
        if (rt_sm == NULL) {
            result = SM_RT_RESULT_ERROR_NULL_PTR;
            break;
        }
        
        if (event == NULL) {
            result = SM_RT_RESULT_ERROR_NULL_PTR;
            break;
        }
        
        if (!rt_sm->is_initialized) {
            result = SM_RT_RESULT_ERROR_NOT_INIT;
            break;
        }
        
        if (!rt_sm->is_started) {
            result = SM_RT_RESULT_ERROR_NOT_STARTED;
            break;
        }
        
        validation_passed = true;
        
    } while (false);
    
    if (validation_passed) {
        /* For now, directly dispatch the event (in real RTOS implementation, 
           this would post to a message queue) */
        result = dispatch_event(rt_sm, event);
    }
    
    return result;
}

SM_RT_Result SM_RT_PostEventId(SM_RT_Instance *rt_sm, uint32_t event_id, void *context)
{
    SM_RT_Result result = SM_RT_RESULT_ERROR_UNKNOWN;
    bool validation_passed = false;
    SM_Event event_to_post = {0U, NULL};
    
    do {
        /* Parameter validation */
        if (rt_sm == NULL) {
            result = SM_RT_RESULT_ERROR_NULL_PTR;
            break;
        }
        
        if (!rt_sm->is_initialized) {
            result = SM_RT_RESULT_ERROR_NOT_INIT;
            break;
        }
        
        if (!rt_sm->is_started) {
            result = SM_RT_RESULT_ERROR_NOT_STARTED;
            break;
        }
        
        validation_passed = true;
        
    } while (false);
    
    if (validation_passed) {
        /* Initialize event structure */
        event_to_post.id = event_id;
        event_to_post.context = context;
        
        /* Post the event */
        result = SM_RT_PostEvent(rt_sm, &event_to_post);
    }
    
    return result;
}

SM_RT_Result SM_RT_Reset(SM_RT_Instance *rt_sm)
{
    SM_RT_Result result = SM_RT_RESULT_ERROR_UNKNOWN;
    bool validation_passed = false;
    
    do {
        /* Parameter validation */
        if (rt_sm == NULL) {
            result = SM_RT_RESULT_ERROR_NULL_PTR;
            break;
        }
        
        if (!rt_sm->is_initialized) {
            result = SM_RT_RESULT_ERROR_NOT_INIT;
            break;
        }
        
        validation_passed = true;
        
    } while (false);
    
    if (validation_passed) {
        /* Reset the base state machine */
        SM_Reset(&rt_sm->base_sm);
        rt_sm->stats.total_transitions++;
        result = SM_RT_RESULT_SUCCESS;
    }
    
    return result;
}

SM_RT_Result SM_RT_IsInState(const SM_RT_Instance *rt_sm, 
                               const SM_State *state, 
                               bool *is_in_state)
{
    SM_RT_Result result = SM_RT_RESULT_ERROR_UNKNOWN;
    bool validation_passed = false;
    
    do {
        /* Parameter validation */
        if (rt_sm == NULL) {
            result = SM_RT_RESULT_ERROR_NULL_PTR;
            break;
        }
        
        if (state == NULL) {
            result = SM_RT_RESULT_ERROR_NULL_PTR;
            break;
        }
        
        if (is_in_state == NULL) {
            result = SM_RT_RESULT_ERROR_NULL_PTR;
            break;
        }
        
        if (!rt_sm->is_initialized) {
            result = SM_RT_RESULT_ERROR_NOT_INIT;
            break;
        }
        
        validation_passed = true;
        
    } while (false);
    
    if (validation_passed) {
        /* Use the base state machine function */
        *is_in_state = SM_IsInState(&rt_sm->base_sm, state);
        result = SM_RT_RESULT_SUCCESS;
    } else {
        /* Ensure output parameter is set even on error */
        if (is_in_state != NULL) {
            *is_in_state = false;
        }
    }
    
    return result;
}

SM_RT_Result SM_RT_GetCurrentStateName(const SM_RT_Instance *rt_sm, 
                                         const char **state_name)
{
    SM_RT_Result result = SM_RT_RESULT_ERROR_UNKNOWN;
    bool validation_passed = false;
    
    do {
        /* Parameter validation */
        if (rt_sm == NULL) {
            result = SM_RT_RESULT_ERROR_NULL_PTR;
            break;
        }
        
        if (state_name == NULL) {
            result = SM_RT_RESULT_ERROR_NULL_PTR;
            break;
        }
        
        if (!rt_sm->is_initialized) {
            result = SM_RT_RESULT_ERROR_NOT_INIT;
            break;
        }
        
        validation_passed = true;
        
    } while (false);
    
    if (validation_passed) {
        /* Use the base state machine function */
        *state_name = SM_GetCurrentStateName(&rt_sm->base_sm);
        result = SM_RT_RESULT_SUCCESS;
    } else {
        /* Ensure output parameter is set even on error */
        if (state_name != NULL) {
            *state_name = "Unknown";
        }
    }
    
    return result;
}

SM_RT_Result SM_RT_GetStatistics(const SM_RT_Instance *rt_sm, 
                                   SM_RT_Statistics *stats)
{
    SM_RT_Result result = SM_RT_RESULT_ERROR_UNKNOWN;
    bool validation_passed = false;
    
    do {
        /* Parameter validation */
        if (rt_sm == NULL) {
            result = SM_RT_RESULT_ERROR_NULL_PTR;
            break;
        }
        
        if (stats == NULL) {
            result = SM_RT_RESULT_ERROR_NULL_PTR;
            break;
        }
        
        if (!rt_sm->is_initialized) {
            result = SM_RT_RESULT_ERROR_NOT_INIT;
            break;
        }
        
        validation_passed = true;
        
    } while (false);
    
    if (validation_passed) {
        /* Copy statistics structure */
        stats->total_events_processed = rt_sm->stats.total_events_processed;
        stats->total_events_unhandled = rt_sm->stats.total_events_unhandled;
        stats->total_transitions = rt_sm->stats.total_transitions;
        stats->current_queue_depth = rt_sm->stats.current_queue_depth;
        stats->max_queue_depth = rt_sm->stats.max_queue_depth;
        result = SM_RT_RESULT_SUCCESS;
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

SM_RT_Result SM_RT_ResetStatistics(SM_RT_Instance *rt_sm)
{
    SM_RT_Result result = SM_RT_RESULT_ERROR_UNKNOWN;
    bool validation_passed = false;
    
    do {
        /* Parameter validation */
        if (rt_sm == NULL) {
            result = SM_RT_RESULT_ERROR_NULL_PTR;
            break;
        }
        
        if (!rt_sm->is_initialized) {
            result = SM_RT_RESULT_ERROR_NOT_INIT;
            break;
        }
        
        validation_passed = true;
        
    } while (false);
    
    if (validation_passed) {
        /* Reset all statistics */
        rt_sm->stats.total_events_processed = 0U;
        rt_sm->stats.total_events_unhandled = 0U;
        rt_sm->stats.total_transitions = 0U;
        rt_sm->stats.current_queue_depth = 0U;
        rt_sm->stats.max_queue_depth = 0U;
        result = SM_RT_RESULT_SUCCESS;
    }
    
    return result;
}

/* --- Private Helper Function Implementations --- */

static void worker_entry(void *parameter)
{
    SM_RT_Instance *rt_sm = NULL;
    bool valid_parameter = false;
    
    /* Initialize local variables */
    rt_sm = (SM_RT_Instance *)parameter;
    
    /* Parameter validation */
    if (rt_sm != NULL) {
        valid_parameter = true;
    }
    
    if (valid_parameter) {
        /* Note: In a real RTOS implementation, this would be an infinite loop
           waiting for events from the message queue and processing them */
        /* For now, this is a placeholder implementation */
    }
    
    /* Single return point */
    return;
}

static SM_RT_Result dispatch_event(SM_RT_Instance *rt_sm, const SM_Event *event)
{
    SM_RT_Result result = SM_RT_RESULT_ERROR_UNKNOWN;
    bool validation_passed = false;
    bool event_handled = false;
    
    do {
        /* Parameter validation */
        if (rt_sm == NULL) {
            result = SM_RT_RESULT_ERROR_NULL_PTR;
            break;
        }
        
        if (event == NULL) {
            result = SM_RT_RESULT_ERROR_NULL_PTR;
            break;
        }
        
        validation_passed = true;
        
    } while (false);
    
    if (validation_passed) {
        /* Dispatch event to base state machine */
        event_handled = SM_Dispatch(&rt_sm->base_sm, event);
        
        /* Update statistics */
        rt_sm->stats.total_events_processed++;
        if (!event_handled) {
            rt_sm->stats.total_events_unhandled++;
        }
        
        result = SM_RT_RESULT_SUCCESS;
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