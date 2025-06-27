#include <rtthread.h>
#include "state_machine.h"

// --- 1. 定义事件 ---
enum
{
    EV_POWER_ON,
    EV_START_TASK,
    EV_TASK_COMPLETE,
    EV_POWER_OFF
};

// --- 2. 状态前置声明 ---
static const SM_State STATE_Off;
static const SM_State STATE_On;
static const SM_State STATE_Idle;
static const SM_State STATE_Running;

// --- 3. 用户数据结构 ---
typedef struct
{
    int tasks_completed;
} AppData;

// --- 4. 动作与守卫函数 ---
static void entry_On(SM_StateMachine *sm, const SM_Event *event) { rt_kprintf("  (Entry)-> On\n"); }
static void exit_On(SM_StateMachine *sm, const SM_Event *event) { rt_kprintf("  (Exit) -> On\n"); }
static void entry_Idle(SM_StateMachine *sm, const SM_Event *event) { rt_kprintf("    (Entry)-> Idle\n"); }
static void entry_Running(SM_StateMachine *sm, const SM_Event *event) { rt_kprintf("    (Entry) -> Running\n"); }
static void exit_Running(SM_StateMachine *sm, const SM_Event *event) { rt_kprintf("    (Exit) -> Running\n"); }
static void on_power_off(SM_StateMachine *sm, const SM_Event *event) { rt_kprintf("  Action: Shutting down...\n"); }
static void on_task_done(SM_StateMachine *sm, const SM_Event *event)
{
    AppData *data = (AppData *)sm->userData;
    data->tasks_completed++;
    rt_kprintf("  Action: Task finished. Total completed: %d\n", data->tasks_completed);
}
static bool can_start_task(SM_StateMachine *sm, const SM_Event *event)
{
    AppData *data = (AppData *)sm->userData;
    rt_kprintf("  Guard: Checking if tasks completed < 3... (%s)\n", (data->tasks_completed < 3) ? "Yes" : "No");
    return data->tasks_completed < 3;
}

// --- 5. 转换表 ---
static const SM_Transition T_Off[] = {
    {EV_POWER_ON, &STATE_Idle, NULL, NULL, SM_TRANSITION_EXTERNAL}
};
static const SM_Transition T_On[] = {
    {EV_POWER_OFF, &STATE_Off, NULL, on_power_off, SM_TRANSITION_EXTERNAL}
};
static const SM_Transition T_Idle[] = {
    {EV_START_TASK, &STATE_Running, can_start_task, NULL, SM_TRANSITION_EXTERNAL}
};
static const SM_Transition T_Running[] = {
    {EV_TASK_COMPLETE, &STATE_Idle, NULL, on_task_done, SM_TRANSITION_EXTERNAL}
};

// --- 6. 状态定义 ---
static const SM_State STATE_Off = {
    .parent = NULL,
    .entryAction = NULL,
    .exitAction = NULL,
    .transitions = T_Off,
    .numTransitions = sizeof(T_Off) / sizeof(T_Off[0]),
    .name = "Off"
};
static const SM_State STATE_On = {
    .parent = NULL,
    .entryAction = entry_On,
    .exitAction = exit_On,
    .transitions = T_On,
    .numTransitions = sizeof(T_On) / sizeof(T_On[0]),
    .name = "On"
};
static const SM_State STATE_Idle = {
    .parent = &STATE_On,
    .entryAction = entry_Idle,
    .exitAction = NULL,
    .transitions = T_Idle,
    .numTransitions = sizeof(T_Idle) / sizeof(T_Idle[0]),
    .name = "Idle"
};
static const SM_State STATE_Running = {
    .parent = &STATE_On,
    .entryAction = entry_Running,
    .exitAction = exit_Running,
    .transitions = T_Running,
    .numTransitions = sizeof(T_Running) / sizeof(T_Running[0]),
    .name = "Running"
};

// --- 7. RT-Thread 相关定义 ---
#define SM_THREAD_PRIORITY   15
#define SM_THREAD_STACK_SIZE 1024
#define SM_THREAD_TIMESLICE  10
#define SM_MQ_MAX_MSGS 10

static rt_thread_t sm_thread;
static rt_mq_t     sm_mq;
static SM_StateMachine app_sm;
static AppData app_data; // 用户数据实例

#define MAX_STATE_DEPTH 8
static const SM_State *sm_entry_path_buffer[MAX_STATE_DEPTH];

// --- 8. 未处理事件钩子 ---
static void on_unhandled_event(SM_StateMachine *sm, const SM_Event *event)
{
    rt_kprintf("--- Unhandled Event Hook: Event %u received in state '%s' ---\n",
               (unsigned)event->id, SM_GetCurrentStateName(sm));
}

