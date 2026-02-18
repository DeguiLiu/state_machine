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
 * @file state_machine_rt.h
 * @brief RTOS wrapper for the hierarchical state machine framework - Independent Implementation.
 *
 * This module provides RTOS-specific extensions to the state machine framework,
 * including thread-safe operations, statistics collection, and integration with
 * real-time operating system services such as threads, mutexes, and message queues.
 *
 * This implementation is completely independent from the base state_machine module,
 * eliminating code duplication and providing clear separation of concerns for
 * RTOS-specific requirements. All base state machine functionality is implemented
 * directly within this module.
 *
 * The design is suitable for integration with RTOS platforms such as RT-Thread,
 * POSIX-like environments.
 */

#ifndef STATE_MACHINE_RT_H
#define STATE_MACHINE_RT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* --- User-configurable Macros --- */

/**
 * @brief User-definable assertion macro.
 * Maps to RT-Thread's RT_ASSERT or a standard assert in other environments.
 */
#ifndef SM_ASSERT
#ifdef NDEBUG
#define SM_ASSERT(expr) ((void)0)
#else
#define SM_ASSERT(expr)                                                                            \
    if (!(expr)) {                                                                                 \
        for (;;)                                                                                   \
            ;                                                                                      \
    }
#endif
#endif

/**
 * @brief User-definable debug logging macro.
 * Define this to your system's logging function (e.g., LOG_D from rt-thread/ulog).
 */
#ifndef SM_LOG_DEBUG
#define SM_LOG_DEBUG(...) ((void)0)
#endif

/* --- Core Types (Independent from base state_machine.h) --- */

/* Only define these types if they haven't been defined by state_machine.h */
#ifndef SM_CORE_TYPES_DEFINED
#define SM_CORE_TYPES_DEFINED

typedef struct SM_StateMachine SM_StateMachine;
typedef struct SM_State SM_State;
typedef struct SM_Event SM_Event;
typedef struct SM_Transition SM_Transition;

/**
 * @brief Event structure passed to the state machine.
 */
struct SM_Event {
    uint32_t id;   /**< Application-specific event identifier. */
    void* context; /**< Optional pointer to event-specific data. */
};

/**
 * @brief Defines the type of a state transition.
 */
typedef enum {
    /**
     * @brief An external transition. Causes exit from source state and entry to target state.
     * If source and target are the same, it's a self-transition which will execute exit and entry
     * actions.
     */
    SM_TRANSITION_EXTERNAL,

    /**
     * @brief An internal transition. Executes only the action, without any exit or entry calls.
     * The state does not change. The target state in the transition table is ignored.
     */
    SM_TRANSITION_INTERNAL
} SM_TransitionType;

typedef void (*SM_ActionFn)(SM_StateMachine* sm, const SM_Event* event);
typedef bool (*SM_GuardFn)(SM_StateMachine* sm, const SM_Event* event);

/**
 * @brief Defines a single state transition rule.
 */
struct SM_Transition {
    uint32_t eventId;       /**< The event ID that triggers this transition. */
    const SM_State* target; /**< The target state (ignored for internal transitions). */
    SM_GuardFn
        guard; /**< Optional guard condition. Transition occurs if it returns true or is NULL. */
    SM_ActionFn action;     /**< Optional action executed during the transition. */
    SM_TransitionType type; /**< The type of the transition (external or internal). */
};

/**
 * @brief Defines a state and its behavior.
 */
struct SM_State {
    const SM_State* parent;  /**< Pointer to parent (super) state, or NULL for top-level states. */
    SM_ActionFn entryAction; /**< Optional action executed upon entering the state. */
    SM_ActionFn exitAction;  /**< Optional action executed upon exiting the state. */
    const SM_Transition* transitions; /**< Pointer to the state's transition table. */
    size_t numTransitions;            /**< Number of transitions in the table. */
    const char* name;                 /**< Optional name for debugging. */
};

/**
 * @brief The state machine instance.
 */
struct SM_StateMachine {
    const SM_State* currentState;     /**< The current active state. */
    const SM_State* initialState;     /**< The initial state for SM_Reset. */
    void* userData;                   /**< Optional pointer to user-specific data. */
    SM_ActionFn unhandledEventHook;   /**< Optional hook for unhandled events. */
    const SM_State** entryPathBuffer; /**< User-provided buffer for transition calculations. */
    uint8_t bufferSize;               /**< Size of the user-provided buffer. */
};

#endif /* SM_CORE_TYPES_DEFINED */

/* --- RT(real-time os)-specific Types --- */

/**
 * @brief Result codes for RT state machine operations.
 */
typedef enum {
    SM_RT_RESULT_SUCCESS = 0,           /**< Operation completed successfully. */
    SM_RT_RESULT_ERROR_NULL_PTR,        /**< Null pointer provided as parameter. */
    SM_RT_RESULT_ERROR_INVALID,         /**< Invalid parameter value. */
    SM_RT_RESULT_ERROR_NOT_INIT,        /**< State machine not initialized. */
    SM_RT_RESULT_ERROR_ALREADY_INIT,    /**< State machine already initialized. */
    SM_RT_RESULT_ERROR_NOT_STARTED,     /**< State machine not started. */
    SM_RT_RESULT_ERROR_ALREADY_STARTED, /**< State machine already started. */
    SM_RT_RESULT_ERROR_QUEUE_FULL,      /**< Event queue is full. */
    SM_RT_RESULT_ERROR_UNKNOWN          /**< Unknown error occurred. */
} SM_RT_Result;

