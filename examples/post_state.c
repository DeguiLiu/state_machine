/*
 * Copyright (c) 2019, redoc
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2021-01-12     redoc        the first version
 */

#define LOG_TAG      "state.post"
#define LOG_LVL      LOG_LVL_DBG

#include <rtthread.h>
#include "state_machine.h"
#include <ulog.h>
#include <stdlib.h>

/*  post state graph
 *
 *
 * +-----------------------------------------------------------------+
 * |                   root--------+                                 |
 * |                               |                                 |
 * |                               |                                 |
 * |  +----------+  fail      +----------+  pass     +----------+    |
 * |  | postfail | <--------  |   post   | --------> | postpass |    |
 * |  +----------+            +----------+           +----------+    |
 * |                              ^    \post                         |
 * |                          post|     \break                       |
 * |                         break|      \on                         |
 * |                           off|       \---->  +-----------+      |
 * |                              +-------------  | postbreak |      |
 * |                                              +-----------+      |
 * |                                                                 |
 * +-----------------------------------------------------------------+
 *
 */
/*
    主要修改点：

    - 用 SM_State、SM_Transition、SM_Event、SM_StateMachine 替换原有结构体。
    - 状态定义、转换表、事件定义、线程/消息队列管理方式与 test_state_machine.c 保持一致。
    - 入口、出口、动作、守卫函数签名全部适配新版。
    - 事件投递、状态机初始化、线程入口等全部参考 test_state_machine.c。
*/

// --- 1. 定义事件 ---
enum
{
    EV_POST_START,
    EV_POST_BREAKON,
    EV_POST_BREAKOFF,
    EV_POST_ANSWER,
    EV_POST_NUMS
};

// --- 2. 状态前置声明 ---
static const SM_State STATE_Root;
static const SM_State STATE_Post;
static const SM_State STATE_PostPass;
static const SM_State STATE_PostFail;
static const SM_State STATE_PostBreak;

// --- 3. 用户数据结构 ---
typedef struct
{
    int answer;
} PostData;

// --- 4. 动作与守卫函数 ---
static void entry_print(SM_StateMachine *sm, const SM_Event *event)
{
    log_i("Entering %s state", SM_GetCurrentStateName(sm));
}
static void exit_print(SM_StateMachine *sm, const SM_Event *event)
{
    log_i("Exiting %s state", SM_GetCurrentStateName(sm));
}
static void entry_post(SM_StateMachine *sm, const SM_Event *event)
{
    log_i("Entering POST state");
    log_i("post start...");
}
static void action_post_break(SM_StateMachine *sm, const SM_Event *event)
{
    log_i("post break, display break...");
}
static void action_post_pass(SM_StateMachine *sm, const SM_Event *event)
{
    log_i("post pass, display pass...");
}
static void action_post_fail(SM_StateMachine *sm, const SM_Event *event)
{
    log_i("post fail, display fail...");
}
static bool guard_post_pass(SM_StateMachine *sm, const SM_Event *event)
{
    // event->context 指向 int*，与转换表 condition 比较
    if (event->id != EV_POST_ANSWER) return false;
    int expected = 2;
    int actual = event->context ? *(int*)event->context : 0;
    return actual == expected;
}
static bool guard_post_fail(SM_StateMachine *sm, const SM_Event *event)
{
    if (event->id != EV_POST_ANSWER) return false;
    int expected = 1;
    int actual = event->context ? *(int*)event->context : 0;
    return actual == expected;
}

// --- 5. 转换表 ---
static const SM_Transition T_Root[] = {
    {EV_POST_START, &STATE_Post, NULL, NULL, SM_TRANSITION_EXTERNAL}
};
static const SM_Transition T_Post[] = {
    {EV_POST_BREAKON, &STATE_PostBreak, NULL, action_post_break, SM_TRANSITION_EXTERNAL},
    {EV_POST_ANSWER, &STATE_PostFail, guard_post_fail, action_post_fail, SM_TRANSITION_EXTERNAL},
    {EV_POST_ANSWER, &STATE_PostPass, guard_post_pass, action_post_pass, SM_TRANSITION_EXTERNAL}
};
static const SM_Transition T_PostBreak[] = {
    {EV_POST_BREAKOFF, &STATE_Post, NULL, NULL, SM_TRANSITION_EXTERNAL}
};

// --- 6. 状态定义 ---
static const SM_State STATE_Root = {
    .parent = NULL,
    .entryAction = entry_print,
    .exitAction = exit_print,
    .transitions = T_Root,
    .numTransitions = sizeof(T_Root) / sizeof(T_Root[0]),
    .name = "ROOT"
};
static const SM_State STATE_Post = {
    .parent = NULL,
    .entryAction = entry_post,
    .exitAction = exit_print,
    .transitions = T_Post,
    .numTransitions = sizeof(T_Post) / sizeof(T_Post[0]),
    .name = "POST"
};
static const SM_State STATE_PostPass = {
    .parent = NULL,
    .entryAction = entry_print,
    .exitAction = exit_print,
    .transitions = NULL,
    .numTransitions = 0,
    .name = "POSTPASS"
};
static const SM_State STATE_PostFail = {
    .parent = NULL,
    .entryAction = entry_print,
    .exitAction = exit_print,
    .transitions = NULL,
    .numTransitions = 0,
    .name = "POSTFAIL"
};
static const SM_State STATE_PostBreak = {
    .parent = NULL,
    .entryAction = entry_print,
    .exitAction = exit_print,
    .transitions = T_PostBreak,
    .numTransitions = sizeof(T_PostBreak) / sizeof(T_PostBreak[0]),
    .name = "POSTBREAK"
};

