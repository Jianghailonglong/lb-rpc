#include "reactor.h"
#include "lbrss.pb.h"
#include "store_report.h"
#include "store_thread.h"
#include <string>

StoreThreadPool *thread_pool = NULL;

void get_report_status(const char *data, uint32_t len, int msgid, NetConnection *conn, void *user_data)
{
    lbrss::ReportStatusRequest req;

    req.ParseFromArray(data, len);

    // 轮询将消息平均发送到每个线程的消息队列中
    thread_pool->get_thread()->send(req);
}


int main(int argc, char **argv)
{
    printf("reporter service ....\n");
    EventLoop loop;

    // 加载配置文件
    ConfigFile::setPath("./conf/reporter.conf");
    std::string ip = ConfigFile::instance()->GetString("reactor", "ip", "0.0.0.0");
    short port = ConfigFile::instance()->GetNumber("reactor", "port", 7779);

    // 创建tcp server
    TCPServer server(&loop, ip.c_str(), port);

    // 创建线程池
    // 为了防止在业务中出现io阻塞，那么需要启动一个线程池对IO进行操作的，接受业务的请求存储消息
    int thread_cnt = ConfigFile::instance()->GetNumber("reporter", "db_thread_cnt", 3);
    std::cout << "reporter service thread_num: " << thread_cnt << std::endl;
    if(thread_cnt > 0)
    {
        thread_pool = new StoreThreadPool(thread_cnt);
        if(thread_pool == NULL)
        {
            fprintf(stderr, "Reporter Service create new StoreThreadPool error\n");
            exit(1);
        }
    }


    // 添加数据上报请求处理的消息分发处理业务
    server.add_msg_router(lbrss::ID_ReportStatusRequest, get_report_status);
      
    // 为了防止在业务中出现io阻塞，那么需要启动一个线程池对IO进行操作的，接受业务的请求存储消息
    // create_reportdb_threads();

    // 启动事件监听
    loop.event_process(); 

    return 0;
}