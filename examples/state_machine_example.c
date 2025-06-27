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

#include <stdint.h>
#include <stdio.h>
#include "state_machine.h"
#include "rtthread.h"

/* This simple example checks keyboad input against the two allowed strings
 * "han" and "hin". If an unrecognised character is read, a group state will
 * handle this by printing a message and returning to the idle state. If the
 * character '!' is encountered, a "reset" message is printed, and the group
 * state's entry state will be entered (the idle state).
 *
 *                   print 'reset'
 *       o      +---------------------+
 *       |      |                     | '!'
 *       |      v     group state     |
 * +-----v-----------------------------------+----+
 * |  +------+  'h'  +---+  'a'  +---+  'n'      |
 * +->| idle | ----> | h | ----> | a | ---------+ |
 * |  +------+       +---+\      +---+          | |
 * |   ^ ^ ^               \'i'  +---+  'n'    | |
 * |   | | |                \--> | i | ------+  | |
 * |   | | |                     +---+       |  | |
 * +---|-|-|----------------+----------------|--|-+
 *     | | |                |                |  |
 *     | | |                | '[^hai!\n]'    |  |
 *     | | | print unrecog. |                |  |
 *     | | +----------------+   print 'hi'   |  |
 *     | +-----------------------------------+  |
 *     |               print 'ha'               |
 *     +----------------------------------------+
 */
/*
    修改记录
    - 所有状态、转换、动作、守卫均已适配修改后的接口。
    - 事件通过 context 传递字符，id 固定为 EVENT_KEYBOARD。
    - 修改线程、消息队列、命令行接口风格。
*/

/* 事件类型定义 */
enum
{
    EVENT_KEYBOARD = 1,
};

/* --- 1. 动作与守卫函数 --- */
static void entry_print(SM_StateMachine *sm, const SM_Event *event)
{
    rt_kprintf("Entering %s state\n", SM_GetCurrentStateName(sm));
}
static void exit_print(SM_StateMachine *sm, const SM_Event *event)
{
    rt_kprintf("Exiting %s state\n", SM_GetCurrentStateName(sm));
}
static void print_msg_recognised_char(SM_StateMachine *sm, const SM_Event *event)
{
    entry_print(sm, event);
    rt_kprintf("parsed: %c\n", (char)(intptr_t)event->context);
}
static void print_msg_unrecognised_char(SM_StateMachine *sm, const SM_Event *event)
{
    rt_kprintf("unrecognised character: %c\n", (char)(intptr_t)event->context);
}
static void print_msg_reset(SM_StateMachine *sm, const SM_Event *event)
{
    rt_kprintf("Resetting\n");
}
static void print_msg_hi(SM_StateMachine *sm, const SM_Event *event)
{
    rt_kprintf("Hi!\n");
}
static void print_msg_ha(SM_StateMachine *sm, const SM_Event *event)
{
    rt_kprintf("Ha-ha\n");
}

/* --- 2. 守卫函数 --- */
static bool keyboard_char_compare(SM_StateMachine *sm, const SM_Event *event)
{
    // 用 transition 的 guard 时，event->context 是输入的字符
    // guard 由 transition 传入的 condition 字符与 event->context 比较
    // 这里 condition 通过 transition 的 context 字段传递
    // 但改进后的 state_machine.h 没有 transition 的 context，需用多个 transition 匹配不同字符
    // 所以 guard 只需返回 true
    return true;
}

/* --- 3. 状态前置声明 --- */
static const SM_State STATE_Group;
static const SM_State STATE_Idle;
static const SM_State STATE_H;
static const SM_State STATE_I;
static const SM_State STATE_A;

/* --- 4. 转换表 --- */
static const SM_Transition T_Group[] = {
    // '!' 重置
    {EVENT_KEYBOARD, &STATE_Idle,
        // guard
        [](SM_StateMachine *sm, const SM_Event *event) { return (char)(intptr_t)event->context == '!'; },
        print_msg_reset, SM_TRANSITION_EXTERNAL},
    // 其它未识别字符
    {EVENT_KEYBOARD, &STATE_Idle,
        [](SM_StateMachine *sm, const SM_Event *event) {
            char ch = (char)(intptr_t)event->context;
            return ch != 'h' && ch != 'a' && ch != 'i' && ch != 'n' && ch != '!';
        },
        print_msg_unrecognised_char, SM_TRANSITION_EXTERNAL},
};

static const SM_Transition T_Idle[] = {
    // 'h' -> H
    {EVENT_KEYBOARD, &STATE_H,
        [](SM_StateMachine *sm, const SM_Event *event) { return (char)(intptr_t)event->context == 'h'; },
        NULL, SM_TRANSITION_EXTERNAL},
};

static const SM_Transition T_H[] = {
    // 'a' -> A
    {EVENT_KEYBOARD, &STATE_A,
        [](SM_StateMachine *sm, const SM_Event *event) { return (char)(intptr_t)event->context == 'a'; },
        NULL, SM_TRANSITION_EXTERNAL},
    // 'i' -> I
    {EVENT_KEYBOARD, &STATE_I,
        [](SM_StateMachine *sm, const SM_Event *event) { return (char)(intptr_t)event->context == 'i'; },
        NULL, SM_TRANSITION_EXTERNAL},
};

static const SM_Transition T_I[] = {
    // 'n' -> Idle, print hi
    {EVENT_KEYBOARD, &STATE_Idle,
        [](SM_StateMachine *sm, const SM_Event *event) { return (char)(intptr_t)event->context == 'n'; },
        print_msg_hi, SM_TRANSITION_EXTERNAL},
};

