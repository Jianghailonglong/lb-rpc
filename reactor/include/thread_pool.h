#pragma once

#include <pthread.h>
#include "task_msg.h"
#include "thread_queue.h"

class ThreadPool
{
public:
    // 构造，初始化线程池, 开辟thread_cnt个
    ThreadPool(int thread_cnt);

    // 获取一个thread_queue
    ThreadQueue<TaskMsg>* get_thread();

    // 发送一个task任务给thread_pool里的全部thread
    void send_task(task_func func, void *args = NULL);

private:

    // _queues是当前thread_pool全部的消息任务队列头指针
    // 每个线程对应一个queue
    ThreadQueue<TaskMsg> ** _queues; 

    // 当前线程池中的线程个数
    int _thread_cnt;

    // 已经启动的全部thread编号
    pthread_t * _tids;

    // 当前选中的线程队列下标
    int _index;
};