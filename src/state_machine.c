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
    size_t transition_idx = 0U;
    const SM_Transition *current_transition = NULL;
    bool guard_passed = false;
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
            /* Check if this state has transitions */
            if ((state_iter->transitions != NULL) && (state_iter->numTransitions > 0U))
            {
                /* Search through state's transition table */
                transition_idx = 0U;
                while ((transition_idx < state_iter->numTransitions) && continue_processing)
                {
                    current_transition = &state_iter->transitions[transition_idx];

                    /* Check if event ID matches */
                    if (current_transition->eventId == event->id)
                    {
                        /* Evaluate guard condition */
                        guard_passed = (current_transition->guard == NULL) || 
                                       current_transition->guard(sm, event);
                        
                        if (guard_passed)
                        {
                            /* Handle transition based on type */
                            if (current_transition->type == SM_TRANSITION_INTERNAL)
                            {
                                /* Internal transition: only execute action */
                                if (current_transition->action != NULL)
                                {
                                    current_transition->action(sm, event);
                                }
                            }
                            else
                            {
                                /* External transition: execute action and change state */
                                if (current_transition->action != NULL)
                                {
                                    current_transition->action(sm, event);
                                }
                                perform_transition(sm, current_transition->target, event);
                            }
                            
                            is_handled = true;
                            continue_processing = false;
                        }
                        else
                        {
                            SM_LOG_DEBUG("Guard for event %u in state '%s' failed.", 
                                       (unsigned)event->id, state_iter->name);
                        }
                    }
                    
                    transition_idx++;
                }
            }
            
            /* If not handled at this level, bubble up to parent */
            if (continue_processing)
            {
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
    const SM_State *exit_iter = NULL;
    const SM_State *entry_iter = NULL;
    uint8_t path_idx = 0U;
    int8_t entry_idx = 0;
    bool same_state = false;
    bool valid_parameters = false;
    bool buffer_sufficient = true;

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

            /* Perform Exit Actions */
            exit_iter = sourceState;
            while ((exit_iter != NULL) && (exit_iter != lca))
            {
                if (exit_iter->exitAction != NULL)
                {
                    exit_iter->exitAction(sm, event);
                }
                exit_iter = exit_iter->parent;
            }

            /* Record Entry Path */
            path_idx = 0U;
            entry_iter = targetState;
            while ((entry_iter != NULL) && (entry_iter != lca) && buffer_sufficient)
            {
                if (path_idx < sm->bufferSize)
                {
                    sm->entryPathBuffer[path_idx] = entry_iter;
                    path_idx++;
                    entry_iter = entry_iter->parent;
                }
                else
                {
                    buffer_sufficient = false;
                }
            }

            if (buffer_sufficient)
            {
                /* Update current state */
                sm->currentState = targetState;

                /* Perform Entry Actions (in reverse order) */
                entry_idx = (int8_t)path_idx - 1;
                while (entry_idx >= 0)
                {
                    if (sm->entryPathBuffer[entry_idx]->entryAction != NULL)
                    {
                        sm->entryPathBuffer[entry_idx]->entryAction(sm, event);
                    }
                    entry_idx--;
                }
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

static const SM_State *find_lca(const SM_State *s1, const SM_State *s2)
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
