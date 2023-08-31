#pragma  once

#include <netinet/in.h>
#include "event_loop.h"
#include "net_connection.h"
#include "message.h"

class UDPServer :public NetConnection 
{
public:
    UDPServer(EventLoop *loop, const char *ip, uint16_t port);

    int send_message(const char *data, int msglen, int msgid);

    // 注册消息路由回调函数
    void add_msg_router(int msgid, msg_callback* cb, void *user_data = NULL);

    int get_fd();

    ~UDPServer();

    // 处理消息业务
    void do_read();
    
private:
    int _sockfd;

    char _read_buf[MESSAGE_LENGTH_LIMIT];
    char _write_buf[MESSAGE_LENGTH_LIMIT];

    // 事件触发
    EventLoop* _loop;

    // 服务端ip
    struct sockaddr_in _client_addr;
    socklen_t _client_addrlen;
    
    // 消息路由分发
    MsgRouter _router;
};