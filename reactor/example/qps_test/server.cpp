#include <string>
#include <string.h>
#include "config_file.h"
#include "tcp_server.h"
#include "echoMessage.pb.h"

//回显业务的回调函数
void callback_busi(const char *data, uint32_t len, int msgid, NetConnection *conn, void *user_data)
{
    qps_test::EchoMessage request, response;  

    //解包，确保data[0-len]是一个完整包
    request.ParseFromArray(data, len); 

    //设置新pb包
    response.set_id(request.id());
    response.set_content(request.content());

    //序列化
    std::string responseString;
    response.SerializeToString(&responseString);

    conn->send_message(responseString.c_str(), responseString.size(), msgid);
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

    // 注册消息业务路由
    server.add_msg_router(1, callback_busi);

    loop.event_process();

    return 0;
}