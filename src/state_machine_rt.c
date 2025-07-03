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
static void sm_rt_worker_thread_entry(void *parameter);
static SM_RT_Result sm_rt_dispatch_event_internal(SM_RT_Instance *rtSm, const SM_Event *event);
static void sm_rt_perform_transition_internal(SM_StateMachine *sm, const SM_State *targetState, const SM_Event *event);
static uint8_t sm_rt_get_state_depth_internal(const SM_State *state);
static const SM_State *sm_rt_find_lca_internal(const SM_State *s1, const SM_State *s2);

/* --- Independent Base State Machine Functions --- */
static void sm_rt_init_base(SM_StateMachine *sm, const SM_State *initialState, 
                           const SM_State **entryPathBuffer, uint8_t bufferSize, 
                           void *userData, SM_ActionFn unhandledHook);
static void sm_rt_reset_base(SM_StateMachine *sm);
static bool sm_rt_dispatch_base(SM_StateMachine *sm, const SM_Event *event);
static bool sm_rt_is_in_state_base(const SM_StateMachine *sm, const SM_State *state);
static const char *sm_rt_get_current_state_name_base(const SM_StateMachine *sm);

/* --- Additional Helper Functions --- */
static const SM_Transition *sm_rt_find_matching_transition(const SM_State *state, const SM_Event *event, bool *guardPassed, SM_StateMachine *sm);
static bool sm_rt_execute_transition(SM_StateMachine *sm, const SM_Transition *transition, const SM_Event *event);
static bool sm_rt_process_state_transitions(SM_StateMachine *sm, const SM_State *state, const SM_Event *event);
static void sm_rt_execute_exit_actions(SM_StateMachine *sm, const SM_State *sourceState, const SM_State *lca, const SM_Event *event);
static bool sm_rt_build_entry_path(SM_StateMachine *sm, const SM_State *targetState, const SM_State *lca);
static void sm_rt_execute_entry_actions(SM_StateMachine *sm, uint8_t pathLength, const SM_Event *event);

/* --- Public API Implementation --- */

SM_RT_Result SM_RT_Init(SM_RT_Instance *rtSm,
                          const SM_State *initialState,
                          const SM_State **entryPathBuffer,
                          uint8_t bufferSize,
                          void *userData,
                          SM_ActionFn unhandledHook)
{
    SM_RT_Result result = SM_RT_RESULT_ERROR_UNKNOWN;
    bool validation_passed = false;
    
    /* Parameter validation */
    if ((rtSm != NULL) && (initialState != NULL) && (entryPathBuffer != NULL) && (bufferSize > 0U))
    {
        if (!rtSm->is_initialized)
        {
            validation_passed = true;
        }
        else
        {
            result = SM_RT_RESULT_ERROR_ALREADY_INIT;
        }
    }
    else
    {
        result = SM_RT_RESULT_ERROR_NULL_PTR;
    }
    
    if (validation_passed)
    {
        /* Initialize the base state machine */
        sm_rt_init_base(&rtSm->base_sm, initialState, entryPathBuffer, 
                       bufferSize, userData, unhandledHook);
        
        /* Initialize RT-specific fields */
        rtSm->stats.total_events_processed = 0U;
        rtSm->stats.total_events_unhandled = 0U;
        rtSm->stats.total_transitions = 0U;
        rtSm->stats.current_queue_depth = 0U;
        rtSm->stats.max_queue_depth = 0U;
        rtSm->is_initialized = true;
        rtSm->is_started = false;
        rtSm->worker_thread = NULL;
        rtSm->event_queue = NULL;
        rtSm->mutex = NULL;
        
        result = SM_RT_RESULT_SUCCESS;
    }
    
    return result;
}

