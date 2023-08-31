#include "main_server.h"
#include <netdb.h>
#include "lbrss.pb.h"
#include "route_lb.h"

//--------- 全局资源 ----------
struct LBConfig lb_config;

// 与report_client通信的thread_queue消息队列
ThreadQueue<lbrss::ReportStatusRequest>* report_queue = NULL;
// 与dns_client通信的thread_queue消息队列
ThreadQueue<lbrss::GetRouteRequest>* dns_queue = NULL;

// 每个Agent UDP server的负载均衡器路由 RouteLB
RouteLB * r_lb[3];

static void create_route_lb()
{
    for (int i = 0; i < 3; i++) {
        int id = i + 1; // RouteLB的id从1开始计数
        r_lb[i]  = new RouteLB(id);
        if (r_lb[i] == NULL) {
            fprintf(stderr, "no more space to new RouteLB\n");
            exit(1);
        }
    }
}

static void init_lb_agent()
{
    // 1. 加载配置文件
    ConfigFile::setPath("./conf/lb_agent.conf");
    lb_config.probe_num = ConfigFile::instance()->GetNumber("loadbalance", "probe_num", 10);
    lb_config.start_port = ConfigFile::instance()->GetNumber("loadbalance", "start_port", 8888);
    lb_config.init_succ_cnt = ConfigFile::instance()->GetNumber("loadbalance", "init_succ_cnt", 180);
    lb_config.err_rate = ConfigFile::instance()->GetFloat("loadbalance", "err_rate", 0.1);
    lb_config.succ_rate = ConfigFile::instance()->GetFloat("loadbalance", "succ_rate", 0.92);
    lb_config.con_succ_limit = ConfigFile::instance()->GetNumber("loadbalance", "con_succ_limit", 10);
    lb_config.con_err_limit = ConfigFile::instance()->GetNumber("loadbalance", "con_err_limit", 10);
    lb_config.window_err_rate = ConfigFile::instance()->GetFloat("loadbalance", "window_err_rate", 0.7);
    lb_config.idle_timeout = ConfigFile::instance()->GetNumber("loadbalance", "idle_timeout", 15);
    lb_config.overload_timeout = ConfigFile::instance()->GetNumber("loadbalance", "overload_timeout", 15);
    lb_config.update_timeout = ConfigFile::instance()->GetNumber("loadbalance", "update_timeout", 15);

    // 2. 初始化3个route_lb模块
    create_route_lb();

    // 3. 加载本地ip，作为请求的调用者ip
    char my_host_name[1024];
    if (gethostname(my_host_name, 1024) == 0) {
        struct hostent *hd = gethostbyname(my_host_name);

        if (hd)
        {
            struct sockaddr_in myaddr;
            myaddr.sin_addr = *(struct in_addr*)hd->h_addr;
            lb_config.local_ip = ntohl(myaddr.sin_addr.s_addr);
        }
    }

    if (!lb_config.local_ip)  {
        struct in_addr inaddr;
        inet_aton("127.0.0.1", &inaddr);
        lb_config.local_ip = ntohl(inaddr.s_addr);
    }
}


int main(int argc, char **argv)
{
    // 1 初始化
    init_lb_agent();

    // 2 启动udp server服务,用来接收业务层(调用者/使用者)的消息
    start_UDP_servers();
    
    // 3 启动reporter client 线程
    report_queue = new ThreadQueue<lbrss::ReportStatusRequest>();
    if (report_queue == NULL) {
        fprintf(stderr, "create report queue error!\n");
        exit(1);
    }
    start_report_client();
    
    // 4 启动dns client 线程
    dns_queue = new ThreadQueue<lbrss::GetRouteRequest>();
    if (dns_queue == NULL) {
        fprintf(stderr, "create dns queue error!\n");
        exit(1);
    }
    start_dns_client();

    std::cout <<"done!" <<std::endl;
    while (1) {
        sleep(10);
    }

    return 0;
}