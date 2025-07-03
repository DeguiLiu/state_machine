/**
 * @file state_machine_rtt.c
 * @brief RTOS wrapper implementation - Independent from base state machine.
 *
 * This module implements thread-safe extensions to the state machine framework
 * with complete independence from the base state_machine module. All required
 * state machine functionality is implemented directly within this file,
 * providing clear separation of concerns and eliminating code dependencies.
 */

#include "state_machine_rt.h"

/* --- Private Helper Function Declarations --- */
static void worker_entry(void *parameter);
static SM_RT_Result dispatch_event(SM_RT_Instance *rt_sm, const SM_Event *event);
static void perform_transition(SM_StateMachine *sm, const SM_State *target_state, const SM_Event *event);
static uint8_t get_state_depth(const SM_State *state);
static const SM_State *find_lca(const SM_State *s1, const SM_State *s2);

/* --- Independent Base State Machine Functions --- */
static void sm_rt_init_base(SM_StateMachine *sm, const SM_State *initialState, 
                           const SM_State **entryPathBuffer, uint8_t bufferSize, 
                           void *userData, SM_ActionFn unhandledHook);
static void sm_rt_reset_base(SM_StateMachine *sm);
static bool sm_rt_dispatch_base(SM_StateMachine *sm, const SM_Event *event);
static bool sm_rt_is_in_state_base(const SM_StateMachine *sm, const SM_State *state);
static const char *sm_rt_get_current_state_name_base(const SM_StateMachine *sm);