SM_RT_Result SM_RT_Start(SM_RT_Instance *rtSm)
{
    SM_RT_Result result = SM_RT_RESULT_ERROR_UNKNOWN;
    bool validation_passed = false;
    
    /* Parameter validation */
    if ((rtSm != NULL) && (rtSm->is_initialized) && (!rtSm->is_started))
    {
        validation_passed = true;
    }
    else if (rtSm == NULL)
    {
        result = SM_RT_RESULT_ERROR_NULL_PTR;
    }
    else if (!rtSm->is_initialized)
    {
        result = SM_RT_RESULT_ERROR_NOT_INIT;
    }
    else
    {
        result = SM_RT_RESULT_ERROR_ALREADY_STARTED;
    }
    
    if (validation_passed)
    {
        /* Note: RTOS specific implementation would create thread and queue here */
        /* For now, we simulate the start operation */
        rtSm->is_started = true;
        result = SM_RT_RESULT_SUCCESS;
    }
    
    return result;
}

SM_RT_Result SM_RT_Stop(SM_RT_Instance *rtSm)
{
    SM_RT_Result result = SM_RT_RESULT_ERROR_UNKNOWN;
    bool validation_passed = false;
    
    /* Parameter validation */
    if ((rtSm != NULL) && (rtSm->is_initialized) && (rtSm->is_started))
    {
        validation_passed = true;
    }
    else if (rtSm == NULL)
    {
        result = SM_RT_RESULT_ERROR_NULL_PTR;
    }
    else if (!rtSm->is_initialized)
    {
        result = SM_RT_RESULT_ERROR_NOT_INIT;
    }
    else
    {
        result = SM_RT_RESULT_ERROR_NOT_STARTED;
    }
    
    if (validation_passed)
    {
        /* Note: RTOS specific implementation would stop thread and cleanup here */
        rtSm->is_started = false;
        rtSm->worker_thread = NULL;
        rtSm->event_queue = NULL;
        rtSm->mutex = NULL;
        result = SM_RT_RESULT_SUCCESS;
    }
    
    return result;
}

SM_RT_Result SM_RT_PostEvent(SM_RT_Instance *rtSm, const SM_Event *event)
{
    SM_RT_Result result = SM_RT_RESULT_ERROR_UNKNOWN;
    bool validation_passed = false;
    
    /* Parameter validation */
    if ((rtSm != NULL) && (event != NULL) && (rtSm->is_initialized) && (rtSm->is_started))
    {
        validation_passed = true;
    }
    else if ((rtSm == NULL) || (event == NULL))
    {
        result = SM_RT_RESULT_ERROR_NULL_PTR;
    }
    else if (!rtSm->is_initialized)
    {
        result = SM_RT_RESULT_ERROR_NOT_INIT;
    }
    else
    {
        result = SM_RT_RESULT_ERROR_NOT_STARTED;
    }
    
    if (validation_passed)
    {
        /* For now, directly dispatch the event (in real RTOS implementation, 
           this would post to a message queue) */
        result = sm_rt_dispatch_event_internal(rtSm, event);
    }
    
    return result;
}

SM_RT_Result SM_RT_PostEventId(SM_RT_Instance *rtSm, uint32_t eventId, void *context)
{
    SM_RT_Result result = SM_RT_RESULT_ERROR_UNKNOWN;
    bool validation_passed = false;
    SM_Event event_to_post = {0U, NULL};
    
    /* Parameter validation */
    if ((rtSm != NULL) && (rtSm->is_initialized) && (rtSm->is_started))
    {
        validation_passed = true;
    }
    else if (rtSm == NULL)
    {
        result = SM_RT_RESULT_ERROR_NULL_PTR;
    }
    else if (!rtSm->is_initialized)
    {
        result = SM_RT_RESULT_ERROR_NOT_INIT;
    }
    else
    {
        result = SM_RT_RESULT_ERROR_NOT_STARTED;
    }
    
    if (validation_passed)
    {
        /* Initialize event structure */
        event_to_post.id = eventId;
        event_to_post.context = context;
        
        /* Post the event */
        result = SM_RT_PostEvent(rtSm, &event_to_post);
    }
    
    return result;
}