// --- 7. RT-Thread 相关定义 ---
#define SM_THREAD_PRIORITY   15
#define SM_THREAD_STACK_SIZE 1024
#define SM_THREAD_TIMESLICE  10
#define SM_MQ_MAX_MSGS 10

static rt_thread_t sm_thread;
static rt_mq_t     sm_mq;
static SM_StateMachine post_sm;
static PostData post_data;

#define MAX_STATE_DEPTH 8
static const SM_State *sm_entry_path_buffer[MAX_STATE_DEPTH];

// --- 8. 未处理事件钩子 ---
static void on_unhandled_event(SM_StateMachine *sm, const SM_Event *event)
{
    log_e("--- Unhandled Event: Event %u received in state '%s' ---",
          (unsigned)event->id, SM_GetCurrentStateName(sm));
}

// --- 9. 状态机线程入口 ---
static void state_machine_thread_entry(void* parameter)
{
    SM_Init(&post_sm, &STATE_Root, sm_entry_path_buffer, MAX_STATE_DEPTH, &post_data, on_unhandled_event);
    log_i("State machine initialized. Initial State: %s", SM_GetCurrentStateName(&post_sm));

    while (1)
    {
        SM_Event event_buffer;
        rt_err_t result = rt_mq_recv(sm_mq, &event_buffer, sizeof(SM_Event), RT_WAITING_FOREVER);

        if (result == RT_EOK)
        {
            log_i("\n--- Event received: %d, dispatching to state machine ---", event_buffer.id);
            bool handled = SM_Dispatch(&post_sm, &event_buffer);
            if (!handled) {
                log_w("Event %d was not handled.", event_buffer.id);
            }
            log_i("Current State: %s", SM_GetCurrentStateName(&post_sm));
        }
    }
}

// --- 10. 外部事件投递接口 ---
rt_err_t post_event_to_sm(uint32_t event_id, void* context)
{
    SM_Event event_to_send;
    event_to_send.id = event_id;
    event_to_send.context = context;
    return rt_mq_send_wait(sm_mq, &event_to_send, sizeof(SM_Event), 0);
}

// --- 11. 初始化函数 ---
int post_sm_init(void)
{
    static struct rt_messagequeue static_sm_mq;
    static rt_uint8_t sm_mq_pool[SM_MQ_MAX_MSGS * sizeof(SM_Event)];

    sm_mq = (rt_mq_t)&static_sm_mq;
    rt_err_t result = rt_mq_init(sm_mq,
                                 "post_mq",
                                 &sm_mq_pool[0],
                                 sizeof(SM_Event),
                                 sizeof(sm_mq_pool),
                                 RT_IPC_FLAG_FIFO);
    if (result != RT_EOK)
    {
        log_e("Failed to initialize post state machine message queue.");
        return -1;
    }

    static struct rt_thread static_sm_thread;
    static rt_uint8_t sm_thread_stack[SM_THREAD_STACK_SIZE];

    sm_thread = &static_sm_thread;
    rt_thread_init(sm_thread,
                   "post_sm_thread",
                   state_machine_thread_entry,
                   RT_NULL,
                   &sm_thread_stack[0],
                   sizeof(sm_thread_stack),
                   SM_THREAD_PRIORITY,
                   SM_THREAD_TIMESLICE);

    rt_thread_startup(sm_thread);

    return 0;
}
INIT_APP_EXPORT(post_sm_init);

// --- 12. 示例命令行接口 ---
#ifdef FINSH_USING_MSH
#include <finsh.h>
static void post_event_set(int argc, char **argv)
{
    if (argc < 2)
    {
        rt_kprintf("Usage: post_event_set <event> [data]\n");
        return;
    }
    uint32_t event = 0;
    void *context = NULL;
    if (!rt_strcmp(argv[1], "start"))
        event = EV_POST_START;
    else if (!rt_strcmp(argv[1], "breakon"))
        event = EV_POST_BREAKON;
    else if (!rt_strcmp(argv[1], "breakoff"))
        event = EV_POST_BREAKOFF;
    else if (!rt_strcmp(argv[1], "answer"))
    {
        event = EV_POST_ANSWER;
        static int answer_val;
        answer_val = (argc >= 3) ? atoi(argv[2]) : 0;
        context = &answer_val;
    }
    else
    {
        rt_kprintf("Unknown event: %s\n", argv[1]);
        return;
    }
    post_event_to_sm(event, context);
}
MSH_CMD_EXPORT(post_event_set, post_event_set <event> [data]);

static void post_current_get(int argc, char **argv)
{
    rt_kprintf("post current state is %s\n", SM_GetCurrentStateName(&post_sm));
}
MSH_CMD_EXPORT(post_current_get, get current state);
#endif
