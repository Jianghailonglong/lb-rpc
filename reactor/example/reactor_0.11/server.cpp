#include "tcp_server.h"
#include <string>
#include <string.h>
#include "config_file.h"
#include <iostream>

TCPServer *server;

void print_lars_task(EventLoop *loop, void *args)
{
    printf("======= Active Task Func! ========\n");
    listen_fd_set fds;
    loop->get_listen_fds(fds);//不同线程的loop，返回的fds是不同的

    //可以向所有fds触发
    listen_fd_set::iterator it;
    //遍历fds
    for (it = fds.begin(); it != fds.end(); it++) {
        int fd = *it;
        std::cout << "fd: " << fd << std::endl;
        TCPConn *conn = TCPServer::conns[fd]; //取出fd
        if (conn != NULL) {
            int msgid = 101;
            const char *msg = "Hello I am a Task!";
            conn->send_message(msg, strlen(msg), msgid);
        }
    }
}

//回显业务的回调函数
void callback_busi(const char *data, uint32_t len, int msgid, NetConnection *conn, void *user_data)
{
    printf("callback_busi ...\n");
    // 直接回显
    conn->send_message(data, len, msgid);
    printf("conn param = %s\n", (const char *)conn->param);
}

//打印信息回调函数
void print_busi(const char *data, uint32_t len, int msgid, NetConnection *conn, void *user_data)
{
    printf("recv client: [%s]\n", data);
    printf("msgid: [%d]\n", msgid);
    printf("len: [%d]\n", len);
}


//新客户端创建的回调
void on_client_build(NetConnection *conn, void *args)
{
    int msgid = 101;
    const char *msg = "welcome! you online..";

    conn->send_message(msg, strlen(msg), msgid);

    // 将当前的net_connection 绑定一个自定义参数，供我们开发者使用
    const char *conn_param_test = "I am the conn for you!";
    conn->param = (void*)conn_param_test;

    // 创建链接成功之后触发任务
    server->get_thread_pool()->send_task(print_lars_task);
}

//客户端销毁的回调
void on_client_lost(NetConnection *conn, void *args)
{
    printf("connection is lost !\n");
}


int main() 
{
    EventLoop loop;

    // 加载配置文件
    ConfigFile::setPath("./serv.conf");
    std::string ip = ConfigFile::instance()->GetString("reactor", "ip", "0.0.0.0");
    short port = ConfigFile::instance()->GetNumber("reactor", "port", 8888);

    printf("ip = %s, port = %d\n", ip.c_str(), port);

    server = new TCPServer(&loop, ip.c_str(), port);

    // 注册消息业务路由
    server->add_msg_router(1, callback_busi);
    server->add_msg_router(2, print_busi);

    // 注册链接hook回调
    server->set_conn_start(on_client_build);
    server->set_conn_close(on_client_lost);


    loop.event_process();

    return 0;
}