/* --- Additional Helper Functions --- */
static const SM_Transition *find_matching_transition(const SM_State *state, const SM_Event *event, bool *guard_passed, SM_StateMachine *sm);
static bool execute_transition(SM_StateMachine *sm, const SM_Transition *transition, const SM_Event *event);
static bool process_state_transitions(SM_StateMachine *sm, const SM_State *state, const SM_Event *event);
static void execute_exit_actions(SM_StateMachine *sm, const SM_State *sourceState, const SM_State *lca, const SM_Event *event);
static bool build_entry_path(SM_StateMachine *sm, const SM_State *targetState, const SM_State *lca);
static void execute_entry_actions(SM_StateMachine *sm, uint8_t path_length, const SM_Event *event);

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
        sm_rt_init_base(&rt_sm->base_sm, initial_state, entry_path_buffer, 
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
        sm_rt_reset_base(&rt_sm->base_sm);
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
        *is_in_state = sm_rt_is_in_state_base(&rt_sm->base_sm, state);
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
        *state_name = sm_rt_get_current_state_name_base(&rt_sm->base_sm);
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
        event_handled = sm_rt_dispatch_base(&rt_sm->base_sm, event);
        
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
    const SM_State *lca = NULL;
    uint8_t path_length = 0U;
    bool same_state = false;
    bool valid_parameters = false;
    bool path_built = true;

    /* Parameter validation */
    if ((sm != NULL) && (target_state != NULL))
    {
        valid_parameters = true;
        source_state = sm->currentState;
        same_state = (source_state == target_state);
    }

    if (valid_parameters)
    {
        if (same_state)
        {
            /* External self-transition: execute exit then entry on the same state */
            if ((source_state != NULL) && (source_state->exitAction != NULL))
            {
                source_state->exitAction(sm, event);
            }
            if (target_state->entryAction != NULL)
            {
                target_state->entryAction(sm, event);
            }
        }
        else
        {
            /* Different states: perform hierarchical transition */
            lca = find_lca(source_state, target_state);

            /* Execute exit actions */
            execute_exit_actions(sm, source_state, lca, event);

            /* Build entry path and check if buffer is sufficient */
            path_built = build_entry_path(sm, target_state, lca);

            if (path_built)
            {
                /* Calculate path length for entry actions */
                const SM_State *entry_iter = target_state;
                path_length = 0U;
                while ((entry_iter != NULL) && (entry_iter != lca))
                {
                    path_length++;
                    entry_iter = entry_iter->parent;
                }

                /* Update current state */
                sm->currentState = target_state;

                /* Execute entry actions */
                execute_entry_actions(sm, path_length, event);
            }
        }
    }
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

/* --- Independent Base State Machine Functions Implementation --- */

static void sm_rt_init_base(SM_StateMachine *sm, const SM_State *initialState, 
                           const SM_State **entryPathBuffer, uint8_t bufferSize, 
                           void *userData, SM_ActionFn unhandledHook)
{
    bool valid_parameters = false;

    /* Parameter validation */
    if ((sm != NULL) && (initialState != NULL) && (entryPathBuffer != NULL) && (bufferSize > 0U))
    {
        valid_parameters = true;
    }

    if (valid_parameters)
    {
        /* Assign user data and hooks first, as they might be used in the initial entry actions */
        sm->userData = userData;
        sm->unhandledEventHook = unhandledHook;
        sm->initialState = initialState;
        sm->entryPathBuffer = entryPathBuffer;
        sm->bufferSize = bufferSize;
        sm->currentState = NULL; /* Setting to NULL ensures the full entry path is executed on first transition */

        /* Perform the initial transition */
        perform_transition(sm, initialState, NULL);
    }
}

static void sm_rt_reset_base(SM_StateMachine *sm)
{
    bool valid_parameters = false;

    /* Parameter validation */
    if ((sm != NULL) && (sm->initialState != NULL))
    {
        valid_parameters = true;
    }

    if (valid_parameters)
    {
        /* Transition from the current state to the initial state */
        perform_transition(sm, sm->initialState, NULL);
    }
}

static bool sm_rt_dispatch_base(SM_StateMachine *sm, const SM_Event *event)
{
    bool is_handled = false;
    const SM_State *state_iter = NULL;
    bool continue_processing = true;
    bool valid_parameters = false;

    /* Parameter validation */
    if ((sm != NULL) && (event != NULL))
    {
        valid_parameters = true;
        state_iter = sm->currentState;
    }

    if (valid_parameters)
    {
        /* Process event through state hierarchy */
        while ((state_iter != NULL) && continue_processing)
        {
            /* Try to process transitions for this state */
            if (process_state_transitions(sm, state_iter, event))
            {
                is_handled = true;
                continue_processing = false;
            }
            else
            {
                /* If not handled at this level, bubble up to parent */
                state_iter = state_iter->parent;
            }
        }

        /* Call unhandled event hook if event was not processed */
        if ((!is_handled) && (sm->unhandledEventHook != NULL))
        {
            sm->unhandledEventHook(sm, event);
        }
    }

    return is_handled;
}

static bool sm_rt_is_in_state_base(const SM_StateMachine *sm, const SM_State *state)
{
    bool is_in_state = false;
    const SM_State *current_iter = NULL;
    bool continue_search = true;
    bool valid_parameters = false;

    /* Parameter validation */
    if ((sm != NULL) && (state != NULL))
    {
        valid_parameters = true;
        current_iter = sm->currentState;
    }

    if (valid_parameters)
    {
        /* Search through state hierarchy */
        while ((current_iter != NULL) && continue_search)
        {
            if (current_iter == state)
            {
                is_in_state = true;
                continue_search = false;
            }
            else
            {
                current_iter = current_iter->parent;
            }
        }
    }

    return is_in_state;
}

static const char *sm_rt_get_current_state_name_base(const SM_StateMachine *sm)
{
    const char *name = "Unknown";

    /* Parameter validation and name retrieval */
    if ((sm != NULL) && (sm->currentState != NULL) && (sm->currentState->name != NULL))
    {
        name = sm->currentState->name;
    }

    return name;
}

/* --- Additional Helper Functions Implementation --- */

/**
 * @brief Finds a matching transition for the given event in the state's transition table.
 * @param[in] state The state to search for transitions.
 * @param[in] event The event to match.
 * @param[out] guard_passed Set to true if a matching transition's guard passes, false otherwise.
 * @param[in] sm The state machine instance (needed for guard evaluation).
 * @return Pointer to the matching transition, or NULL if none found.
 */
static const SM_Transition *find_matching_transition(const SM_State *state, const SM_Event *event, bool *guard_passed, SM_StateMachine *sm)
{
    const SM_Transition *result = NULL;
    size_t transition_idx = 0U;
    const SM_Transition *current_transition = NULL;
    bool found = false;

    /* Initialize output parameter */
    *guard_passed = false;

    /* Check if this state has transitions */
    if ((state != NULL) && (state->transitions != NULL) && (state->numTransitions > 0U) && (event != NULL))
    {
        /* Search through state's transition table */
        while ((transition_idx < state->numTransitions) && (!found))
        {
            current_transition = &state->transitions[transition_idx];

            /* Check if event ID matches */
            if (current_transition->eventId == event->id)
            {
                result = current_transition;
                found = true;
                
                /* Evaluate guard condition */
                *guard_passed = (current_transition->guard == NULL) || 
                               current_transition->guard(sm, event);
            }
            
            transition_idx++;
        }
    }

    return result;
}

/**
 * @brief Executes a transition action and state change if needed.
 * @param[in,out] sm The state machine instance.
 * @param[in] transition The transition to execute.
 * @param[in] event The event being processed.
 * @return true if the transition was executed, false otherwise.
 */
static bool execute_transition(SM_StateMachine *sm, const SM_Transition *transition, const SM_Event *event)
{
    bool executed = false;
    
    if ((sm != NULL) && (transition != NULL))
    {
        /* Handle transition based on type */
        if (transition->type == SM_TRANSITION_INTERNAL)
        {
            /* Internal transition: only execute action */
            if (transition->action != NULL)
            {
                transition->action(sm, event);
            }
        }
        else
        {
            /* External transition: execute action and change state */
            if (transition->action != NULL)
            {
                transition->action(sm, event);
            }
            perform_transition(sm, transition->target, event);
        }
        
        executed = true;
    }
    
    return executed;
}

/**
 * @brief Processes transitions for a single state.
 * @param[in,out] sm The state machine instance.
 * @param[in] state The state to process.
 * @param[in] event The event being processed.
 * @return true if the event was handled, false otherwise.
 */
static bool process_state_transitions(SM_StateMachine *sm, const SM_State *state, const SM_Event *event)
{
    bool handled = false;
    const SM_Transition *matching_transition = NULL;
    bool guard_passed = false;
    
    if ((sm != NULL) && (state != NULL) && (event != NULL))
    {
        /* Find matching transition */
        matching_transition = find_matching_transition(state, event, &guard_passed, sm);
        
        if ((matching_transition != NULL) && guard_passed)
        {
            /* Execute the transition */
            if (execute_transition(sm, matching_transition, event))
            {
                handled = true;
            }
        }
        else if ((matching_transition != NULL) && (!guard_passed))
        {
            SM_LOG_DEBUG("Guard for event %u in state '%s' failed.", 
                       (unsigned)event->id, state->name);
        }
    }
    
    return handled;
}

/**
 * @brief Executes exit actions from source state up to (but not including) the LCA.
 * @param[in,out] sm The state machine instance.
 * @param[in] sourceState The source state to start from.
 * @param[in] lca The lowest common ancestor state (exit actions stop before this state).
 * @param[in] event The event being processed.
 */
static void execute_exit_actions(SM_StateMachine *sm, const SM_State *sourceState, const SM_State *lca, const SM_Event *event)
{
    const SM_State *exit_iter = sourceState;
    
    /* Perform Exit Actions */
    while ((exit_iter != NULL) && (exit_iter != lca))
    {
        if (exit_iter->exitAction != NULL)
        {
            exit_iter->exitAction(sm, event);
        }
        exit_iter = exit_iter->parent;
    }
}

/**
 * @brief Builds the entry path from target state down to (but not including) the LCA.
 * @param[in,out] sm The state machine instance.
 * @param[in] targetState The target state to build path from.
 * @param[in] lca The lowest common ancestor state (path building stops before this state).
 * @return true if path was built successfully, false if buffer insufficient.
 */
static bool build_entry_path(SM_StateMachine *sm, const SM_State *targetState, const SM_State *lca)
{
    bool success = true;
    uint8_t path_idx = 0U;
    const SM_State *entry_iter = targetState;
    
    /* Record Entry Path */
    while ((entry_iter != NULL) && (entry_iter != lca) && success)
    {
        if (path_idx < sm->bufferSize)
        {
            sm->entryPathBuffer[path_idx] = entry_iter;
            path_idx++;
            entry_iter = entry_iter->parent;
        }
        else
        {
            success = false;
        }
    }
    
    return success;
}

/**
 * @brief Executes entry actions in reverse order from the entry path.
 * @param[in,out] sm The state machine instance.
 * @param[in] path_length The number of states in the entry path.
 * @param[in] event The event being processed.
 */
static void execute_entry_actions(SM_StateMachine *sm, uint8_t path_length, const SM_Event *event)
{
    int8_t entry_idx = (int8_t)path_length - 1;
    
    /* Perform Entry Actions (in reverse order) */
    while (entry_idx >= 0)
    {
        if (sm->entryPathBuffer[entry_idx]->entryAction != NULL)
        {
            sm->entryPathBuffer[entry_idx]->entryAction(sm, event);
        }
        entry_idx--;
    }
}