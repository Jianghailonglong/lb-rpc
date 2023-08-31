#include "lbrss.pb.h"
#include "reactor.h"
#include "store_report.h"
#include "store_thread.h"


void thread_report(EventLoop *loop, int fd, void *args)
{
    // 1. 从queue里面取出需要report的数据(需要thread_queue)
    ThreadQueue<lbrss::ReportStatusRequest>* queue = ((Args*)args)->first;
    StoreReport *sr = ((Args*)args)->second;

    std::queue<lbrss::ReportStatusRequest> report_msgs;

    // 1.1 从消息队列中取出全部的消息元素集合
    queue->recv(report_msgs);
    while ( !report_msgs.empty() ) {
        lbrss::ReportStatusRequest msg = report_msgs.front();
        report_msgs.pop();
    
        // 2. 将数据存储到DB中(需要StoreReport)
        sr->store(msg);
    }
}

void *store_main(void *args)
{
    // 得到对应的thread_queue
    ThreadQueue<lbrss::ReportStatusRequest> *queue = (ThreadQueue<lbrss::ReportStatusRequest>*)args;

    // 定义事件触发机制
    EventLoop loop;

    // 定义一个存储对象
    StoreReport sr; 

    Args callback_args;
    callback_args.first = queue;
    callback_args.second = &sr;

    queue->set_loop(&loop);
    queue->set_callback(thread_report, &callback_args);

    // 启动事件监听
    loop.event_process();

    return NULL;
}

StoreThreadPool::StoreThreadPool(int thread_cnt)
{
    index_ = 0;
    queues_ = NULL;
    thread_cnt_ = thread_cnt;
    if (thread_cnt_ <= 0) {
        fprintf(stderr, "thread_cnt_ < 0\n");
        exit(1);
    }

    // 任务队列的个数和线程个数一致
    queues_ = new ThreadQueue<lbrss::ReportStatusRequest>* [thread_cnt];
    tids_ = new pthread_t[thread_cnt];

    int ret;
    for (int i = 0; i < thread_cnt; ++i) {
        // 创建一个线程
        printf("create %d thread\n", i);
        // 给当前线程创建一个任务消息队列
        queues_[i] = new ThreadQueue<lbrss::ReportStatusRequest>();
        ret = pthread_create(&tids_[i], NULL, store_main, queues_[i]);
        if (ret == -1) {
            perror("thread_pool, create thread");
            exit(1);
        }

        // 将线程脱离
        pthread_detach(tids_[i]);
    }

}

StoreThreadPool::~StoreThreadPool()
{
    if(queues_ != nullptr)
    {
        for(int i = 0; i < thread_cnt_; i++){
            if(queues_[i] != nullptr)
            {
                delete(queues_[i]);
                queues_[i] = nullptr;
            }
        }
        queues_ = nullptr;
    }
}


ThreadQueue<lbrss::ReportStatusRequest>* StoreThreadPool::get_thread()
{
    ThreadQueue<lbrss::ReportStatusRequest>* thread = queues_[index_++];
    
    if (index_ == thread_cnt_) {
        index_ = 0; 
    }

    return thread;
}
