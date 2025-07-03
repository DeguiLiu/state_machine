/*
 * @file   posix_async_example.c
 * @brief  POSIX/Linux 多线程、消息队列驱动的异步+同步事件分发状态机完整示例
 *
 * 功能说明：
 *  - 使用 pthread/mqueue/pthread_mutex 实现异步事件分发
 *  - 支持直接加锁同步分发（同步事件），演示两种方式并存
 *  - worker线程循环取队列异步分发
 *  - 多个线程安全投递事件，状态机内部操作全加锁
 *  - 支持基本状态切换、内部统计与输出
 *
 * 编译: gcc -o posix_async_example examples/posix_async_example.c src/state_machine_rt.c -lpthread -lrt
 */

 #define _GNU_SOURCE
 #include "state_machine_rt.h"   
 #include <pthread.h>
 #include <mqueue.h>
 #include <unistd.h>
 #include <stdio.h>
 #include <string.h>
 #include <stdlib.h>
 #include <errno.h>
 #include <sys/time.h>
 
 #define EVENT_START   1
 #define EVENT_STOP    2
 #define EVENT_TICK    3
 #define EVENT_PAUSE   4
 #define EVENT_RESUME  5
 #define EVENT_SYNC    6    // 用于同步分发演示
 
 #define EVENT_QUEUE_NAME  "/smq_example"
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
     {EVENT_SYNC, NULL, NULL, action_sync, SM_TRANSITION_INTERNAL}    // 同步事件分发演示
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
 
 /* ---- 状态机实例和互斥/队列 ---- */
 static SM_RT_Instance sm_instance;
 static const SM_State *entry_path_buffer[4];
 static ExampleUserData user_data;
 
 static mqd_t event_queue = (mqd_t)-1;
 static pthread_mutex_t sm_mutex;
 
 /* ---- worker线程体 ---- */
 static void* sm_worker_thread(void *param)
 {
     SM_RT_Instance *sm = (SM_RT_Instance*)param;
     SM_Event event;
     ssize_t n;
     while (1) {
         n = mq_receive(event_queue, (char*)&event, sizeof(event), NULL);
         if (n == sizeof(event)) {
             pthread_mutex_lock(&sm_mutex);
             SM_RT_PostEvent(sm, &event); // 修改为SM_RT_PostEvent
             pthread_mutex_unlock(&sm_mutex);
 
             // 统计异步事件
             ExampleUserData *ud = (ExampleUserData*)sm->base_sm.userData;
             if(event.id != EVENT_SYNC) ud->async_count++;
 
             if (event.id == EVENT_STOP)
                 break;
         } else {
             usleep(1000);
         }
     }
     return NULL;
 }
 
 /* ---- 事件投递封装 ---- */
 static int sm_post_event(SM_RT_Instance *sm, const SM_Event *event)
 {
     return mq_send(event_queue, (const char*)event, sizeof(*event), 0);
 }
 
 /* ---- 状态动作实现 ---- */
 static void idle_entry(SM_StateMachine *sm, const SM_Event *event) {
     printf("[Idle] Entry\n");
 }
 static void idle_exit(SM_StateMachine *sm, const SM_Event *event) {
     printf("[Idle] Exit\n");
 }
 static void running_entry(SM_StateMachine *sm, const SM_Event *event) {
     printf("[Running] Entry\n");
 }
 static void running_exit(SM_StateMachine *sm, const SM_Event *event) {
     printf("[Running] Exit\n");
 }
 static void paused_entry(SM_StateMachine *sm, const SM_Event *event) {
     printf("[Paused] Entry\n");
 }
 static void paused_exit(SM_StateMachine *sm, const SM_Event *event) {
     printf("[Paused] Exit\n");
 }
 static void action_start(SM_StateMachine *sm, const SM_Event *event) {
     SM_RT_Instance *inst = (SM_RT_Instance*)(((char*)sm) - offsetof(SM_RT_Instance, base_sm));
     printf("[Action] START\n");
     inst->base_sm.currentState = &running_state;
     if (inst->base_sm.currentState->entryAction)
         inst->base_sm.currentState->entryAction(sm, event);
 }
 static void action_stop(SM_StateMachine *sm, const SM_Event *event) {
     SM_RT_Instance *inst = (SM_RT_Instance*)(((char*)sm) - offsetof(SM_RT_Instance, base_sm));
     printf("[Action] STOP\n");
     inst->base_sm.currentState = &idle_state;
     if (inst->base_sm.currentState->entryAction)
         inst->base_sm.currentState->entryAction(sm, event);
 }
 static void action_tick(SM_StateMachine *sm, const SM_Event *event) {
     ExampleUserData *ud = (ExampleUserData*)sm->userData;
     ud->tick_count++;
     printf("[Action] TICK, count=%d\n", ud->tick_count);
     if (ud->tick_count == 5) {
         printf("Auto PAUSE after 5 ticks\n");
         SM_Event pause_evt = {.id=EVENT_PAUSE, .context=NULL};
         sm_post_event((SM_RT_Instance*)(((char*)sm) - offsetof(SM_RT_Instance, base_sm)), &pause_evt);
     }
 }
 static void action_pause(SM_StateMachine *sm, const SM_Event *event) {
     ExampleUserData *ud = (ExampleUserData*)sm->userData;
     SM_RT_Instance *inst = (SM_RT_Instance*)(((char*)sm) - offsetof(SM_RT_Instance, base_sm));
     printf("[Action] PAUSE\n");
     ud->pause_count++;
     inst->base_sm.currentState = &paused_state;
     if (inst->base_sm.currentState->entryAction)
         inst->base_sm.currentState->entryAction(sm, event);
 }
 static void action_resume(SM_StateMachine *sm, const SM_Event *event) {
     SM_RT_Instance *inst = (SM_RT_Instance*)(((char*)sm) - offsetof(SM_RT_Instance, base_sm));
     printf("[Action] RESUME\n");
     inst->base_sm.currentState = &running_state;
     if (inst->base_sm.currentState->entryAction)
         inst->base_sm.currentState->entryAction(sm, event);
 }
 static void action_sync(SM_StateMachine *sm, const SM_Event *event) {
     ExampleUserData *ud = (ExampleUserData*)sm->userData;
     ud->sync_count++;
     printf("[Action] SYNC_EVENT received synchronously, sync_count=%d\n", ud->sync_count);
 }
 
 /* ---- 生产者线程：周期性投递TICK事件 ---- */
 static void* tick_producer_thread(void *param)
 {
     int i = 0;
     SM_Event event = { .id=EVENT_TICK, .context=NULL };
     while (i++ < 10) {
         usleep(200*1000);
         sm_post_event(&sm_instance, &event);
     }
     return NULL;
 }
 
 /* ---- 生产者线程2：投递RESUME事件 ---- */
 static void* resume_producer_thread(void *param)
 {
     usleep(1500*1000);
     printf("[Producer2] Send RESUME\n");
     SM_Event event = { .id=EVENT_RESUME, .context=NULL };
     sm_post_event(&sm_instance, &event);
     return NULL;
 }
 
 /* ---- 主函数 ---- */
 int main(void)
 {
     printf("=== POSIX example of async and sync event distribution ===\n");
 
     /* 1. 创建消息队列和互斥锁，并填入SM_RT_Instance */
     struct mq_attr attr;
     memset(&attr, 0, sizeof(attr));
     attr.mq_flags = 0;
     attr.mq_maxmsg = EVENT_QUEUE_SIZE;
     attr.mq_msgsize = sizeof(SM_Event);
 
     mq_unlink(EVENT_QUEUE_NAME); // 保证清理
     event_queue = mq_open(EVENT_QUEUE_NAME, O_CREAT|O_RDWR, 0666, &attr);
     if (event_queue == (mqd_t)-1) {
         perror("mq_open");
         return 1;
     }
     pthread_mutex_init(&sm_mutex, NULL);
 
     sm_instance.event_queue = &event_queue;
     sm_instance.mutex = &sm_mutex;
 
     /* 2. 初始化状态机 */
     memset(&user_data, 0, sizeof(user_data));
     SM_RT_Init(&sm_instance, &idle_state, entry_path_buffer,
             sizeof(entry_path_buffer)/sizeof(entry_path_buffer[0]),
             &user_data, NULL); // 传递SM_RT_Instance指针
 
     /* 3. 启动worker线程 */
     pthread_t worker;
     pthread_create(&worker, NULL, sm_worker_thread, &sm_instance);
 
     /* 4. 启动TICK生产者线程 */
     pthread_t tick_thread;
     pthread_create(&tick_thread, NULL, tick_producer_thread, NULL);
 
     /* 5. 主线程异步投递START事件 */
     SM_Event event = { .id = EVENT_START, .context = NULL };
     printf("[Main] Post START (async)\n");
     sm_post_event(&sm_instance, &event);
 
     /* 6. 主线程同步分发SYNC事件 */
     usleep(100*1000); // 确保状态机已经切换到running
     event.id = EVENT_SYNC;
     printf("[Main] Dispatch SYNC_EVENT (sync)\n");
     pthread_mutex_lock(&sm_mutex);
     SM_RT_PostEvent(&sm_instance, &event); // 修改为SM_RT_PostEvent
     pthread_mutex_unlock(&sm_mutex);
 
     /* 7. 启动RESUME生产者线程（延迟投递） */
     pthread_t resume_thread;
     pthread_create(&resume_thread, NULL, resume_producer_thread, NULL);
 
     /* 8. 主线程再同步分发一次SYNC事件 */
     usleep(500*1000); // 延迟保证状态机还在running
     event.id = EVENT_SYNC;
     printf("[Main] Dispatch SYNC_EVENT (sync) 2nd time\n");
     pthread_mutex_lock(&sm_mutex);
     SM_RT_PostEvent(&sm_instance, &event); // 修改为SM_RT_PostEvent
     pthread_mutex_unlock(&sm_mutex);
 
     /* 9. 主线程等一段时间后投递STOP事件 */
     sleep(4);
     event.id = EVENT_STOP;
     printf("[Main] Post STOP (async)\n");
     sm_post_event(&sm_instance, &event);
 
     /* 10. 等待各线程结束 */
     pthread_join(tick_thread, NULL);
     pthread_join(resume_thread, NULL);
     pthread_join(worker, NULL);
 
     /* 11. 输出统计 */
     printf("Tick count (async): %d\n", user_data.tick_count);
     printf("Pause count (async): %d\n", user_data.pause_count);
     printf("Sync event dispatched count (sync): %d\n", user_data.sync_count);
     printf("Async event dispatched count (async): %d\n", user_data.async_count);
 
     /* 12. 清理资源 */
     mq_close(event_queue);
     mq_unlink(EVENT_QUEUE_NAME);
     pthread_mutex_destroy(&sm_mutex);
 
     printf("=== end of example ===\n");
     return 0;
 }

/*
$ ./posix_async_example                                                                                                         ✖ ✹ ✭main 
=== POSIX example of async and sync  event distribution ===
[Idle] Entry
[Main] Post START (async)
[Action] START
[Running] Entry
[Main] Dispatch SYNC_EVENT (sync)
[Action] SYNC_EVENT received synchronously, sync_count=1
[Action] TICK, count=1
[Action] TICK, count=2
[Main] Dispatch SYNC_EVENT (sync) 2nd time
[Action] SYNC_EVENT received synchronously, sync_count=2
[Action] TICK, count=3
[Action] TICK, count=4
[Action] TICK, count=5
Auto PAUSE after 5 ticks
[Action] PAUSE
[Paused] Entry
[Producer2] Send RESUME
[Action] RESUME
[Running] Entry
[Action] TICK, count=6
[Action] TICK, count=7
[Action] TICK, count=8
[Main] Post STOP (async)
[Action] STOP
[Idle] Entry
Tick count (async): 8
Pause count (async): 1
Sync event dispatched count (sync): 2
Async event dispatched count (async): 14
=== end of example ===
*/