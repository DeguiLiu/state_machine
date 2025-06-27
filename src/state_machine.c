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
    SM_ASSERT(sm != NULL);
    SM_ASSERT(initialState != NULL);
    SM_ASSERT(entryPathBuffer != NULL);
    SM_ASSERT(bufferSize > 0U);

    // Assign user data and hooks first, as they might be used in the initial entry actions.
    sm->userData = userData;
    sm->unhandledEventHook = unhandledHook;
    sm->initialState = initialState;
    sm->entryPathBuffer = entryPathBuffer;
    sm->bufferSize = bufferSize;
    sm->currentState = NULL; // Setting to NULL ensures the full entry path is executed on first transition.

    // Perform the initial transition.
    perform_transition(sm, initialState, NULL);
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
    SM_ASSERT(sm != NULL);
    SM_ASSERT(sm->initialState != NULL);

    // Transition from the current state to the initial state.
    perform_transition(sm, sm->initialState, NULL);
}

bool SM_Dispatch(SM_StateMachine *sm, const SM_Event *event)
{
    SM_ASSERT(sm != NULL);
    SM_ASSERT(event != NULL);

    bool is_handled = false;
    const SM_State *state_iter = sm->currentState;

    while ((state_iter != NULL) && (!is_handled))
    {
        if ((state_iter->transitions != NULL) && (state_iter->numTransitions > 0U))
        {
            for (size_t i = 0; i < state_iter->numTransitions; ++i)
            {
                const SM_Transition *t = &state_iter->transitions[i];

                if (t->eventId == event->id)
                {
                    bool guard_passed = (t->guard == NULL) || t->guard(sm, event);
                    if (guard_passed)
                    {
                        // Guard passed, now handle based on transition type
                        if (t->type == SM_TRANSITION_INTERNAL)
                        {
                            // Internal transition: only execute action
                            if (t->action != NULL)
                            {
                                t->action(sm, event);
                            }
                        }
                        else
                        {
                            // External transition: execute action and then change state
                            if (t->action != NULL)
                            {
                                t->action(sm, event);
                            }
                            perform_transition(sm, t->target, event);
                        }
                        is_handled = true;
                        break; // Transition found and handled, exit for-loop
                    }
                    else
                    {
                        SM_LOG_DEBUG("Guard for event %u in state '%s' failed.", (unsigned)event->id, state_iter->name);
                    }
                }
            }
        }
        if (!is_handled)
        {
            state_iter = state_iter->parent; // Bubble up
        }
    }

    if (!is_handled && (sm->unhandledEventHook != NULL))
    {
        sm->unhandledEventHook(sm, event);
    }

    return is_handled;
}

bool SM_IsInState(const SM_StateMachine *sm, const SM_State *state)
{
    SM_ASSERT(sm != NULL);
    SM_ASSERT(state != NULL);

    bool is_in_state = false;
    const SM_State *current_iter = sm->currentState;

    while (current_iter != NULL)
    {
        if (current_iter == state)
        {
            is_in_state = true;
            break;
        }
        current_iter = current_iter->parent;
    }
    return is_in_state;
}

const char *SM_GetCurrentStateName(const SM_StateMachine *sm)
{
    const char *name = "Unknown";
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
    SM_ASSERT(sm != NULL);
    SM_ASSERT(targetState != NULL);

    const SM_State *sourceState = sm->currentState;

    if (sourceState == targetState)
    {
        // External self-transition: execute exit then entry on the same state.
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
        const SM_State *lca = find_lca(sourceState, targetState);

        // --- Perform Exit Actions ---
        const SM_State *exit_iter = sourceState;
        while ((exit_iter != NULL) && (exit_iter != lca))
        {
            if (exit_iter->exitAction != NULL)
            {
                exit_iter->exitAction(sm, event);
            }
            exit_iter = exit_iter->parent;
        }

        // --- Record Entry Path ---
        uint8_t path_idx = 0;
        const SM_State *entry_iter = targetState;
        while ((entry_iter != NULL) && (entry_iter != lca))
        {
            SM_ASSERT(path_idx < sm->bufferSize); // Ensure user-provided buffer is not overflowed
            sm->entryPathBuffer[path_idx] = entry_iter;
            path_idx++;
            entry_iter = entry_iter->parent;
        }

        // --- Update current state ---
        sm->currentState = targetState;

        // --- Perform Entry Actions ---
        for (int8_t i = (int8_t)path_idx - 1; i >= 0; --i)
        {
            if (sm->entryPathBuffer[i]->entryAction != NULL)
            {
                sm->entryPathBuffer[i]->entryAction(sm, event);
            }
        }
    }
}

static uint8_t get_state_depth(const SM_State *state)
{
    uint8_t depth = 0U;
    for (const SM_State *s = state; s != NULL; s = s->parent)
    {
        depth++;
    }
    return depth;
}

static const SM_State *find_lca(const SM_State *s1, const SM_State *s2)
{
    if ((s1 == NULL) || (s2 == NULL))
    {
        return NULL;
    }

    const SM_State *p1 = s1;
    const SM_State *p2 = s2;
    uint8_t depth1 = get_state_depth(p1);
    uint8_t depth2 = get_state_depth(p2);

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
    while (p1 != p2)
    {
        p1 = p1->parent;
        p2 = p2->parent;
    }

    return p1;
}
