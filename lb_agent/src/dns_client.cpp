#include "reactor.h"
#include "main_server.h"
#include <pthread.h>

static void conn_init(NetConnection *conn, void *args)
{
    // 与dns service建立连接需要加锁操作
    for (int i = 0; i < 3; i++) {
        r_lb[i]->reset_lb_status();
    }
}

// 只要thread_queue有数据，loop就会触发此回调函数来处理业务
void new_dns_request(EventLoop *loop, int fd, void *args)
{
    TCPClient *client = (TCPClient*)args;

    // 1. 将请求数据从thread_queue中取出，
    std::queue<lbrss::GetRouteRequest>  msgs;

    // 2. 将数据放在queue队列中
    dns_queue->recv(msgs);
    
    // 3. 遍历队列，通过client依次将每个msg发送给reporter service
    while (!msgs.empty()) {
        lbrss::GetRouteRequest req = msgs.front();
        msgs.pop();

        std::string requestString;
        req.SerializeToString(&requestString);

        // client 发送数据
        client->send_message(requestString.c_str(), requestString.size(), lbrss::ID_GetRouteRequest);
    }

}

/*
 * 处理远程dns service回复的modid/cmdid对应的路由信息
 * */
void deal_recv_route(const char *data, uint32_t len, int msgid, NetConnection *net_conn, void *user_data)
{
    lbrss::GetRouteResponse rsp;

    //解析远程消息到proto结构体中
    rsp.ParseFromArray(data, len);

    int modid = rsp.modid();
    int cmdid = rsp.cmdid();
    int index = (modid+cmdid) % 3; // 改成一致性hash

    // 将该modid/cmdid交给一个route_lb处理 将rsp中的hostinfo集合加入到对应的route_lb中
    r_lb[index]->update_host(modid, cmdid, rsp);
}

void *dns_client_thread(void* args)
{
    printf("dns client thread start\n");
    EventLoop loop;

    // 1 加载配置文件得到dns service ip + port
    std::string ip = ConfigFile::instance()->GetString("dns-server", "ip", "");
    short port = ConfigFile::instance()->GetNumber("dns-server", "port", 0);

    // 2 创建客户端
    TCPClient client(&loop, ip.c_str(), port, "dns client");

    // 3 将thread_queue消息回调事件，绑定到loop中
    dns_queue->set_loop(&loop);
    dns_queue->set_callback(new_dns_request, &client);
    
    client.set_conn_start(conn_init);

    // 4 设置当收到dns service回执的消息ID_GetRouteResponse处理函数
    client.add_msg_router(lbrss::ID_GetRouteResponse, deal_recv_route);
        
    // 5 设置链接成功/链接断开重连成功之后，通过conn_init来清理之前的任务

    // 6 启动事件监听
    loop.event_process(); 

    return NULL;
}


void start_dns_client()
{
    //开辟一个线程
    pthread_t tid;

    //启动线程业务函数
    int ret = pthread_create(&tid, NULL, dns_client_thread, NULL);
    if (ret == -1) {
        perror("pthread_create");
        exit(1);
    }

    //设置分离模式
    pthread_detach(tid);
}