//
//  tfthreadpool.h
//  tfcommon0.4
//
//  Created by yin shen on 11/5/12.
//
//

#ifndef tfcommon_tfthreadpool_h
#define tfcommon_tfthreadpool_h

/*!
 @file          tfthreadpool
 @abstract      the file provides functions to add tasks to a thread pool.
 @discussion    version 0.4
 @author        shenyin
 */

#define TFTASK_TEST_CANCEL(flag,...) if(flag) {printf("{\nenter cancel point in \n%s->%s->%d\nreturn immediately\n}\n",__FILE__,__FUNCTION__,__LINE__); __VA_ARGS__; return;}
#define TFTASK_TEST_CANCEL_AND_RETURN(flag,ret) if(flag) return ret;



typedef short* tfcancel_flag;

/*!
 @function      tfmethod
 @abstract      "tfthreadpool_throw_in" 载入的函数
 @param         void * 载入函数指针
 @param         short * 取消标志位*/
typedef void (*tfmethod)(void *,short *);


/*!
 @struct        tfthread_config_t
 @abstract      mutext信号量，cond信号量*/
typedef struct tfthread_config_t{
    pthread_mutex_t mtx;
    pthread_cond_t cond;
}tfthread_config_t;

/*!
 @struct        tftask_t
 @abstract      依附于每个线程的task列表*/
typedef struct tftask_t{
    void *args;
    tfmethod method;
    short *cancel_point;
    pthread_mutex_t mtx;
    struct tftask_t *next;
}tftask_t,*tftask_t_list;

/*!
 @enum          tfthread_status
 @constant      tfthread_init       线程初始化
 @constant      tfthread_idle       空闲线程
 @constant      tfthread_inuse      线程在使用
 @constant      tfthread_dead       线程已死亡
 @constant      tfthread_suspend    线程挂起*/
enum  tfthread_status{
    tfthread_idle=0xf0,
    tfthread_init=0x01,
    tfthread_inuse=0x02,
    tfthread_dead=0x03,
    tfthread_suspend=0x04
};

/*!
 @struct        tfthread_t
 @abstract      tftask  @struct tftask_t
 @abstract      status  线程状态 @enum tfthread_status
 @abstract      tasks_count  线程任务数
 @abstract      tasks  线程任务链表
 @abstract      config  线程锁
 @abstract      next  线程next指针*/
typedef struct tfthread_t{
    short status;
    int tasks_count;
    tftask_t_list tasks;
    tfthread_config_t config;
    struct tfthread_t *next;
}tfthread_t,*tfthread_t_list;

/*!
 @struct        tfthreadpool_t
 @abstract      threads  线程池线程列表
 @abstract      mtx  线程池操作锁
 @abstract      name  线程池名*/
typedef struct tfthreadpool_t{
    tfthread_t_list threads;
    pthread_mutex_t mtx;
    char *name;
}tfthreadpool_t;

/*!
 @function      tfthreadpool_init
 @abstract      在线程池中创建固定数量的线程，每个线程初始化完毕后将会被挂起
 @param         count 常驻线程迟线程数*/
void tfthreadpool_init(tfthreadpool_t *,int);

/*!
 @function      tfthreadpool_free
 @abstract      释放线程池中的所有资源*/
void tfthreadpool_free(tfthreadpool_t *);

/*!
 @function      tfthead_once
 @abstract      单独创建线程 method执行完后即销毁*/
void tfthread_once(tfmethod,void *,tfcancel_flag);

/*!
 @function      tfthreadpool_eat2
 @abstract      将需要执行的函数丢入线程池，线程池将会根据mode参数进行线程分配
 @param         method          @struct tfmethod
 @param         arg             tfmethod参数*/
tftask_t *tfthreadpool_eat2(tfthreadpool_t *,tfmethod , void *);

/*!
 @function      tfthreadpool_task_cancel
 @abstract      将cancel_point 所对应的线程取消
 @param         method          @struct tfmethod
 @param         arg             tfmethod参数
 @param         mode            @enum tftask_type
 @param         cancel_point    task取消标识符*/
void tfthreadpool_task_cancel2(tftask_t *);

/*!
 @function      tfthreadpool_suspend
 @abstract      将threadpool中的线程全部挂起*/
void tfthreadpool_suspend(tfthreadpool_t *);

/*!
 @function      tfthreadpool_resume
 @abstract      将threadpool中的线程继续执行*/
void tfthreadpool_resume(tfthreadpool_t *);

/*!
 @function      print_tfthreadpool_status
 @abstract      将threadpool打印线程池中线程状态*/
void print_tfthreadpool_status(tfthreadpool_t *);

/*!
 @function      print_tfthreadpool_status
 @abstract      将threadpool打印线程池中线程状态*/
void print_tfthreadpool_status(tfthreadpool_t *);

int tfthreadpool_task_count(tfthreadpool_t *);

#endif