// --- 9. 状态机线程入口 ---
static void state_machine_thread_entry(void* parameter)
{
    // 初始化状态机
    SM_Init(&app_sm, &STATE_Off, sm_entry_path_buffer, MAX_STATE_DEPTH, &app_data, on_unhandled_event);
    rt_kprintf("State machine initialized. Initial State: %s\n", SM_GetCurrentStateName(&app_sm));

    while (1)
    {
        SM_Event event_buffer;
        rt_err_t result = rt_mq_recv(sm_mq, &event_buffer, sizeof(SM_Event), RT_WAITING_FOREVER);

        if (result == RT_EOK)
        {
            rt_kprintf("\n--- Event received: %d, dispatching to state machine ---\n", event_buffer.id);
            bool handled = SM_Dispatch(&app_sm, &event_buffer);
            if (!handled) {
                rt_kprintf("Event %d was not handled.\n", event_buffer.id);
            }
            rt_kprintf("Current State: %s\n", SM_GetCurrentStateName(&app_sm));
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
int app_sm_init(void)
{
    static struct rt_messagequeue static_sm_mq;
    static rt_uint8_t sm_mq_pool[SM_MQ_MAX_MSGS * sizeof(SM_Event)];

    sm_mq = (rt_mq_t)&static_sm_mq;
    rt_err_t result = rt_mq_init(sm_mq,
                                 "sm_mq",
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
                   "sm_thread",
                   state_machine_thread_entry,
                   RT_NULL,
                   &sm_thread_stack[0],
                   sizeof(sm_thread_stack),
                   SM_THREAD_PRIORITY,
                   SM_THREAD_TIMESLICE);

    rt_thread_startup(sm_thread);

    return 0;
}
// INIT_APP_EXPORT(app_sm_init);


int main(void)
{
    rt_kprintf("State Machine Application Starting...\n");

    // Initialize the state machine application
    if (app_sm_init() != 0)
    {
        rt_kprintf("State Machine Application Initialization Failed!\n");
        return -1;
    }

    // 查询初始状态
    rt_kprintf("Initial State: %s\n", SM_GetCurrentStateName(&app_sm));

    // 依次投递事件，演示状态流转
    post_event_to_sm(EV_POWER_ON, NULL);
    rt_thread_mdelay(100); // 延时便于观察输出

    post_event_to_sm(EV_START_TASK, NULL);
    rt_thread_mdelay(100);

    post_event_to_sm(EV_TASK_COMPLETE, NULL);
    rt_thread_mdelay(100);

    post_event_to_sm(EV_START_TASK, NULL);
    rt_thread_mdelay(100);

    post_event_to_sm(EV_TASK_COMPLETE, NULL);
    rt_thread_mdelay(100);

    post_event_to_sm(EV_START_TASK, NULL);
    rt_thread_mdelay(100);

    post_event_to_sm(EV_TASK_COMPLETE, NULL);
    rt_thread_mdelay(100);

    // 演示未处理事件
    post_event_to_sm(99, NULL);
    rt_thread_mdelay(100);

    // 查询当前状态
    rt_kprintf("Current State: %s\n", SM_GetCurrentStateName(&app_sm));

    // 重置状态机
    rt_kprintf("Resetting state machine...\n");
    SM_Reset(&app_sm);
    rt_kprintf("After reset, State: %s\n", SM_GetCurrentStateName(&app_sm));

    // 关闭状态机
    post_event_to_sm(EV_POWER_OFF, NULL);
    rt_thread_mdelay(100);

    rt_kprintf("State Machine Application Finished.\n");
    return 0;
}

/*
执行结果：
State Machine Application Starting...
Initial State: Unknown
State machine initialized. Initial State: Off

--- Event received: 0, dispatching to state machine ---
ms(Ent  (Entry)-> On
    (Entry)-> Idle
Current State: Idle

--- Event received: 1, dispatching to state machine ---
  Guard: Checking if tasks completed < 3... (Yes)
    (Entry) -> Running
Current State: Running

--- Event received: 2, dispatching to state machine ---
  Action: Task finished. Total completed: 1
    (Exit) -> Running
    (Entry)-> Idle
Current State: Idle

--- Event received: 1, dispatching to state machine ---
  Guard: Checking if tasks completed < 3... (Yes)
    (Entry) -> Running
Current State: Running

--- Event received: 2, dispatching to state machine ---
  Action: Task finished. Total completed: 2
    (Exit) -> Running
    (Entry)-> Idle
Current State: Idle

--- Event received: 1, dispatching to state machine ---
  Guard: Checking if tasks completed < 3... (Yes)
    (Entry) -> Running
Current State: Running

--- Event received: 2, dispatching to state machine ---
  Action: Task finished. Total completed: 3
    (Exit) -> Running
    (Entry)-> Idle
Current State: Idle

--- Event received: 99, dispatching to state machine ---
--- Unhandled Event Hook: Event 99 received in state 'Idle' ---
Event 99 was not handled.
Current State: Idle
Current State: Idle
Resetting state machine...
  (Exit) -> On
After reset, State: Off

--- Event received: 3, dispatching to state machine ---
--- Unhandled Event Hook: Event 3 received in state 'Off' ---
Event 3 was not handled.
Current State: Off
State Machine Application Finished.
*/