SM_RT_Result SM_RT_Reset(SM_RT_Instance *rtSm)
{
    SM_RT_Result result = SM_RT_RESULT_ERROR_UNKNOWN;
    bool validation_passed = false;
    
    /* Parameter validation */
    if ((rtSm != NULL) && (rtSm->is_initialized))
    {
        validation_passed = true;
    }
    else if (rtSm == NULL)
    {
        result = SM_RT_RESULT_ERROR_NULL_PTR;
    }
    else
    {
        result = SM_RT_RESULT_ERROR_NOT_INIT;
    }
    
    if (validation_passed)
    {
        /* Reset the base state machine */
        sm_rt_reset_base(&rtSm->base_sm);
        rtSm->stats.total_transitions++;
        result = SM_RT_RESULT_SUCCESS;
    }
    
    return result;
}

SM_RT_Result SM_RT_IsInState(const SM_RT_Instance *rtSm, 
                               const SM_State *state, 
                               bool *isInState)
{
    SM_RT_Result result = SM_RT_RESULT_ERROR_UNKNOWN;
    bool validation_passed = false;
    
    /* Parameter validation */
    if ((rtSm != NULL) && (state != NULL) && (isInState != NULL) && (rtSm->is_initialized))
    {
        validation_passed = true;
    }
    else if ((rtSm == NULL) || (state == NULL) || (isInState == NULL))
    {
        result = SM_RT_RESULT_ERROR_NULL_PTR;
    }
    else
    {
        result = SM_RT_RESULT_ERROR_NOT_INIT;
    }
    
    if (validation_passed)
    {
        /* Use the base state machine function */
        *isInState = sm_rt_is_in_state_base(&rtSm->base_sm, state);
        result = SM_RT_RESULT_SUCCESS;
    }
    else
    {
        /* Ensure output parameter is set even on error */
        if (isInState != NULL)
        {
            *isInState = false;
        }
    }
    
    return result;
}

SM_RT_Result SM_RT_GetCurrentStateName(const SM_RT_Instance *rtSm, 
                                         const char **stateName)
{
    SM_RT_Result result = SM_RT_RESULT_ERROR_UNKNOWN;
    bool validation_passed = false;
    
    /* Parameter validation */
    if ((rtSm != NULL) && (stateName != NULL) && (rtSm->is_initialized))
    {
        validation_passed = true;
    }
    else if ((rtSm == NULL) || (stateName == NULL))
    {
        result = SM_RT_RESULT_ERROR_NULL_PTR;
    }
    else
    {
        result = SM_RT_RESULT_ERROR_NOT_INIT;
    }
    
    if (validation_passed)
    {
        /* Use the base state machine function */
        *stateName = sm_rt_get_current_state_name_base(&rtSm->base_sm);
        result = SM_RT_RESULT_SUCCESS;
    }
    else
    {
        /* Ensure output parameter is set even on error */
        if (stateName != NULL)
        {
            *stateName = "Unknown";
        }
    }
    
    return result;
}

SM_RT_Result SM_RT_GetStatistics(const SM_RT_Instance *rtSm, 
                                   SM_RT_Statistics *stats)
{
    SM_RT_Result result = SM_RT_RESULT_ERROR_UNKNOWN;
    bool validation_passed = false;
    
    /* Parameter validation */
    if ((rtSm != NULL) && (stats != NULL) && (rtSm->is_initialized))
    {
        validation_passed = true;
    }
    else if ((rtSm == NULL) || (stats == NULL))
    {
        result = SM_RT_RESULT_ERROR_NULL_PTR;
    }
    else
    {
        result = SM_RT_RESULT_ERROR_NOT_INIT;
    }
    
    if (validation_passed)
    {
        /* Copy statistics structure */
        stats->total_events_processed = rtSm->stats.total_events_processed;
        stats->total_events_unhandled = rtSm->stats.total_events_unhandled;
        stats->total_transitions = rtSm->stats.total_transitions;
        stats->current_queue_depth = rtSm->stats.current_queue_depth;
        stats->max_queue_depth = rtSm->stats.max_queue_depth;
        result = SM_RT_RESULT_SUCCESS;
    }
    else
    {
        /* Ensure output parameter is initialized even on error */
        if (stats != NULL)
        {
            stats->total_events_processed = 0U;
            stats->total_events_unhandled = 0U;
            stats->total_transitions = 0U;
            stats->current_queue_depth = 0U;
            stats->max_queue_depth = 0U;
        }
    }
    
    return result;
}

