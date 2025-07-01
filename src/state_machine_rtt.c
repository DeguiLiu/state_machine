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

/* Include RT-Thread APIs - in real RT-Thread environment, this would be rtthread.h */
#ifdef RT_THREAD_MOCK
#include "rt_thread_mock.h"
#else
#include <rtthread.h>
#endif

/* --- Private Helper Function Declarations --- */
static void worker_entry(void *parameter);
static SM_RTT_Result dispatch_event_safe(SM_RTT_Instance *rtt_sm, const SM_Event *event);
static SM_RTT_Result update_queue_stats(SM_RTT_Instance *rtt_sm);
static SM_RTT_Result cleanup_resources(SM_RTT_Instance *rtt_sm);

/* --- Public API Implementation --- */

SM_RTT_Result SM_RTT_Init(SM_RTT_Instance *rtt_sm,
                          const SM_RTT_Config *config,
                          const SM_State *initial_state,
                          const SM_State **entry_path_buffer,
                          uint8_t buffer_size,
                          void *user_data,
                          SM_ActionFn unhandled_hook)
{
    SM_RTT_Result result = SM_RTT_RESULT_ERROR_UNKNOWN;
    bool validation_passed = false;
    
    /* Initialize all local variables */
    do {
        /* Parameter validation */
        if (rtt_sm == NULL) {
            result = SM_RTT_RESULT_ERROR_NULL_PTR;
            break;
        }
        
        if (config == NULL) {
            result = SM_RTT_RESULT_ERROR_NULL_PTR;
            break;
        }
        
        if (initial_state == NULL) {
            result = SM_RTT_RESULT_ERROR_NULL_PTR;
            break;
        }
        
        if (entry_path_buffer == NULL) {
            result = SM_RTT_RESULT_ERROR_NULL_PTR;
            break;
        }
        
        if (buffer_size == 0U) {
            result = SM_RTT_RESULT_ERROR_INVALID;
            break;
        }
        
        if (config->queue_size == 0U) {
            result = SM_RTT_RESULT_ERROR_INVALID;
            break;
        }
        
        if (rtt_sm->is_initialized) {
            result = SM_RTT_RESULT_ERROR_ALREADY_INIT;
            break;
        }
        
        validation_passed = true;
        
    } while (false);
    
    if (validation_passed) {
        /* Initialize the base state machine */
        SM_Init(&rtt_sm->base_sm, initial_state, entry_path_buffer, 
                buffer_size, user_data, unhandled_hook);
        
        /* Copy configuration */
        rtt_sm->config = *config;
        
        /* Initialize RTT-specific fields */
        rtt_sm->stats.total_events_processed = 0U;
        rtt_sm->stats.total_events_unhandled = 0U;
        rtt_sm->stats.total_transitions = 0U;
        rtt_sm->stats.current_queue_depth = 0U;
        rtt_sm->stats.max_queue_depth = 0U;
        rtt_sm->is_initialized = true;
        rtt_sm->is_started = false;
        rtt_sm->stop_requested = false;
        rtt_sm->worker_thread = NULL;
        rtt_sm->event_queue = NULL;
        rtt_sm->mutex = NULL;
        
        result = SM_RTT_RESULT_SUCCESS;
    }
    
    return result;
}

SM_RTT_Result SM_RTT_Deinit(SM_RTT_Instance *rtt_sm)
{
    SM_RTT_Result result = SM_RTT_RESULT_ERROR_UNKNOWN;
    bool validation_passed = false;
    
    do {
        /* Parameter validation */
        if (rtt_sm == NULL) {
            result = SM_RTT_RESULT_ERROR_NULL_PTR;
            break;
        }
        
        if (!rtt_sm->is_initialized) {
            result = SM_RTT_RESULT_ERROR_NOT_INIT;
            break;
        }
        
        validation_passed = true;
        
    } while (false);
    
    if (validation_passed) {
        /* Stop if started */
        if (rtt_sm->is_started) {
            SM_RTT_Stop(rtt_sm);
        }
        
        /* Cleanup resources */
        cleanup_resources(rtt_sm);
        
        /* Deinitialize base state machine */
        SM_Deinit(&rtt_sm->base_sm);
        
        /* Mark as uninitialized */
        rtt_sm->is_initialized = false;
        result = SM_RTT_RESULT_SUCCESS;
    }
    
    return result;
}

