#pragma once

#include <pthread.h>
#include "thread_queue.h"
#include "lbrss.pb.h"

typedef struct Args 
{
    ThreadQueue<lbrss::ReportStatusRequest>* first;
    StoreReport *second;
}StoreReportArgs;

class StoreThreadPool
{
public:
    // 构造，初始化线程池, 开辟thread_cnt个
    StoreThreadPool(int thread_cnt);

    ~StoreThreadPool();

    // 获取一个thread_queue
    ThreadQueue<lbrss::ReportStatusRequest>* get_thread();

private:

    // _queues是当前thread_pool全部的消息任务队列头指针
    // 每个线程对应一个queue
    ThreadQueue<lbrss::ReportStatusRequest> **queues_; 

    // 当前线程池中的线程个数
    int thread_cnt_;

    // 已经启动的全部thread编号
    pthread_t * tids_;

    // 当前选中的线程队列下标
    int index_;
};