SM_RT_Result SM_RT_ResetStatistics(SM_RT_Instance *rtSm)
{
    SM_RT_Result result = SM_RT_RESULT_ERROR_UNKNOWN;
    bool validation_passed = false;
    
    /* Parameter validation */
    if ((rtSm != NULL) && (rtSm->is_initialized))
    {
        validation_passed = true;
    }
    else if (rtSm == NULL)
    {
        result = SM_RT_RESULT_ERROR_NULL_PTR;
    }
    else
    {
        result = SM_RT_RESULT_ERROR_NOT_INIT;
    }
    
    if (validation_passed)
    {
        /* Reset all statistics */
        rtSm->stats.total_events_processed = 0U;
        rtSm->stats.total_events_unhandled = 0U;
        rtSm->stats.total_transitions = 0U;
        rtSm->stats.current_queue_depth = 0U;
        rtSm->stats.max_queue_depth = 0U;
        result = SM_RT_RESULT_SUCCESS;
    }
    
    return result;
}

/* --- Private Helper Function Implementations --- */

static void sm_rt_worker_thread_entry(void *parameter)
{
    SM_RT_Instance *rt_sm = NULL;
    bool valid_parameter = false;
    
    /* Initialize local variables */
    rt_sm = (SM_RT_Instance *)parameter;
    
    /* Parameter validation */
    if (rt_sm != NULL)
    {
        valid_parameter = true;
    }
    
    if (valid_parameter)
    {
        /* Note: In a real RTOS implementation, this would be an infinite loop
           waiting for events from the message queue and processing them */
        /* For now, this is a placeholder implementation */
    }
    
    /* Single return point */
    return;
}

static SM_RT_Result sm_rt_dispatch_event_internal(SM_RT_Instance *rtSm, const SM_Event *event)
{
    SM_RT_Result result = SM_RT_RESULT_ERROR_UNKNOWN;
    bool validation_passed = false;
    bool event_handled = false;
    
    /* Parameter validation */
    if ((rtSm != NULL) && (event != NULL))
    {
        validation_passed = true;
    }
    else
    {
        result = SM_RT_RESULT_ERROR_NULL_PTR;
    }
    
    if (validation_passed)
    {
        /* Dispatch event to base state machine */
        event_handled = sm_rt_dispatch_base(&rtSm->base_sm, event);
        
        /* Update statistics */
        rtSm->stats.total_events_processed++;
        if (!event_handled)
        {
            rtSm->stats.total_events_unhandled++;
        }
        
        result = SM_RT_RESULT_SUCCESS;
    }
    
    return result;
}

static void sm_rt_perform_transition_internal(SM_StateMachine *sm, const SM_State *targetState, const SM_Event *event)
{
    const SM_State *sourceState = NULL;
    const SM_State *lca = NULL;
    uint8_t pathLength = 0U;
    bool same_state = false;
    bool valid_parameters = false;
    bool path_built = true;

    /* Parameter validation */
    if ((sm != NULL) && (targetState != NULL))
    {
        valid_parameters = true;
        sourceState = sm->currentState;
        same_state = (sourceState == targetState);
    }

    if (valid_parameters)
    {
        if (same_state)
        {
            /* External self-transition: execute exit then entry on the same state */
            if ((sourceState != NULL) && (sourceState->exitAction != NULL))
            {
                sourceState->exitAction(sm, event);
            }
            if (targetState->entryAction != NULL)
            {
                targetState->entryAction(sm, event);
            }
        }
        else
        {
            /* Different states: perform hierarchical transition */
            lca = sm_rt_find_lca_internal(sourceState, targetState);

            /* Execute exit actions */
            sm_rt_execute_exit_actions(sm, sourceState, lca, event);

            /* Build entry path and check if buffer is sufficient */
            path_built = sm_rt_build_entry_path(sm, targetState, lca);

            if (path_built)
            {
                /* Calculate path length for entry actions */
                const SM_State *entry_iter = targetState;
                pathLength = 0U;
                while ((entry_iter != NULL) && (entry_iter != lca))
                {
                    pathLength++;
                    entry_iter = entry_iter->parent;
                }

                /* Update current state */
                sm->currentState = targetState;

                /* Execute entry actions */
                sm_rt_execute_entry_actions(sm, pathLength, event);
            }
        }
    }
}