SM_RTT_Result SM_RTT_Start(SM_RTT_Instance *rtt_sm)
{
    SM_RTT_Result result = SM_RTT_RESULT_ERROR_UNKNOWN;
    bool validation_passed = false;
    bool resources_created = false;
    
    do {
        /* Parameter validation */
        if (rtt_sm == NULL) {
            result = SM_RTT_RESULT_ERROR_NULL_PTR;
            break;
        }
        
        if (!rtt_sm->is_initialized) {
            result = SM_RTT_RESULT_ERROR_NOT_INIT;
            break;
        }
        
        if (rtt_sm->is_started) {
            result = SM_RTT_RESULT_ERROR_ALREADY_STARTED;
            break;
        }
        
        validation_passed = true;
        
    } while (false);
    
    if (validation_passed) {
        /* Create mutex */
        rtt_sm->mutex = rt_mutex_create(rtt_sm->config.mutex_name, 0);
        if (rtt_sm->mutex == NULL) {
            result = SM_RTT_RESULT_ERROR_UNKNOWN;
        } else {
            /* Create message queue */
            rtt_sm->event_queue = rt_mq_create(rtt_sm->config.queue_name,
                                               sizeof(SM_Event),
                                               rtt_sm->config.queue_size,
                                               RT_IPC_FLAG_FIFO);
            if (rtt_sm->event_queue == NULL) {
                rt_mutex_delete(rtt_sm->mutex);
                rtt_sm->mutex = NULL;
                result = SM_RTT_RESULT_ERROR_UNKNOWN;
            } else {
                /* Create worker thread */
                rtt_sm->worker_thread = rt_thread_create(rtt_sm->config.thread_name,
                                                         worker_entry,
                                                         rtt_sm,
                                                         rtt_sm->config.thread_stack_size,
                                                         rtt_sm->config.thread_priority,
                                                         rtt_sm->config.thread_timeslice);
                if (rtt_sm->worker_thread == NULL) {
                    rt_mq_delete(rtt_sm->event_queue);
                    rt_mutex_delete(rtt_sm->mutex);
                    rtt_sm->event_queue = NULL;
                    rtt_sm->mutex = NULL;
                    result = SM_RTT_RESULT_ERROR_UNKNOWN;
                } else {
                    resources_created = true;
                }
            }
        }
        
        if (resources_created) {
            /* Start worker thread */
            rtt_sm->stop_requested = false;
            if (rt_thread_startup(rtt_sm->worker_thread) == RT_EOK) {
                rtt_sm->is_started = true;
                result = SM_RTT_RESULT_SUCCESS;
            } else {
                /* Cleanup on failure */
                rt_thread_delete(rtt_sm->worker_thread);
                rt_mq_delete(rtt_sm->event_queue);
                rt_mutex_delete(rtt_sm->mutex);
                rtt_sm->worker_thread = NULL;
                rtt_sm->event_queue = NULL;
                rtt_sm->mutex = NULL;
                result = SM_RTT_RESULT_ERROR_UNKNOWN;
            }
        }
    }
    
    return result;
}

