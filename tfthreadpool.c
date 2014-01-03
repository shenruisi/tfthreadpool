//
//  tfthreadpool.c
//  tfcommon0.4
//
//  Created by yin shen on 11/5/12.
//
//

#include <stdio.h>
#include <pthread.h>
#include <assert.h>
#include <sys/time.h>
#include "tfthreadpool.h"

void *malloc(size_t);
void free(void *);
void *calloc(size_t, size_t);

#define TFTASK_FREE(t) do {free(t->cancel_point); t->cancel_point = NULL; free(t); t = NULL; } while (0);
#define TFTASK_INIT(t,m,a) do { t->next=NULL; t->method=m; t->args=a; t->cancel_point=NULL; pthread_mutex_init(&t->mtx, NULL);}while(0);
#define TFNULL_CHECK(o) do { if(NULL == o) return; }while(0);

tfthread_t *tfthreadpool_idle_thread(tfthreadpool_t *threadpool);

void _free_tasks(tftask_t_list tasks);

void *tfthreadpool_task_run(void *arg)
{
    tfthread_t *thread_t = (tfthread_t *)arg;
    
    for (;;) {
        pthread_mutex_lock(&thread_t->config.mtx);

        if (NULL == thread_t->tasks) {
            printf("@@@@@@@@@@ task_list null fixed");
            thread_t->status = tfthread_idle;
            pthread_cond_wait(&thread_t->config.cond, &thread_t->config.mtx);
        }
        else {
            tfmethod method = thread_t->tasks->method;

            (method)(thread_t->tasks->args, thread_t->tasks->cancel_point);
            
            if (NULL == thread_t->tasks->next) {
                thread_t->status = tfthread_idle;
                pthread_cond_wait(&thread_t->config.cond, &thread_t->config.mtx);
            }

            if (tfthread_dead == thread_t->status) {
                _free_tasks(thread_t->tasks);
                free(thread_t);
                return NULL;
            }

            if (tfthread_suspend == thread_t->status) {
                printf("{ `tag`thread suspend in thread  }\n");
                pthread_cond_wait(&thread_t->config.cond, &thread_t->config.mtx);
            }

            
            tftask_t *free_task = thread_t->tasks;
            thread_t->tasks = free_task->next;
            thread_t->tasks_count--;
            TFTASK_FREE(free_task);
        }

        pthread_mutex_unlock(&thread_t->config.mtx);
    }
}

void _free_tasks(tftask_t_list tasks){
    tftask_t *task_begin = tasks;
    
    while (task_begin->next!=NULL) {
        tftask_t *d = task_begin->next;
        task_begin->next=d->next;
        TFTASK_FREE(d);
    }
    
}


void empty_run(void *a, short *b) {}

void tfthreadpool_init(tfthreadpool_t *threadpool, int count)
{
    assert(count > 0);

    pthread_mutex_init(&threadpool->mtx, NULL);
    
    for (int i = 0; i < count; ++i) {
        pthread_attr_t  attr;
        pthread_t       posix_t_id;
        int             ret;

        ret = pthread_attr_init(&attr);

        ret = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

        tftask_t *task = (tftask_t *)malloc(sizeof(tftask_t));
        
        TFTASK_INIT(task,empty_run,NULL);

        tfthread_t *thread = (tfthread_t *)malloc(sizeof(tfthread_t));

        tfthread_config_t config = {PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER};
        thread->config = config;
        thread->tasks = task;
        thread->tasks_count = 1;
        thread->status = tfthread_idle;
        thread->next = NULL;
        

        if (NULL == threadpool->threads) {
            threadpool->threads = thread;
        } else {
            tfthread_t *thread_begin = threadpool->threads;

            while (NULL != thread_begin->next) {
                thread_begin = thread_begin->next;
            }

            thread_begin->next = thread;
        } 
        
        ret = pthread_create(&posix_t_id, &attr, tfthreadpool_task_run, thread);

        ret = pthread_attr_destroy(&attr);
    }
}

void tfthreadpool_free(tfthreadpool_t *threadpool)
{
    
    if (NULL == threadpool) {
        return;
    }

    if (NULL == threadpool->threads) {
        free(threadpool);
    } else {
        tfthread_t *thread_begin = threadpool->threads;

        
        if (NULL == thread_begin->next) {
            tfthread_t *free_thread = thread_begin;

            free_thread->status = tfthread_dead;

            pthread_cond_signal(&free_thread->config.cond);

        }

        while (NULL != thread_begin->next) {
            tfthread_t *free_thread = thread_begin;

            free_thread->status = tfthread_dead;
            
            thread_begin = thread_begin->next;

            pthread_cond_signal(&free_thread->config.cond);

            
        }

        free(threadpool);
        threadpool = NULL;
    }
}



void *tfthread_once_fun(void *arg)
{
    tftask_t *task = (tftask_t *)arg;

    tfmethod method = task->method;

    (method)(task->args, task->cancel_point);

    TFTASK_FREE(task);

    return NULL;
}

