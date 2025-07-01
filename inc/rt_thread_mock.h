/* 
 * Simple RT-Thread API mock for testing purposes
 * This allows building and testing the state machine without full RT-Thread
 */

#ifndef RT_THREAD_MOCK_H
#define RT_THREAD_MOCK_H

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <semaphore.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

/* RT-Thread Types */
typedef int rt_err_t;
typedef void* rt_thread_t;
typedef void* rt_mutex_t;
typedef void* rt_mq_t;

/* RT-Thread Error Codes */
#define RT_EOK          0
#define RT_ERROR        1
#define RT_ETIMEOUT     2
#define RT_EFULL        3
#define RT_EEMPTY       4
#define RT_ENOMEM       5
#define RT_ENOSYS       6
#define RT_EBUSY        7
#define RT_EIO          8
#define RT_EINTR        9
#define RT_EINVAL       10

/* RT-Thread Constants */
#define RT_WAITING_FOREVER  ((uint32_t)-1)
#define RT_WAITING_NO       0

/* RT-Thread IPC Flags */
#define RT_IPC_FLAG_FIFO    0
#define RT_IPC_FLAG_PRIO    1

/* Mock Structures */
typedef struct {
    pthread_t pthread;
    void (*entry)(void* parameter);
    void* parameter;
    bool running;
    char name[16];
} rt_thread_mock_t;

typedef struct {
    pthread_mutex_t mutex;
    char name[16];
} rt_mutex_mock_t;

typedef struct {
    void* buffer;
    size_t msg_size;
    size_t max_msgs;
    size_t count;
    size_t head;
    size_t tail;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
    char name[16];
} rt_mq_mock_t;

/* RT-Thread API Mock Functions */
rt_thread_t rt_thread_create(const char* name, void (*entry)(void* parameter), 
                           void* parameter, uint32_t stack_size, 
                           uint8_t priority, uint32_t tick);
rt_err_t rt_thread_startup(rt_thread_t thread);
rt_err_t rt_thread_delete(rt_thread_t thread);

rt_mutex_t rt_mutex_create(const char* name, uint8_t flag);
rt_err_t rt_mutex_delete(rt_mutex_t mutex);
rt_err_t rt_mutex_take(rt_mutex_t mutex, int32_t time);
rt_err_t rt_mutex_release(rt_mutex_t mutex);

rt_mq_t rt_mq_create(const char* name, size_t msg_size, size_t max_msgs, uint8_t flag);
rt_err_t rt_mq_delete(rt_mq_t mq);
rt_err_t rt_mq_send(rt_mq_t mq, void* buffer, size_t size);
rt_err_t rt_mq_send_wait(rt_mq_t mq, void* buffer, size_t size, int32_t timeout);
rt_err_t rt_mq_recv(rt_mq_t mq, void* buffer, size_t size, int32_t timeout);

#define rt_kprintf printf

#endif /* RT_THREAD_MOCK_H */