SM_RTT_Result SM_RTT_Stop(SM_RTT_Instance *rtt_sm)
{
    SM_RTT_Result result = SM_RTT_RESULT_ERROR_UNKNOWN;
    bool validation_passed = false;
    
    do {
        /* Parameter validation */
        if (rtt_sm == NULL) {
            result = SM_RTT_RESULT_ERROR_NULL_PTR;
            break;
        }
        
        if (!rtt_sm->is_initialized) {
            result = SM_RTT_RESULT_ERROR_NOT_INIT;
            break;
        }
        
        if (!rtt_sm->is_started) {
            result = SM_RTT_RESULT_ERROR_NOT_STARTED;
            break;
        }
        
        validation_passed = true;
        
    } while (false);
    
    if (validation_passed) {
        /* Signal worker thread to stop */
        rtt_sm->stop_requested = true;
        
        /* Send a dummy event to wake up the worker thread */
        SM_Event stop_event = {UINT32_MAX, NULL};
        rt_mq_send(rtt_sm->event_queue, &stop_event, sizeof(SM_Event));
        
        /* Wait for and cleanup thread */
        if (rtt_sm->worker_thread != NULL) {
            rt_thread_delete(rtt_sm->worker_thread);
            rtt_sm->worker_thread = NULL;
        }
        
        /* Cleanup other resources */
        cleanup_resources(rtt_sm);
        
        rtt_sm->is_started = false;
        result = SM_RTT_RESULT_SUCCESS;
    }
    
    return result;
}

SM_RTT_Result SM_RTT_DispatchSync(SM_RTT_Instance *rtt_sm, const SM_Event *event)
{
    SM_RTT_Result result = SM_RTT_RESULT_ERROR_UNKNOWN;
    bool validation_passed = false;
    
    do {
        /* Parameter validation */
        if (rtt_sm == NULL) {
            result = SM_RTT_RESULT_ERROR_NULL_PTR;
            break;
        }
        
        if (event == NULL) {
            result = SM_RTT_RESULT_ERROR_NULL_PTR;
            break;
        }
        
        if (!rtt_sm->is_initialized) {
            result = SM_RTT_RESULT_ERROR_NOT_INIT;
            break;
        }
        
        validation_passed = true;
        
    } while (false);
    
    if (validation_passed) {
        /* Dispatch event synchronously with mutex protection */
        result = dispatch_event_safe(rtt_sm, event);
    }
    
    return result;
}

SM_RTT_Result SM_RTT_PostEvent(SM_RTT_Instance *rtt_sm, const SM_Event *event)
{
    SM_RTT_Result result = SM_RTT_RESULT_ERROR_UNKNOWN;
    bool validation_passed = false;
    rt_err_t mq_result = RT_ERROR;
    
    do {
        /* Parameter validation */
        if (rtt_sm == NULL) {
            result = SM_RTT_RESULT_ERROR_NULL_PTR;
            break;
        }
        
        if (event == NULL) {
            result = SM_RTT_RESULT_ERROR_NULL_PTR;
            break;
        }
        
        if (!rtt_sm->is_initialized) {
            result = SM_RTT_RESULT_ERROR_NOT_INIT;
            break;
        }
        
        if (!rtt_sm->is_started) {
            result = SM_RTT_RESULT_ERROR_NOT_STARTED;
            break;
        }
        
        validation_passed = true;
        
    } while (false);
    
    if (validation_passed) {
        /* Post event to message queue */
        mq_result = rt_mq_send(rtt_sm->event_queue, (void*)event, sizeof(SM_Event));
        
        if (mq_result == RT_EOK) {
            /* Update queue statistics */
            update_queue_stats(rtt_sm);
            result = SM_RTT_RESULT_SUCCESS;
        } else if (mq_result == RT_EFULL) {
            result = SM_RTT_RESULT_ERROR_QUEUE_FULL;
        } else {
            result = SM_RTT_RESULT_ERROR_UNKNOWN;
        }
    }
    
    return result;
}