static uint8_t sm_rt_get_state_depth_internal(const SM_State *state)
{
    uint8_t depth = 0U;
    const SM_State *current_state = state;
    
    /* Calculate depth by traversing parent chain */
    while (current_state != NULL)
    {
        depth++;
        current_state = current_state->parent;
    }
    
    return depth;
}

static const SM_State *sm_rt_find_lca_internal(const SM_State *s1, const SM_State *s2)
{
    const SM_State *result = NULL;
    const SM_State *p1 = NULL;
    const SM_State *p2 = NULL;
    uint8_t depth1 = 0U;
    uint8_t depth2 = 0U;
    bool valid_parameters = false;
    
    /* Parameter validation and initialization */
    if ((s1 != NULL) && (s2 != NULL))
    {
        valid_parameters = true;
        p1 = s1;
        p2 = s2;
        depth1 = sm_rt_get_state_depth_internal(p1);
        depth2 = sm_rt_get_state_depth_internal(p2);
    }
    
    if (valid_parameters)
    {
        /* Normalize depths */
        while (depth1 > depth2)
        {
            p1 = p1->parent;
            depth1--;
        }
        while (depth2 > depth1)
        {
            p2 = p2->parent;
            depth2--;
        }
        
        /* Find common ancestor */
        while (p1 != p2)
        {
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
        sm_rt_perform_transition_internal(sm, initialState, NULL);
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
        sm_rt_perform_transition_internal(sm, sm->initialState, NULL);
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
            if (sm_rt_process_state_transitions(sm, state_iter, event))
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
static const SM_Transition *sm_rt_find_matching_transition(const SM_State *state, const SM_Event *event, bool *guardPassed, SM_StateMachine *sm)
{
    const SM_Transition *result = NULL;
    size_t transition_idx = 0U;
    const SM_Transition *current_transition = NULL;
    bool found = false;

    /* Initialize output parameter */
    *guardPassed = false;

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
                *guardPassed = (current_transition->guard == NULL) || 
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
static bool sm_rt_execute_transition(SM_StateMachine *sm, const SM_Transition *transition, const SM_Event *event)
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
            sm_rt_perform_transition_internal(sm, transition->target, event);
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
static bool sm_rt_process_state_transitions(SM_StateMachine *sm, const SM_State *state, const SM_Event *event)
{
    bool handled = false;
    const SM_Transition *matching_transition = NULL;
    bool guard_passed = false;
    
    if ((sm != NULL) && (state != NULL) && (event != NULL))
    {
        /* Find matching transition */
        matching_transition = sm_rt_find_matching_transition(state, event, &guard_passed, sm);
        
        if ((matching_transition != NULL) && guard_passed)
        {
            /* Execute the transition */
            if (sm_rt_execute_transition(sm, matching_transition, event))
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
static void sm_rt_execute_exit_actions(SM_StateMachine *sm, const SM_State *sourceState, const SM_State *lca, const SM_Event *event)
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
 * @param[in] target_state The target state to build path from.
 * @param[in] lca The lowest common ancestor state (path building stops before this state).
 * @return true if path was built successfully, false if buffer insufficient.
 */
static bool sm_rt_build_entry_path(SM_StateMachine *sm, const SM_State *targetState, const SM_State *lca)
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
 * @param[in] pathLength The number of states in the entry path.
 * @param[in] event The event being processed.
 */
static void sm_rt_execute_entry_actions(SM_StateMachine *sm, uint8_t pathLength, const SM_Event *event)
{
    int8_t entry_idx = (int8_t)pathLength - 1;
    
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