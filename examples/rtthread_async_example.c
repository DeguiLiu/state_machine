/*
 * @file   rtthread_async_example.c
 * @brief  RT-Thread 多线程、消息队列驱动的异步+同步事件分发状态机完整示例
 *
 * 功能说明：
 *  - 使用 rt_thread/rt_mq/rt_mutex 实现异步事件分发
 *  - 支持直接加锁同步分发（同步事件），演示两种方式并存
 *  - worker线程循环取队列异步分发
 *  - 多个线程安全投递事件，状态机内部操作全加锁
 *  - 支持基本状态切换、内部统计与输出
 *
 * 编译: 用RT-Thread环境编译，添加到工程并MSH调用 sm_rt_async_example
 */

 #include <rtthread.h>
 #include "state_machine_rt.h"
 #include <stdio.h>
 #include <string.h>
 
 #define EVENT_START   1
 #define EVENT_STOP    2
 #define EVENT_TICK    3
 #define EVENT_PAUSE   4
 #define EVENT_RESUME  5
 #define EVENT_SYNC    6    // 专用于同步分发演示
 
 #define EVENT_QUEUE_SIZE  8   // 安全的队列长度
 
 typedef struct {
     int tick_count;
     int pause_count;
     int sync_count;
     int async_count;
 } ExampleUserData;
 
 /* ---- 状态和动作 ---- */
 static void idle_entry(SM_StateMachine *sm, const SM_Event *event);
 static void idle_exit(SM_StateMachine *sm, const SM_Event *event);
 static void running_entry(SM_StateMachine *sm, const SM_Event *event);
 static void running_exit(SM_StateMachine *sm, const SM_Event *event);
 static void paused_entry(SM_StateMachine *sm, const SM_Event *event);
 static void paused_exit(SM_StateMachine *sm, const SM_Event *event);
 
 static void action_start(SM_StateMachine *sm, const SM_Event *event);
 static void action_stop(SM_StateMachine *sm, const SM_Event *event);
 static void action_tick(SM_StateMachine *sm, const SM_Event *event);
 static void action_pause(SM_StateMachine *sm, const SM_Event *event);
 static void action_resume(SM_StateMachine *sm, const SM_Event *event);
 static void action_sync(SM_StateMachine *sm, const SM_Event *event);
 
 /* ---- 状态转发表 ---- */
 static const SM_Transition idle_transitions[] = {
     {EVENT_START, NULL, NULL, action_start, SM_TRANSITION_EXTERNAL}
 };
 static const SM_Transition running_transitions[] = {
     {EVENT_TICK, NULL, NULL, action_tick, SM_TRANSITION_INTERNAL},
     {EVENT_PAUSE, NULL, NULL, action_pause, SM_TRANSITION_EXTERNAL},
     {EVENT_STOP, NULL, NULL, action_stop, SM_TRANSITION_EXTERNAL},
     {EVENT_SYNC, NULL, NULL, action_sync, SM_TRANSITION_INTERNAL}
 };
 static const SM_Transition paused_transitions[] = {
     {EVENT_RESUME, NULL, NULL, action_resume, SM_TRANSITION_EXTERNAL},
     {EVENT_STOP, NULL, NULL, action_stop, SM_TRANSITION_EXTERNAL}
 };
 
 /* ---- 状态定义 ---- */
 static const SM_State idle_state = {
     .parent = NULL,
     .entryAction = idle_entry,
     .exitAction = idle_exit,
     .transitions = idle_transitions,
     .numTransitions = sizeof(idle_transitions)/sizeof(idle_transitions[0]),
     .name = "Idle"
 };
 static const SM_State running_state = {
     .parent = NULL,
     .entryAction = running_entry,
     .exitAction = running_exit,
     .transitions = running_transitions,
     .numTransitions = sizeof(running_transitions)/sizeof(running_transitions[0]),
     .name = "Running"
 };
 static const SM_State paused_state = {
     .parent = NULL,
     .entryAction = paused_entry,
     .exitAction = paused_exit,
     .transitions = paused_transitions,
     .numTransitions = sizeof(paused_transitions)/sizeof(paused_transitions[0]),
     .name = "Paused"
 };
 
 /* ---- 状态机实例和RTT对象 ---- */
 static SM_RT_Instance sm_instance;
 static const SM_State *entry_path_buffer[4];
 static ExampleUserData user_data;
 
 static struct rt_messagequeue *event_queue = RT_NULL;
 static struct rt_mutex *sm_mutex = RT_NULL;
 static struct rt_thread *worker_thread = RT_NULL;
 
 /* ---- worker线程体 ---- */
 static void sm_worker_thread_entry(void *param)
 {
     SM_RT_Instance *sm = (SM_RT_Instance*)param;
     SM_Event event;
     while (1) {
         if (rt_mq_recv(event_queue, &event, sizeof(event), RT_WAITING_FOREVER) == RT_EOK) {
             rt_mutex_take(sm_mutex, RT_WAITING_FOREVER);
             SM_Dispatch(&sm->base_sm, &event);
             rt_mutex_release(sm_mutex);
 
             // 统计异步事件
             ExampleUserData *ud = (ExampleUserData*)sm->base_sm.userData;
             if(event.id != EVENT_SYNC) ud->async_count++;
 
             if (event.id == EVENT_STOP)
                 break;
         } else {
             rt_thread_mdelay(1);
         }
     }
 }
 
 /* ---- 事件投递封装 ---- */
 static rt_err_t sm_post_event(SM_RT_Instance *sm, const SM_Event *event)
 {
     return rt_mq_send(event_queue, event, sizeof(*event));
 }
 
 /* ---- 状态动作实现 ---- */
 static void idle_entry(SM_StateMachine *sm, const SM_Event *event) {
     rt_kprintf("[Idle] Entry\n");
 }
 static void idle_exit(SM_StateMachine *sm, const SM_Event *event) {
     rt_kprintf("[Idle] Exit\n");
 }
 static void running_entry(SM_StateMachine *sm, const SM_Event *event) {
     rt_kprintf("[Running] Entry\n");
 }
 static void running_exit(SM_StateMachine *sm, const SM_Event *event) {
     rt_kprintf("[Running] Exit\n");
 }
 static void paused_entry(SM_StateMachine *sm, const SM_Event *event) {
     rt_kprintf("[Paused] Entry\n");
 }
 static void paused_exit(SM_StateMachine *sm, const SM_Event *event) {
     rt_kprintf("[Paused] Exit\n");
 }
 static void action_start(SM_StateMachine *sm, const SM_Event *event) {
     SM_RT_Instance *inst = rt_container_of(sm, SM_RT_Instance, base_sm);
     rt_kprintf("[Action] START\n");
     inst->base_sm.currentState = &running_state;
     if (inst->base_sm.currentState->entryAction)
         inst->base_sm.currentState->entryAction(sm, event);
 }
 static void action_stop(SM_StateMachine *sm, const SM_Event *event) {
     SM_RT_Instance *inst = rt_container_of(sm, SM_RT_Instance, base_sm);
     rt_kprintf("[Action] STOP\n");
     inst->base_sm.currentState = &idle_state;
     if (inst->base_sm.currentState->entryAction)
         inst->base_sm.currentState->entryAction(sm, event);
 }
 static void action_tick(SM_StateMachine *sm, const SM_Event *event) {
     ExampleUserData *ud = (ExampleUserData*)sm->userData;
     ud->tick_count++;
     rt_kprintf("[Action] TICK, count=%d\n", ud->tick_count);
     if (ud->tick_count == 5) {
         rt_kprintf("Auto PAUSE after 5 ticks\n");
         SM_Event pause_evt = {.id=EVENT_PAUSE, .context=NULL};
         sm_post_event(rt_container_of(sm, SM_RT_Instance, base_sm), &pause_evt);
     }
 }
 static void action_pause(SM_StateMachine *sm, const SM_Event *event) {
     ExampleUserData *ud = (ExampleUserData*)sm->userData;
     SM_RT_Instance *inst = rt_container_of(sm, SM_RT_Instance, base_sm);
     rt_kprintf("[Action] PAUSE\n");
     ud->pause_count++;
     inst->base_sm.currentState = &paused_state;
     if (inst->base_sm.currentState->entryAction)
         inst->base_sm.currentState->entryAction(sm, event);
 }
 static void action_resume(SM_StateMachine *sm, const SM_Event *event) {
     SM_RT_Instance *inst = rt_container_of(sm, SM_RT_Instance, base_sm);
     rt_kprintf("[Action] RESUME\n");
     inst->base_sm.currentState = &running_state;
     if (inst->base_sm.currentState->entryAction)
         inst->base_sm.currentState->entryAction(sm, event);
 }
 static void action_sync(SM_StateMachine *sm, const SM_Event *event) {
     ExampleUserData *ud = (ExampleUserData*)sm->userData;
     ud->sync_count++;
     rt_kprintf("[Action] SYNC_EVENT received synchronously, sync_count=%d\n", ud->sync_count);
 }
 
 /* ---- 生产者线程：周期性投递TICK事件 ---- */
 static void tick_producer_thread_entry(void *param)
 {
     int i = 0;
     SM_Event event = { .id=EVENT_TICK, .context=NULL };
     while (i++ < 10) {
         rt_thread_mdelay(200);
         sm_post_event(&sm_instance, &event);
     }
 }
 
 /* ---- 生产者线程2：投递RESUME事件 ---- */
 static void resume_producer_thread_entry(void *param)
 {
     rt_thread_mdelay(1500);
     rt_kprintf("[Producer2] Send RESUME\n");
     SM_Event event = { .id=EVENT_RESUME, .context=NULL };
     sm_post_event(&sm_instance, &event);
 }
 
 /* ---- 主例程 ---- */
 static void sm_rt_async_example(void)
 {
     rt_kprintf("=== POSIX example of async and sync event distribution ===\n");
 
     static char event_mq_buf[EVENT_QUEUE_SIZE * sizeof(SM_Event)];
     static char worker_stack[1024];
     static char tick_stack[512];
     static char resume_stack[512];
 
     event_queue = rt_mq_create("smq", sizeof(SM_Event), EVENT_QUEUE_SIZE, RT_IPC_FLAG_FIFO);
     if (!event_queue) {
         rt_kprintf("rt_mq_create failed!\n");
         return;
     }
     sm_mutex = rt_mutex_create("smmtx", RT_IPC_FLAG_PRIO);
     if (!sm_mutex) {
         rt_kprintf("rt_mutex_create failed!\n");
         return;
     }
 
     sm_instance.event_queue = event_queue;
     sm_instance.mutex = sm_mutex;
 
     memset(&user_data, 0, sizeof(user_data));
     SM_Init(&sm_instance.base_sm, &idle_state, entry_path_buffer,
             sizeof(entry_path_buffer)/sizeof(entry_path_buffer[0]),
             &user_data, NULL);
 
     worker_thread = rt_thread_create("smwkr", sm_worker_thread_entry, &sm_instance, sizeof(worker_stack), 20, 10);
     rt_thread_startup(worker_thread);
 
     rt_thread_t tick_thread = rt_thread_create("smtick", tick_producer_thread_entry, RT_NULL, sizeof(tick_stack), 22, 10);
     rt_thread_startup(tick_thread);
 
     SM_Event event = { .id = EVENT_START, .context = NULL };
     rt_kprintf("[Main] Post START (async)\n");
     sm_post_event(&sm_instance, &event);
 
     /* 主线程同步分发SYNC事件 */
     rt_thread_mdelay(100); // 确保状态机已经切换到running
     event.id = EVENT_SYNC;
     rt_kprintf("[Main] Dispatch SYNC_EVENT (sync)\n");
     rt_mutex_take(sm_mutex, RT_WAITING_FOREVER);
     SM_Dispatch(&sm_instance.base_sm, &event);
     rt_mutex_release(sm_mutex);
 
     rt_thread_t resume_thread = rt_thread_create("smrsum", resume_producer_thread_entry, RT_NULL, sizeof(resume_stack), 23, 10);
     rt_thread_startup(resume_thread);
 
     /* 再同步分发一次 */
     rt_thread_mdelay(500);
     event.id = EVENT_SYNC;
     rt_kprintf("[Main] Dispatch SYNC_EVENT (sync) 2nd time\n");
     rt_mutex_take(sm_mutex, RT_WAITING_FOREVER);
     SM_Dispatch(&sm_instance.base_sm, &event);
     rt_mutex_release(sm_mutex);
 
     /* 等一段时间后投递STOP事件 */
     rt_thread_mdelay(4000);
     event.id = EVENT_STOP;
     rt_kprintf("[Main] Post STOP (async)\n");
     sm_post_event(&sm_instance, &event);
 
     /* 等待线程退出 */
     rt_thread_mdelay(300);
 
     rt_kprintf("Tick count (async): %d\n", user_data.tick_count);
     rt_kprintf("Pause count (async): %d\n", user_data.pause_count);
     rt_kprintf("Sync event dispatched count (sync): %d\n", user_data.sync_count);
     rt_kprintf("Async event dispatched count (async): %d\n", user_data.async_count);
 
     rt_mq_delete(event_queue);
     rt_mutex_delete(sm_mutex);
 
     rt_kprintf("=== end of example ===\n");
 }
 
 MSH_CMD_EXPORT(sm_rt_async_example, RT-Thread状态机异步+同步事件驱动示例);
 