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
 
#include "state_machine.h"

// --- Private Helper Functions ---
static uint8_t get_state_depth(const SM_State *state);
static const SM_State *find_lca(const SM_State *s1, const SM_State *s2);
static void perform_transition(SM_StateMachine *sm, const SM_State *targetState, const SM_Event *event);
static const SM_Transition *find_matching_transition(const SM_State *state, const SM_Event *event, bool *guard_passed, SM_StateMachine *sm);
static bool execute_transition(SM_StateMachine *sm, const SM_Transition *transition, const SM_Event *event);
static bool process_state_transitions(SM_StateMachine *sm, const SM_State *state, const SM_Event *event);
static void execute_exit_actions(SM_StateMachine *sm, const SM_State *sourceState, const SM_State *lca, const SM_Event *event);
static bool build_entry_path(SM_StateMachine *sm, const SM_State *targetState, const SM_State *lca);
static void execute_entry_actions(SM_StateMachine *sm, uint8_t path_length, const SM_Event *event);

// --- Public API Implementation ---

void SM_Init(SM_StateMachine *sm,
             const SM_State *initialState,
             const SM_State **entryPathBuffer,
             uint8_t bufferSize,
             void *userData,
             SM_ActionFn unhandledHook)
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

void SM_Deinit(SM_StateMachine *sm)
{
    if (sm != NULL)
    {
        sm->currentState = NULL;
        sm->initialState = NULL;
        sm->userData = NULL;
        sm->unhandledEventHook = NULL;
        sm->entryPathBuffer = NULL;
        sm->bufferSize = 0U;
    }
}

void SM_Reset(SM_StateMachine *sm)
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

bool SM_Dispatch(SM_StateMachine *sm, const SM_Event *event)
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

bool SM_IsInState(const SM_StateMachine *sm, const SM_State *state)
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

const char *SM_GetCurrentStateName(const SM_StateMachine *sm)
{
    const char *name = "Unknown";

    /* Parameter validation and name retrieval */
    if ((sm != NULL) && (sm->currentState != NULL) && (sm->currentState->name != NULL))
    {
        name = sm->currentState->name;
    }

    return name;
}

// --- Private Helper Implementations ---

/**
 * @brief Performs the state transition logic, executing exit and entry actions.
 * @note This is the core transition function.
 */
static void perform_transition(SM_StateMachine *sm, const SM_State *targetState, const SM_Event *event)
{
    const SM_State *sourceState = NULL;
    const SM_State *lca = NULL;
    uint8_t path_length = 0U;
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
            lca = find_lca(sourceState, targetState);

            /* Execute exit actions */
            execute_exit_actions(sm, sourceState, lca, event);

            /* Build entry path and check if buffer is sufficient */
            path_built = build_entry_path(sm, targetState, lca);

            if (path_built)
            {
                /* Calculate path length for entry actions */
                const SM_State *entry_iter = targetState;
                path_length = 0U;
                while ((entry_iter != NULL) && (entry_iter != lca))
                {
                    path_length++;
                    entry_iter = entry_iter->parent;
                }

                /* Update current state */
                sm->currentState = targetState;

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
    while (current_state != NULL)
    {
        depth++;
        current_state = current_state->parent;
    }

    return depth;
}

const SM_State *find_lca(const SM_State *s1, const SM_State *s2)
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
        depth1 = get_state_depth(p1);
        depth2 = get_state_depth(p2);
    }

    if (valid_parameters)
    {
        /* Normalize depths to same level */
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
