#include "udp_server.h"
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

int main() 
{
    EventLoop loop;

    //加载配置文件
    ConfigFile::setPath("./serv.conf");
    std::string ip = ConfigFile::instance()->GetString("reactor", "ip", "0.0.0.0");
    short port = ConfigFile::instance()->GetNumber("reactor", "port", 8888);

    printf("ip = %s, port = %d\n", ip.c_str(), port);

    UDPServer server(&loop, ip.c_str(), port);

    //注册消息业务路由
    server.add_msg_router(1, callback_busi);

    loop.event_process();

    return 0;
}