#pragma once

#include "reactor_buf.h"
#include "event_loop.h"
#include "net_connection.h"

// 一个tcp的连接信息
class TCPConn : public NetConnection
{
public:
    // 初始化TCPConn
    TCPConn(int connfd, EventLoop *loop);

    // 处理读业务
    void do_read();

    // 处理写业务
    void do_write();

    // 销毁tcp_conn
    void clean_conn();

    // 发送消息的方法
    int send_message(const char *data, int msglen, int msgid);

    // 获取fd
    int get_fd();

private:
    // 当前链接的fd
    int _connfd;
    // 该连接归属的event_poll
    EventLoop *_loop;
    // 输出buf
    OutputBuf obuf;     
    // 输入buf
    InputBuf ibuf;
};