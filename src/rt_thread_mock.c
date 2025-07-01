/* 
 * Simple RT-Thread API mock implementation for testing purposes
 */

#include "rt_thread_mock.h"
#include <errno.h>
#include <sys/time.h>

/* Thread Management */
static void* thread_wrapper(void* arg) {
    rt_thread_mock_t* thread = (rt_thread_mock_t*)arg;
    if (thread && thread->entry) {
        thread->entry(thread->parameter);
    }
    return NULL;
}

rt_thread_t rt_thread_create(const char* name, void (*entry)(void* parameter), 
                           void* parameter, uint32_t stack_size, 
                           uint8_t priority, uint32_t tick) {
    rt_thread_mock_t* thread = (rt_thread_mock_t*)malloc(sizeof(rt_thread_mock_t));
    if (!thread) return NULL;
    
    thread->entry = entry;
    thread->parameter = parameter;
    thread->running = false;
    strncpy(thread->name, name ? name : "thread", sizeof(thread->name) - 1);
    thread->name[sizeof(thread->name) - 1] = '\0';
    
    return (rt_thread_t)thread;
}

rt_err_t rt_thread_startup(rt_thread_t thread) {
    rt_thread_mock_t* t = (rt_thread_mock_t*)thread;
    if (!t) return RT_EINVAL;
    
    if (pthread_create(&t->pthread, NULL, thread_wrapper, t) == 0) {
        t->running = true;
        return RT_EOK;
    }
    return RT_ERROR;
}

rt_err_t rt_thread_delete(rt_thread_t thread) {
    rt_thread_mock_t* t = (rt_thread_mock_t*)thread;
    if (!t) return RT_EINVAL;
    
    if (t->running) {
        pthread_cancel(t->pthread);
        pthread_join(t->pthread, NULL);
    }
    free(t);
    return RT_EOK;
}

/* Mutex Management */
rt_mutex_t rt_mutex_create(const char* name, uint8_t flag) {
    rt_mutex_mock_t* mutex = (rt_mutex_mock_t*)malloc(sizeof(rt_mutex_mock_t));
    if (!mutex) return NULL;
    
    if (pthread_mutex_init(&mutex->mutex, NULL) != 0) {
        free(mutex);
        return NULL;
    }
    
    strncpy(mutex->name, name ? name : "mutex", sizeof(mutex->name) - 1);
    mutex->name[sizeof(mutex->name) - 1] = '\0';
    
    return (rt_mutex_t)mutex;
}

rt_err_t rt_mutex_delete(rt_mutex_t mutex) {
    rt_mutex_mock_t* m = (rt_mutex_mock_t*)mutex;
    if (!m) return RT_EINVAL;
    
    pthread_mutex_destroy(&m->mutex);
    free(m);
    return RT_EOK;
}

rt_err_t rt_mutex_take(rt_mutex_t mutex, int32_t time) {
    rt_mutex_mock_t* m = (rt_mutex_mock_t*)mutex;
    if (!m) return RT_EINVAL;
    
    if (time == RT_WAITING_FOREVER) {
        return pthread_mutex_lock(&m->mutex) == 0 ? RT_EOK : RT_ERROR;
    } else if (time == RT_WAITING_NO) {
        return pthread_mutex_trylock(&m->mutex) == 0 ? RT_EOK : RT_EBUSY;
    } else {
        struct timespec ts;
        struct timeval now;
        gettimeofday(&now, NULL);
        ts.tv_sec = now.tv_sec + time / 1000;
        ts.tv_nsec = (now.tv_usec + (time % 1000) * 1000) * 1000;
        return pthread_mutex_timedlock(&m->mutex, &ts) == 0 ? RT_EOK : RT_ETIMEOUT;
    }
}

rt_err_t rt_mutex_release(rt_mutex_t mutex) {
    rt_mutex_mock_t* m = (rt_mutex_mock_t*)mutex;
    if (!m) return RT_EINVAL;
    
    return pthread_mutex_unlock(&m->mutex) == 0 ? RT_EOK : RT_ERROR;
}