/**
 * @brief State machine statistics.
 */
typedef struct {
    uint32_t total_events_processed; /**< Total number of events processed. */
    uint32_t total_events_unhandled; /**< Total number of unhandled events. */
    uint32_t total_transitions;      /**< Total number of state transitions. */
    uint32_t current_queue_depth;    /**< Current depth of event queue. */
    uint32_t max_queue_depth;        /**< Maximum depth reached by event queue. */
} SM_RT_Statistics;

/**
 * @brief RT state machine instance with thread-safe operations.
 */
typedef struct {
    SM_StateMachine base_sm; /**< Base state machine instance. */
    SM_RT_Statistics stats;  /**< Usage statistics. */
    bool is_initialized;     /**< Initialization status flag. */
    bool is_started;         /**< Started status flag. */
    void* worker_thread;     /**< RT-Thread worker thread handle. */
    void* event_queue;       /**< RT-Thread message queue handle. */
    void* mutex;             /**< RT-Thread mutex for thread safety. */
} SM_RT_Instance;

/* --- Public API --- */

/**
 * @brief Initializes an RT state machine instance.
 * @param[out] rt_sm           Pointer to the RT state machine instance.
 * @param[in]  initial_state    Pointer to the initial state.
 * @param[in]  entry_path_buffer User-provided buffer for transition calculations.
 * @param[in]  buffer_size      Size of the entry path buffer.
 * @param[in]  user_data        Optional pointer to user data.
 * @param[in]  unhandled_hook   Optional hook for unhandled events.
 * @return SM_RT_RESULT_SUCCESS on success, error code otherwise.
 */
SM_RT_Result SM_RT_Init(SM_RT_Instance* rt_sm, const SM_State* initial_state,
                        const SM_State** entry_path_buffer, uint8_t buffer_size, void* user_data,
                        SM_ActionFn unhandled_hook);

/**
 * @brief Starts the RT state machine worker thread.
 * @param[in,out] rt_sm Pointer to the RT state machine instance.
 * @return SM_RT_RESULT_SUCCESS on success, error code otherwise.
 */
SM_RT_Result SM_RT_Start(SM_RT_Instance* rt_sm);

/**
 * @brief Stops the RT state machine worker thread.
 * @param[in,out] rt_sm Pointer to the RT state machine instance.
 * @return SM_RT_RESULT_SUCCESS on success, error code otherwise.
 */
SM_RT_Result SM_RT_Stop(SM_RT_Instance* rt_sm);

/**
 * @brief Posts an event to the RT state machine queue.
 * @param[in,out] rt_sm Pointer to the RT state machine instance.
 * @param[in]     event  Pointer to the event to post.
 * @return SM_RT_RESULT_SUCCESS on success, error code otherwise.
 */
SM_RT_Result SM_RT_PostEvent(SM_RT_Instance* rt_sm, const SM_Event* event);

/**
 * @brief Posts an event with ID to the RT state machine queue.
 * @param[in,out] rt_sm    Pointer to the RT state machine instance.
 * @param[in]     event_id  Event identifier.
 * @param[in]     context   Optional event context data.
 * @return SM_RT_RESULT_SUCCESS on success, error code otherwise.
 */
SM_RT_Result SM_RT_PostEventId(SM_RT_Instance* rt_sm, uint32_t event_id, void* context);

/**
 * @brief Resets the RT state machine to its initial state.
 * @param[in,out] rt_sm Pointer to the RT state machine instance.
 * @return SM_RT_RESULT_SUCCESS on success, error code otherwise.
 */
SM_RT_Result SM_RT_Reset(SM_RT_Instance* rt_sm);

/**
 * @brief Checks if the current state is a specific state or substate.
 * @param[in]  rt_sm      Pointer to the RT state machine instance.
 * @param[in]  state       The state to check against.
 * @param[out] is_in_state Pointer to store the result.
 * @return SM_RT_RESULT_SUCCESS on success, error code otherwise.
 */
SM_RT_Result SM_RT_IsInState(const SM_RT_Instance* rt_sm, const SM_State* state, bool* is_in_state);

/**
 * @brief Gets the name of the current state.
 * @param[in]  rt_sm     Pointer to the RT state machine instance.
 * @param[out] state_name Pointer to store the state name pointer.
 * @return SM_RT_RESULT_SUCCESS on success, error code otherwise.
 */
SM_RT_Result SM_RT_GetCurrentStateName(const SM_RT_Instance* rt_sm, const char** state_name);

/**
 * @brief Gets the current statistics of the RT state machine.
 * @param[in]  rt_sm Pointer to the RT state machine instance.
 * @param[out] stats  Pointer to store the statistics.
 * @return SM_RT_RESULT_SUCCESS on success, error code otherwise.
 */
SM_RT_Result SM_RT_GetStatistics(const SM_RT_Instance* rt_sm, SM_RT_Statistics* stats);

/**
 * @brief Resets the statistics of the RT state machine.
 * @param[in,out] rt_sm Pointer to the RT state machine instance.
 * @return SM_RT_RESULT_SUCCESS on success, error code otherwise.
 */
SM_RT_Result SM_RT_ResetStatistics(SM_RT_Instance* rt_sm);

#endif /* STATE_MACHINE_RT_H */