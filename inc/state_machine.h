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
 * @file state_machine.h
 * @brief A lightweight, MISRA-C:2012 compliant, hierarchical state machine framework.
 *
 * This framework provides a data-driven approach to creating hierarchical state machines (HSMs).
 * It is designed to be decoupled from any specific OS, making it highly portable.
 */

#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* --- User-configurable Macros --- */

/**
 * @brief User-definable assertion macro.
 * Maps to RT-Thread's RT_ASSERT or a standard assert in other environments.
 */
#ifndef SM_ASSERT
    #ifdef NDEBUG
        #define SM_ASSERT(expr) ((void)0)
    #else
        #define SM_ASSERT(expr) if (!(expr)) { for(;;); }
    #endif
#endif

/**
 * @brief User-definable debug logging macro.
 * Define this to your system's logging function (e.g., LOG_D from rt-thread/ulog).
 */
#ifndef SM_LOG_DEBUG
    #define SM_LOG_DEBUG(...) ((void)0)
#endif


/* --- Core Types --- */

typedef struct SM_StateMachine SM_StateMachine;
typedef struct SM_State        SM_State;
typedef struct SM_Event        SM_Event;
typedef struct SM_Transition   SM_Transition;

/**
 * @brief Event structure passed to the state machine.
 */
struct SM_Event {
    uint32_t  id;       /**< Application-specific event identifier. */
    void     *context;  /**< Optional pointer to event-specific data. */
};

/**
 * @brief Defines the type of a state transition.
 */
typedef enum {
    /**
     * @brief An external transition. Causes exit from source state and entry to target state.
     * If source and target are the same, it's a self-transition which will execute exit and entry actions.
     */
    SM_TRANSITION_EXTERNAL,

    /**
     * @brief An internal transition. Executes only the action, without any exit or entry calls.
     * The state does not change. The target state in the transition table is ignored.
     */
    SM_TRANSITION_INTERNAL
} SM_TransitionType;

typedef void (*SM_ActionFn)(SM_StateMachine *sm, const SM_Event *event);
typedef bool (*SM_GuardFn)(SM_StateMachine *sm, const SM_Event *event);

/**
 * @brief Defines a single state transition rule.
 */
struct SM_Transition {
    uint32_t          eventId;  /**< The event ID that triggers this transition. */
    const SM_State   *target;   /**< The target state (ignored for internal transitions). */
    SM_GuardFn        guard;    /**< Optional guard condition. Transition occurs if it returns true or is NULL. */
    SM_ActionFn       action;   /**< Optional action executed during the transition. */
    SM_TransitionType type;     /**< The type of the transition (external or internal). */
};

/**
 * @brief Defines a state and its behavior.
 */
struct SM_State {
    const SM_State      *parent;        /**< Pointer to parent (super) state, or NULL for top-level states. */
    SM_ActionFn          entryAction;   /**< Optional action executed upon entering the state. */
    SM_ActionFn          exitAction;    /**< Optional action executed upon exiting the state. */
    const SM_Transition *transitions;   /**< Pointer to the state's transition table. */
    size_t               numTransitions;/**< Number of transitions in the table. */
    const char          *name;          /**< Optional name for debugging. */
};

/**
 * @brief The state machine instance.
 */
struct SM_StateMachine {
    const SM_State *currentState;         /**< The current active state. */
    const SM_State *initialState;         /**< The initial state for SM_Reset. */
    void           *userData;             /**< Optional pointer to user-specific data. */
    SM_ActionFn     unhandledEventHook;   /**< Optional hook for unhandled events. */
    const SM_State **entryPathBuffer;     /**< User-provided buffer for transition calculations. */
    uint8_t         bufferSize;           /**< Size of the user-provided buffer. */
};


/* --- Public API --- */

/**
 * @brief Initializes a state machine instance.
 * @param[out] sm                 Pointer to the state machine instance.
 * @param[in]  initialState       Pointer to the initial state.
 * @param[in]  entryPathBuffer    A user-provided buffer to calculate transition paths. Its size must be >= max hierarchy depth.
 * @param[in]  bufferSize         The size of the entryPathBuffer.
 * @param[in]  userData           Optional pointer to user data, can be NULL.
 * @param[in]  unhandledHook      Optional hook for unhandled events, can be NULL.
 */
void SM_Init(SM_StateMachine *sm,
             const SM_State *initialState,
             const SM_State **entryPathBuffer,
             uint8_t bufferSize,
             void *userData,
             SM_ActionFn unhandledHook);

/**
 * @brief Deinitializes the state machine instance, clearing internal pointers.
 * @param[out] sm Pointer to the state machine instance.
 */
void SM_Deinit(SM_StateMachine *sm);

/**
 * @brief Resets the state machine to its initial state.
 * @param[in,out] sm Pointer to the state machine instance.
 */
void SM_Reset(SM_StateMachine *sm);

/**
 * @brief Dispatches an event to the state machine.
 * @param[in,out] sm    Pointer to the state machine instance.
 * @param[in]     event Pointer to the event to process.
 * @return true if the event was handled, false otherwise.
 */
bool SM_Dispatch(SM_StateMachine *sm, const SM_Event *event);

/**
 * @brief Checks if the current state is a specific state or a substate of it.
 * @param[in] sm    Pointer to the state machine instance.
 * @param[in] state The state to check against (the potential parent state).
 * @return true if the current state is `state` or one of its children, false otherwise.
 */
bool SM_IsInState(const SM_StateMachine *sm, const SM_State *state);

/**
 * @brief Gets the name of the current state.
 * @param[in] sm Pointer to the state machine instance.
 * @return The name string of the current state, or "Unknown" if not available.
 */
const char* SM_GetCurrentStateName(const SM_StateMachine *sm);

#endif /* STATE_MACHINE_H */
