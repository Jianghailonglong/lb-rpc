#include "tcp_server.h"
#include <string>
#include <string.h>
#include "config_file.h"


//回显业务的回调函数
void callback_busi(const char *data, uint32_t len, int msgid, NetConnection *conn, void *user_data)
{
    printf("callback_busi ...\n");
    //直接回显
    conn->send_message(data, len, msgid);
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
}

//客户端销毁的回调
void on_client_lost(NetConnection *conn, void *args)
{
    printf("connection is lost !\n");
}


int main() 
{
    EventLoop loop;

    //加载配置文件
    ConfigFile::setPath("./serv.conf");
    std::string ip = ConfigFile::instance()->GetString("reactor", "ip", "0.0.0.0");
    short port = ConfigFile::instance()->GetNumber("reactor", "port", 8888);

    printf("ip = %s, port = %d\n", ip.c_str(), port);

    TCPServer server(&loop, ip.c_str(), port);

    //注册消息业务路由
    server.add_msg_router(1, callback_busi);
    server.add_msg_router(2, print_busi);

    //注册链接hook回调
    server.set_conn_start(on_client_build);
    server.set_conn_close(on_client_lost);

    loop.event_process();

    return 0;
}