SM_RTT_Result SM_RTT_PostEventId(SM_RTT_Instance *rtt_sm, uint32_t event_id, void *context)
{
    SM_RTT_Result result = SM_RTT_RESULT_ERROR_UNKNOWN;
    bool validation_passed = false;
    SM_Event event_to_post = {0U, NULL};
    
    do {
        /* Parameter validation */
        if (rtt_sm == NULL) {
            result = SM_RTT_RESULT_ERROR_NULL_PTR;
            break;
        }
        
        if (!rtt_sm->is_initialized) {
            result = SM_RTT_RESULT_ERROR_NOT_INIT;
            break;
        }
        
        if (!rtt_sm->is_started) {
            result = SM_RTT_RESULT_ERROR_NOT_STARTED;
            break;
        }
        
        validation_passed = true;
        
    } while (false);
    
    if (validation_passed) {
        /* Initialize event structure */
        event_to_post.id = event_id;
        event_to_post.context = context;
        
        /* Post the event */
        result = SM_RTT_PostEvent(rtt_sm, &event_to_post);
    }
    
    return result;
}

SM_RTT_Result SM_RTT_Reset(SM_RTT_Instance *rtt_sm)
{
    SM_RTT_Result result = SM_RTT_RESULT_ERROR_UNKNOWN;
    bool validation_passed = false;
    
    do {
        /* Parameter validation */
        if (rtt_sm == NULL) {
            result = SM_RTT_RESULT_ERROR_NULL_PTR;
            break;
        }
        
        if (!rtt_sm->is_initialized) {
            result = SM_RTT_RESULT_ERROR_NOT_INIT;
            break;
        }
        
        validation_passed = true;
        
    } while (false);
    
    if (validation_passed) {
        /* Reset the base state machine */
        SM_Reset(&rtt_sm->base_sm);
        rtt_sm->stats.total_transitions++;
        result = SM_RTT_RESULT_SUCCESS;
    }
    
    return result;
}

SM_RTT_Result SM_RTT_IsInState(const SM_RTT_Instance *rtt_sm, 
                               const SM_State *state, 
                               bool *is_in_state)
{
    SM_RTT_Result result = SM_RTT_RESULT_ERROR_UNKNOWN;
    bool validation_passed = false;
    
    do {
        /* Parameter validation */
        if (rtt_sm == NULL) {
            result = SM_RTT_RESULT_ERROR_NULL_PTR;
            break;
        }
        
        if (state == NULL) {
            result = SM_RTT_RESULT_ERROR_NULL_PTR;
            break;
        }
        
        if (is_in_state == NULL) {
            result = SM_RTT_RESULT_ERROR_NULL_PTR;
            break;
        }
        
        if (!rtt_sm->is_initialized) {
            result = SM_RTT_RESULT_ERROR_NOT_INIT;
            break;
        }
        
        validation_passed = true;
        
    } while (false);
    
    if (validation_passed) {
        /* Use the base state machine function */
        *is_in_state = SM_IsInState(&rtt_sm->base_sm, state);
        result = SM_RTT_RESULT_SUCCESS;
    } else {
        /* Ensure output parameter is set even on error */
        if (is_in_state != NULL) {
            *is_in_state = false;
        }
    }
    
    return result;
}

SM_RTT_Result SM_RTT_GetCurrentStateName(const SM_RTT_Instance *rtt_sm, 
                                         const char **state_name)
{
    SM_RTT_Result result = SM_RTT_RESULT_ERROR_UNKNOWN;
    bool validation_passed = false;
    
    do {
        /* Parameter validation */
        if (rtt_sm == NULL) {
            result = SM_RTT_RESULT_ERROR_NULL_PTR;
            break;
        }
        
        if (state_name == NULL) {
            result = SM_RTT_RESULT_ERROR_NULL_PTR;
            break;
        }
        
        if (!rtt_sm->is_initialized) {
            result = SM_RTT_RESULT_ERROR_NOT_INIT;
            break;
        }
        
        validation_passed = true;
        
    } while (false);
    
    if (validation_passed) {
        /* Use the base state machine function */
        *state_name = SM_GetCurrentStateName(&rtt_sm->base_sm);
        result = SM_RTT_RESULT_SUCCESS;
    } else {
        /* Ensure output parameter is set even on error */
        if (state_name != NULL) {
            *state_name = "Unknown";
        }
    }
    
    return result;
}