static const SM_Transition T_A[] = {
    // 'n' -> Idle, print ha
    {EVENT_KEYBOARD, &STATE_Idle,
        [](SM_StateMachine *sm, const SM_Event *event) { return (char)(intptr_t)event->context == 'n'; },
        print_msg_ha, SM_TRANSITION_EXTERNAL},
};

/* --- 5. 状态定义 --- */
static const SM_State STATE_Group = {
    .parent = NULL,
    .entryAction = entry_print,
    .exitAction = exit_print,
    .transitions = T_Group,
    .numTransitions = sizeof(T_Group) / sizeof(T_Group[0]),
    .name = "GROUP"
};
static const SM_State STATE_Idle = {
    .parent = &STATE_Group,
    .entryAction = entry_print,
    .exitAction = exit_print,
    .transitions = T_Idle,
    .numTransitions = sizeof(T_Idle) / sizeof(T_Idle[0]),
    .name = "IDLE"
};
static const SM_State STATE_H = {
    .parent = &STATE_Group,
    .entryAction = print_msg_recognised_char,
    .exitAction = exit_print,
    .transitions = T_H,
    .numTransitions = sizeof(T_H) / sizeof(T_H[0]),
    .name = "H"
};
static const SM_State STATE_I = {
    .parent = &STATE_Group,
    .entryAction = print_msg_recognised_char,
    .exitAction = exit_print,
    .transitions = T_I,
    .numTransitions = sizeof(T_I) / sizeof(T_I[0]),
    .name = "I"
};
static const SM_State STATE_A = {
    .parent = &STATE_Group,
    .entryAction = print_msg_recognised_char,
    .exitAction = exit_print,
    .transitions = T_A,
    .numTransitions = sizeof(T_A) / sizeof(T_A[0]),
    .name = "A"
};

/* --- 6. 状态机实例及消息队列 --- */
#define SM_THREAD_STACK_SIZE 1024
#define SM_THREAD_PRIORITY   10
#define SM_THREAD_TIMESLICE  10
#define SM_MQ_MAX_MSGS 8
#define MAX_STATE_DEPTH 8

static rt_thread_t sm_thread;
static rt_mq_t     sm_mq;
static SM_StateMachine sm;
static const SM_State *sm_entry_path_buffer[MAX_STATE_DEPTH];

/* --- 7. 未处理事件钩子 --- */
static void on_unhandled_event(SM_StateMachine *sm, const SM_Event *event)
{
    rt_kprintf("--- Unhandled Event: Event %u received in state '%s' ---\n",
        (unsigned)event->id, SM_GetCurrentStateName(sm));
}

/* --- 8. 状态机线程入口 --- */
static void state_machine_thread_entry(void* parameter)
{
    SM_Init(&sm, &STATE_Idle, sm_entry_path_buffer, MAX_STATE_DEPTH, NULL, on_unhandled_event);
    rt_kprintf("State machine initialized. Initial State: %s\n", SM_GetCurrentStateName(&sm));

    while (1)
    {
        SM_Event event_buffer;
        rt_err_t result = rt_mq_recv(sm_mq, &event_buffer, sizeof(SM_Event), RT_WAITING_FOREVER);

        if (result == RT_EOK)
        {
            rt_kprintf("\n--- Event received: %d, dispatching to state machine ---\n", (int)(intptr_t)event_buffer.context);
            bool handled = SM_Dispatch(&sm, &event_buffer);
            if (!handled) {
                rt_kprintf("Event %d was not handled.\n", (int)(intptr_t)event_buffer.context);
            }
            rt_kprintf("Current State: %s\n", SM_GetCurrentStateName(&sm));
        }
    }
}

/* --- 9. 外部事件投递接口 --- */
rt_err_t post_event_to_sm(uint32_t event_id, void* context)
{
    SM_Event event_to_send;
    event_to_send.id = event_id;
    event_to_send.context = context;
    return rt_mq_send_wait(sm_mq, &event_to_send, sizeof(SM_Event), 0);
}

/* --- 10. 初始化函数 --- */
int state_init(void)
{
    static struct rt_messagequeue static_sm_mq;
    static rt_uint8_t sm_mq_pool[SM_MQ_MAX_MSGS * sizeof(SM_Event)];

    sm_mq = (rt_mq_t)&static_sm_mq;
    rt_err_t result = rt_mq_init(sm_mq,
                                 "key_mq",
                                 &sm_mq_pool[0],
                                 sizeof(SM_Event),
                                 sizeof(sm_mq_pool),
                                 RT_IPC_FLAG_FIFO);
    if (result != RT_EOK)
    {
        rt_kprintf("Failed to initialize state machine message queue.\n");
        return -1;
    }

    static struct rt_thread static_sm_thread;
    static rt_uint8_t sm_thread_stack[SM_THREAD_STACK_SIZE];

    sm_thread = &static_sm_thread;
    rt_thread_init(sm_thread,
                   "state_sm_thread",
                   state_machine_thread_entry,
                   RT_NULL,
                   &sm_thread_stack[0],
                   sizeof(sm_thread_stack),
                   SM_THREAD_PRIORITY,
                   SM_THREAD_TIMESLICE);

    rt_thread_startup(sm_thread);

    return 0;
}
INIT_APP_EXPORT(state_init);

/* --- 11. 命令行接口 --- */
#ifdef FINSH_USING_MSH
#include <finsh.h>
static void state_key_set(int argc, char **argv)
{
    if (argc == 2)
    {
        rt_kprintf("state key set:%c\n", argv[1][0]);
        char ch = argv[1][0];
        post_event_to_sm(EVENT_KEYBOARD, (void *)(intptr_t)ch);
    }
    else
    {
        rt_kprintf("state key set <a-z>\n");
    }
}
MSH_CMD_EXPORT(state_key_set, state key set <a-z>.);
#endif /* FINSH_USING_MSH */
