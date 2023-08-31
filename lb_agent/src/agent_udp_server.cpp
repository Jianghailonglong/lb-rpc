#include "reactor.h"
#include "main_server.h"
#include <fstream>


void print_log(const char *log_message)
{
    std::string log_file = "example.log";
    
    // 打开文件日志
    std::ofstream ofs(log_file, std::ios_base::out | std::ios_base::app);
    
    // 写入日志消息
    if (ofs.is_open()) {
        ofs << std::string(log_message) << std::endl;
        ofs.close();
    } else {
        std::cerr << "Failed to open log file: " << log_file << std::endl;
    }
}

static void get_host_cb(const char *data, uint32_t len, int msgid, NetConnection *net_conn, void *user_data)
{
    print_log("get_host_cb is called ....\n");

    // 1、解析api发送的请求包
    lbrss::GetHostRequest req;         

    req.ParseFromArray(data, len);

    int modid = req.modid();
    int cmdid = req.cmdid();

    // 2、设置回执消息
    lbrss::GetHostResponse rsp;
    rsp.set_seq(req.seq());
    rsp.set_modid(modid);
    rsp.set_cmdid(cmdid);

    RouteLB *route_lb_ptr = (RouteLB*)user_data;
    
    // 3、调用route_lb的获取host方法，得到rsp返回结果
    route_lb_ptr->get_host(modid, cmdid, rsp);

    // 4、打包回执给api消息
    std::string responseString; 
    rsp.SerializeToString(&responseString);
    net_conn->send_message(responseString.c_str(), responseString.size(), lbrss::ID_GetHostResponse);
}

static void get_route_cb(const char *data, uint32_t len, int msgid, NetConnection *net_conn, void *user_data)
{
    print_log("get_route_cb is called ....\n");

    // 解析api发送的请求包
    lbrss::GetRouteRequest req;         

    req.ParseFromArray(data, len);
    int modid = req.modid();
    int cmdid = req.cmdid();

    // 设置回执消息
    lbrss::GetRouteResponse rsp;
    rsp.set_modid(modid);
    rsp.set_cmdid(cmdid);

    RouteLB *route_lb_ptr = (RouteLB*)user_data;
    
    // 调用route_lb的获取host方法，得到rsp返回结果
    route_lb_ptr->get_route(modid, cmdid, rsp);

    // 打包回执给api消息
    std::string responseString; 
    rsp.SerializeToString(&responseString);
    net_conn->send_message(responseString.c_str(), responseString.size(), lbrss::ID_API_GetRouteResponse);
    
}

static void report_cb(const char *data, uint32_t len, int msgid, NetConnection *net_conn, void *user_data)
{
    print_log("report_cb is called ....\n");

    lbrss::ReportRequest req;
    
    req.ParseFromArray(data, len);

    RouteLB *route_lb_ptr = (RouteLB*)user_data;
    route_lb_ptr->report_host(req);
}

void *agent_server_main(void *args)
{
    long index = (long)(args);
    // todo 配置文件读取
    short port = index + lb_config.start_port;
    EventLoop loop;

    UDPServer server(&loop, "0.0.0.0", port);

    // 给server注册消息分发路由业务, 每个udp拥有一个对应的route_lb
    server.add_msg_router(lbrss::ID_GetHostRequest, get_host_cb,  r_lb[port-lb_config.start_port]);
    
    // 给server注册消息分发路由业务，针对ID_ReportRequest处理
    server.add_msg_router(lbrss::ID_ReportRequest, report_cb, r_lb[port-lb_config.start_port]);

    // 给server注册消息分发路由业务，针对ID_API_GetRouteRequest处理
    server.add_msg_router(lbrss::ID_API_GetRouteRequest, get_route_cb, r_lb[port-lb_config.start_port]);

    printf("agent UDP server :port %d is started...\n", port);
    loop.event_process();

    return NULL;
}

void start_UDP_servers(void)
{
    for (long i = 0; i < 3; i ++) {
        pthread_t tid;
        
        int ret = pthread_create(&tid, NULL, agent_server_main, (void*)i);
        if (ret == -1) {
            perror("pthread_create");
            exit(1);
        }

        pthread_detach(tid);
    }

}