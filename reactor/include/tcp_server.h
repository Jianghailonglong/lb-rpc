#pragma once

#include <netinet/in.h>
#include "event_loop.h"
#include "tcp_conn.h"
#include "message.h"
#include "thread_pool.h"

class TCPServer
{
public:
    TCPServer(EventLoop* loop, const char *ip, uint16_t port); 

    // 提供创建连接服务
    void do_accept();

    ~TCPServer();

    // 注册消息路由回调函数
    void add_msg_router(int msgid, msg_callback *cb, void *user_data = NULL) {
        router.register_msg_router(msgid, cb, user_data);
    }


private:
    int _sockfd; // 套接字
    struct sockaddr_in _connaddr; // 客户端连接地址
    socklen_t _addrlen; // 客户端连接地址长度

    // EventLoop epoll事件机制
    EventLoop* _loop;

public:
    //---- 客户端链接管理部分-----
    static void increase_conn(int connfd, TCPConn *conn);    // 新增一个新建的连接
    static void decrease_conn(int connfd);                    // 减少一个断开的连接
    static void get_conn_num(int *curr_conn);                 // 得到当前链接的刻度
    static TCPConn **conns;                                  // 全部已经在线的连接信息
    
    
    // ------- 创建链接/销毁链接 Hook 部分 -----

    // 设置连接的创建hook函数
    static void set_conn_start(conn_callback cb, void *args = NULL) {
        conn_start_cb = cb;
        conn_start_cb_args = args;
    }

    // 设置连接的销毁hook函数
    static void set_conn_close(conn_callback cb, void *args = NULL) {
        conn_close_cb = cb;
        conn_close_cb_args = args;
    }

    // 创建链接之后要触发的 回调函数
    static conn_callback conn_start_cb;
    static void *conn_start_cb_args;

    // 销毁链接之前要触发的 回调函数
    static conn_callback conn_close_cb;
    static void *conn_close_cb_args;

    // 获取当前server的线程池
    ThreadPool *get_thread_pool() {
        return _thread_pool;
    }

private:
    // TODO 
    // 从配置文件中读取
#define MAX_CONNS  10
    static int _max_conns;          // 最大client链接个数
    static int _curr_conns;         // 当前链接刻度
    static pthread_mutex_t _conns_mutex; // 保护_curr_conns刻度修改的锁

    // 线程池
    ThreadPool *_thread_pool;
public:
    //---- 消息分发路由 ----
    static MsgRouter router; 
};