#pragma once

#include "net_connection.h"
#include "message.h"
#include "event_loop.h"

class UDPClient: public NetConnection
{
public:
    UDPClient(EventLoop *loop, const char *ip, uint16_t port);
    ~UDPClient();

    void add_msg_router(int msgid, msg_callback *cb, void *user_data = NULL);

    virtual int send_message(const char *data, int msglen, int msgid);

    //处理消息
    void do_read();

private:

    int _sockfd;

    char _read_buf[MESSAGE_LENGTH_LIMIT];
    char _write_buf[MESSAGE_LENGTH_LIMIT];

    //事件触发
    EventLoop *_loop;

    //消息路由分发
    MsgRouter _router;
};