SM_RTT_Result SM_RTT_GetStatistics(const SM_RTT_Instance *rtt_sm, 
                                   SM_RTT_Statistics *stats)
{
    SM_RTT_Result result = SM_RTT_RESULT_ERROR_UNKNOWN;
    bool validation_passed = false;
    rt_err_t mutex_result = RT_ERROR;
    
    do {
        /* Parameter validation */
        if (rtt_sm == NULL) {
            result = SM_RTT_RESULT_ERROR_NULL_PTR;
            break;
        }
        
        if (stats == NULL) {
            result = SM_RTT_RESULT_ERROR_NULL_PTR;
            break;
        }
        
        if (!rtt_sm->is_initialized) {
            result = SM_RTT_RESULT_ERROR_NOT_INIT;
            break;
        }
        
        validation_passed = true;
        
    } while (false);
    
    if (validation_passed) {
        /* Acquire mutex for thread-safe access to statistics */
        if (rtt_sm->mutex != NULL) {
            mutex_result = rt_mutex_take(rtt_sm->mutex, 1000); /* 1 second timeout */
        } else {
            mutex_result = RT_EOK; /* No mutex available, proceed without protection */
        }
        
        if (mutex_result == RT_EOK) {
            /* Copy statistics structure */
            stats->total_events_processed = rtt_sm->stats.total_events_processed;
            stats->total_events_unhandled = rtt_sm->stats.total_events_unhandled;
            stats->total_transitions = rtt_sm->stats.total_transitions;
            stats->current_queue_depth = rtt_sm->stats.current_queue_depth;
            stats->max_queue_depth = rtt_sm->stats.max_queue_depth;
            
            /* Release mutex */
            if (rtt_sm->mutex != NULL) {
                rt_mutex_release(rtt_sm->mutex);
            }
            
            result = SM_RTT_RESULT_SUCCESS;
        } else {
            result = SM_RTT_RESULT_ERROR_UNKNOWN;
        }
    }
    
    if (!validation_passed || (mutex_result != RT_EOK)) {
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

SM_RTT_Result SM_RTT_ResetStatistics(SM_RTT_Instance *rtt_sm)
{
    SM_RTT_Result result = SM_RTT_RESULT_ERROR_UNKNOWN;
    bool validation_passed = false;
    
    do {
        /* Parameter validation */
        if (rtt_sm == NULL) {
            result = SM_RTT_RESULT_ERROR_NULL_PTR;
            break;
        }
        
        if (!rtt_sm->is_initialized) {
            result = SM_RTT_RESULT_ERROR_NOT_INIT;
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
        result = SM_RTT_RESULT_SUCCESS;
    }
    
    return result;
}

/* --- Private Helper Function Implementations --- */

static void worker_entry(void *parameter)
{
    SM_RTT_Instance *rtt_sm = NULL;
    bool valid_parameter = false;
    SM_Event event_buffer = {0U, NULL};
    rt_err_t recv_result = RT_ERROR;
    
    /* Initialize local variables */
    rtt_sm = (SM_RTT_Instance *)parameter;
    
    /* Parameter validation */
    if (rtt_sm != NULL) {
        valid_parameter = true;
    }
    
    if (valid_parameter) {
        /* Worker thread main loop */
        while (!rtt_sm->stop_requested) {
            /* Wait for events from the message queue */
            recv_result = rt_mq_recv(rtt_sm->event_queue, &event_buffer, 
                                     sizeof(SM_Event), RT_WAITING_FOREVER);
            
            if (recv_result == RT_EOK) {
                /* Check if this is a stop signal */
                if (rtt_sm->stop_requested || event_buffer.id == UINT32_MAX) {
                    break;
                }
                
                /* Process the event */
                dispatch_event_safe(rtt_sm, &event_buffer);
                
                /* Update queue statistics */
                update_queue_stats(rtt_sm);
            }
        }
    }
    
    /* Single return point */
    return;
}

static SM_RTT_Result dispatch_event_safe(SM_RTT_Instance *rtt_sm, const SM_Event *event)
{
    SM_RTT_Result result = SM_RTT_RESULT_ERROR_UNKNOWN;
    bool validation_passed = false;
    bool event_handled = false;
    rt_err_t mutex_result = RT_ERROR;
    const SM_State *prev_state = NULL;
    
    do {
        /* Parameter validation */
        if (rtt_sm == NULL) {
            result = SM_RTT_RESULT_ERROR_NULL_PTR;
            break;
        }
        
        if (event == NULL) {
            result = SM_RTT_RESULT_ERROR_NULL_PTR;
            break;
        }
        
        validation_passed = true;
        
    } while (false);
    
    if (validation_passed) {
        /* Acquire mutex for thread-safe access */
        if (rtt_sm->mutex != NULL) {
            mutex_result = rt_mutex_take(rtt_sm->mutex, RT_WAITING_FOREVER);
        } else {
            mutex_result = RT_EOK; /* No mutex available, proceed without protection */
        }
        
        if (mutex_result == RT_EOK) {
            /* Store previous state to detect transitions */
            prev_state = rtt_sm->base_sm.currentState;
            
            /* Dispatch event to base state machine */
            event_handled = SM_Dispatch(&rtt_sm->base_sm, event);
            
            /* Check if state changed (transition occurred) */
            if (prev_state != rtt_sm->base_sm.currentState) {
                rtt_sm->stats.total_transitions++;
            }
            
            /* Update statistics */
            rtt_sm->stats.total_events_processed++;
            if (!event_handled) {
                rtt_sm->stats.total_events_unhandled++;
            }
            
            /* Release mutex */
            if (rtt_sm->mutex != NULL) {
                rt_mutex_release(rtt_sm->mutex);
            }
            
            result = SM_RTT_RESULT_SUCCESS;
        } else {
            result = SM_RTT_RESULT_ERROR_UNKNOWN;
        }
    }
    
    return result;
}

static SM_RTT_Result update_queue_stats(SM_RTT_Instance *rtt_sm)
{
    SM_RTT_Result result = SM_RTT_RESULT_ERROR_UNKNOWN;
    
    if (rtt_sm != NULL && rtt_sm->event_queue != NULL) {
        /* Thread-safe access to queue statistics */
        if (rtt_sm->mutex == NULL || rt_mutex_take(rtt_sm->mutex, 100) == RT_EOK) {
            #ifdef RT_THREAD_MOCK
            /* For RT-Thread mock, use the queue count function */
            rtt_sm->stats.current_queue_depth = (uint32_t)rt_mq_get_count(rtt_sm->event_queue);
            #else
            /* For real RT-Thread, would use appropriate RT-Thread queue query API */
            /* This would be implementation-specific based on RT-Thread version */
            #endif
            
            if (rtt_sm->stats.current_queue_depth > rtt_sm->stats.max_queue_depth) {
                rtt_sm->stats.max_queue_depth = rtt_sm->stats.current_queue_depth;
            }
            
            if (rtt_sm->mutex != NULL) {
                rt_mutex_release(rtt_sm->mutex);
            }
            result = SM_RTT_RESULT_SUCCESS;
        }
    }
    
    return result;
}

static SM_RTT_Result cleanup_resources(SM_RTT_Instance *rtt_sm)
{
    SM_RTT_Result result = SM_RTT_RESULT_ERROR_UNKNOWN;
    
    if (rtt_sm != NULL) {
        /* Cleanup message queue */
        if (rtt_sm->event_queue != NULL) {
            rt_mq_delete(rtt_sm->event_queue);
            rtt_sm->event_queue = NULL;
        }
        
        /* Cleanup mutex */
        if (rtt_sm->mutex != NULL) {
            rt_mutex_delete(rtt_sm->mutex);
            rtt_sm->mutex = NULL;
        }
        
        result = SM_RTT_RESULT_SUCCESS;
    }
    
    return result;
}