/* Message Queue Management */
rt_mq_t rt_mq_create(const char* name, size_t msg_size, size_t max_msgs, uint8_t flag) {
    rt_mq_mock_t* mq = (rt_mq_mock_t*)malloc(sizeof(rt_mq_mock_t));
    if (!mq) return NULL;
    
    mq->buffer = malloc(msg_size * max_msgs);
    if (!mq->buffer) {
        free(mq);
        return NULL;
    }
    
    mq->msg_size = msg_size;
    mq->max_msgs = max_msgs;
    mq->count = 0;
    mq->head = 0;
    mq->tail = 0;
    
    if (pthread_mutex_init(&mq->mutex, NULL) != 0) {
        free(mq->buffer);
        free(mq);
        return NULL;
    }
    
    if (pthread_cond_init(&mq->not_empty, NULL) != 0) {
        pthread_mutex_destroy(&mq->mutex);
        free(mq->buffer);
        free(mq);
        return NULL;
    }
    
    if (pthread_cond_init(&mq->not_full, NULL) != 0) {
        pthread_cond_destroy(&mq->not_empty);
        pthread_mutex_destroy(&mq->mutex);
        free(mq->buffer);
        free(mq);
        return NULL;
    }
    
    strncpy(mq->name, name ? name : "mq", sizeof(mq->name) - 1);
    mq->name[sizeof(mq->name) - 1] = '\0';
    
    return (rt_mq_t)mq;
}

rt_err_t rt_mq_delete(rt_mq_t mq) {
    rt_mq_mock_t* m = (rt_mq_mock_t*)mq;
    if (!m) return RT_EINVAL;
    
    pthread_cond_destroy(&m->not_full);
    pthread_cond_destroy(&m->not_empty);
    pthread_mutex_destroy(&m->mutex);
    free(m->buffer);
    free(m);
    return RT_EOK;
}

rt_err_t rt_mq_send(rt_mq_t mq, void* buffer, size_t size) {
    return rt_mq_send_wait(mq, buffer, size, RT_WAITING_NO);
}

rt_err_t rt_mq_send_wait(rt_mq_t mq, void* buffer, size_t size, int32_t timeout) {
    rt_mq_mock_t* m = (rt_mq_mock_t*)mq;
    if (!m || !buffer || size != m->msg_size) return RT_EINVAL;
    
    pthread_mutex_lock(&m->mutex);
    
    if (timeout == RT_WAITING_NO && m->count >= m->max_msgs) {
        pthread_mutex_unlock(&m->mutex);
        return RT_EFULL;
    }
    
    while (m->count >= m->max_msgs) {
        if (timeout == RT_WAITING_NO) {
            pthread_mutex_unlock(&m->mutex);
            return RT_EFULL;
        }
        
        if (timeout != RT_WAITING_FOREVER) {
            struct timespec ts;
            struct timeval now;
            gettimeofday(&now, NULL);
            ts.tv_sec = now.tv_sec + timeout / 1000;
            ts.tv_nsec = (now.tv_usec + (timeout % 1000) * 1000) * 1000;
            
            if (pthread_cond_timedwait(&m->not_full, &m->mutex, &ts) != 0) {
                pthread_mutex_unlock(&m->mutex);
                return RT_ETIMEOUT;
            }
        } else {
            pthread_cond_wait(&m->not_full, &m->mutex);
        }
    }
    
    char* dest = ((char*)m->buffer) + (m->tail * m->msg_size);
    memcpy(dest, buffer, size);
    m->tail = (m->tail + 1) % m->max_msgs;
    m->count++;
    
    pthread_cond_signal(&m->not_empty);
    pthread_mutex_unlock(&m->mutex);
    
    return RT_EOK;
}

rt_err_t rt_mq_recv(rt_mq_t mq, void* buffer, size_t size, int32_t timeout) {
    rt_mq_mock_t* m = (rt_mq_mock_t*)mq;
    if (!m || !buffer || size != m->msg_size) return RT_EINVAL;
    
    pthread_mutex_lock(&m->mutex);
    
    if (timeout == RT_WAITING_NO && m->count == 0) {
        pthread_mutex_unlock(&m->mutex);
        return RT_EEMPTY;
    }
    
    while (m->count == 0) {
        if (timeout == RT_WAITING_NO) {
            pthread_mutex_unlock(&m->mutex);
            return RT_EEMPTY;
        }
        
        if (timeout != RT_WAITING_FOREVER) {
            struct timespec ts;
            struct timeval now;
            gettimeofday(&now, NULL);
            ts.tv_sec = now.tv_sec + timeout / 1000;
            ts.tv_nsec = (now.tv_usec + (timeout % 1000) * 1000) * 1000;
            
            if (pthread_cond_timedwait(&m->not_empty, &m->mutex, &ts) != 0) {
                pthread_mutex_unlock(&m->mutex);
                return RT_ETIMEOUT;
            }
        } else {
            pthread_cond_wait(&m->not_empty, &m->mutex);
        }
    }
    
    char* src = ((char*)m->buffer) + (m->head * m->msg_size);
    memcpy(buffer, src, size);
    m->head = (m->head + 1) % m->max_msgs;
    m->count--;
    
    pthread_cond_signal(&m->not_full);
    pthread_mutex_unlock(&m->mutex);
    
    return RT_EOK;
}