void tfthread_once(tfmethod method, void *args, tfcancel_flag cancel_point)
{
    pthread_attr_t  attr;
    pthread_t       posix_t_id;
    int             ret;

    *cancel_point = 0;
    tftask_t *task = (tftask_t *)malloc(sizeof(tftask_t));
    task->next = NULL;
    task->method = method;
    task->args = args;
    task->cancel_point = cancel_point;

    ret = pthread_attr_init(&attr);

    ret = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    int error = pthread_create(&posix_t_id, &attr, tfthread_once_fun, task);
    
    if (error) {
        
    }
    ret = pthread_attr_destroy(&attr);
}


void tfthreadpool_task_cancel(tfcancel_flag cancel_point)
{
    *cancel_point = 1;
}

tfthread_t *tfthreadpool_idle_thread(tfthreadpool_t *threadpool)
{
    
    if (NULL == threadpool->threads) {
        return NULL;
    }

    tfthread_t  *thread_begin = threadpool->threads;
    tfthread_t  *thread_task_less = NULL;

    int task_count = 65535;

    if (thread_begin->status >> 4) {
        printf("{ get idle thread in pool %s }\n", threadpool->name);
        thread_begin->status = tfthread_inuse;
        return thread_begin;
    } else {
        if (thread_begin->tasks_count < task_count) {
            thread_task_less = thread_begin;
            task_count = thread_begin->tasks_count;
        }
    }

    while (thread_begin->next) {
        if (thread_begin->status >> 4) {
            printf("{ get idle thread in pool %s }\n", threadpool->name);
            thread_begin->status = tfthread_inuse;
            return thread_begin;
        } else {
            if (thread_begin->tasks_count < task_count) {
                thread_task_less = thread_begin;
                task_count = thread_begin->tasks_count;
            }
        }

        thread_begin = thread_begin->next;
    }

    printf("{ no idle thread, add to busy thread in pool %s,task count %d}", threadpool->name, thread_task_less->tasks_count);

    return thread_task_less;
}

tftask_t *tfthreadpool_eat2(tfthreadpool_t *threadpool, tfmethod method, void *args)
{
    
    pthread_mutex_lock(&threadpool->mtx);
    tfthread_t *thread = NULL;

    thread = tfthreadpool_idle_thread(threadpool);

    if (NULL == thread) {
        thread = threadpool->threads;
    }

    tftask_t *task = (tftask_t *)malloc(sizeof(tftask_t));

    if (NULL == task) {
        printf("{ fail to alloc task point, ingore and return\n }");
        return NULL;
    }

    TFTASK_INIT(task, method, args)

    tfcancel_flag cancel_point = (tfcancel_flag)calloc(4, 0);
    *cancel_point = 0;
    task->cancel_point = cancel_point;
    thread->tasks_count++;

    tftask_t *task_begin = thread->tasks;

    if (NULL == task_begin) {
        thread->tasks = task;
    } else {
        while (NULL != task_begin->next) {
            task_begin = task_begin->next;
        }

        task_begin->next = task;
    }

    printf("{ threadpool %s running now }\n", threadpool->name);

    pthread_cond_signal(&thread->config.cond);
    
    pthread_mutex_unlock(&threadpool->mtx);
    
    return task;
}

void tfthreadpool_task_cancel2(tftask_t *task)
{
    TFNULL_CHECK(task);
    TFNULL_CHECK(task->cancel_point);
    *task->cancel_point = 1;

}

void tfthreadpool_suspend(tfthreadpool_t *threadpool)
{
    TFNULL_CHECK(threadpool->threads);

    tfthread_t *thread_begin = threadpool->threads;

    while (thread_begin) {
        printf("{ `tag`threadpool thread suspend }\n");
        thread_begin->status = tfthread_suspend;
        thread_begin = thread_begin->next;
    }
}

void tfthreadpool_resume(tfthreadpool_t *threadpool)
{
    TFNULL_CHECK(threadpool->threads);

    tfthread_t *thread_begin = threadpool->threads;

    while (thread_begin) {
        printf("{ `tag`threadpool thread resume }\n");
        thread_begin->status = tfthread_inuse;

        pthread_cond_signal(&thread_begin->config.cond);

        thread_begin = thread_begin->next;
    }
}

void print_tfthreadpool_status(tfthreadpool_t *threadpool){
    pthread_mutex_lock(&threadpool->mtx);
    
    tfthread_t *thread_begin = threadpool->threads;
    
    while (thread_begin) {
        printf("{ threadpool <%s> thread has tasks %d }\n",threadpool->name,thread_begin->tasks_count);
        
        thread_begin = thread_begin->next;
    }
    
    pthread_mutex_unlock(&threadpool->mtx);
}

int tfthreadpool_task_count(tfthreadpool_t *threadpool)
{
    tfthread_t *thread_begin = threadpool->threads;
    
    int count = 0;
    
    while (thread_begin) {
        count += thread_begin->tasks_count;
        thread_begin = thread_begin->next;
    }
    
    return count;
}
