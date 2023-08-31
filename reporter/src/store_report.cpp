#include "store_report.h"
#include "reactor.h"
#include <string>
#include <unistd.h>

StoreReport::StoreReport()
{
    // 1 初始化
    // 1.1 多线程使用mysql需要先调用mysql_library_init
    mysql_library_init(0, NULL, NULL);

    // 1.2 初始化链接，和设置超时时间
    mysql_init(&db_conn_);

    //2 加载配置
    std::string db_host = ConfigFile::instance()->GetString("mysql", "db_host", "127.0.0.1");
    short db_port = ConfigFile::instance()->GetNumber("mysql", "db_port", 3306);
    std::string db_user = ConfigFile::instance()->GetString("mysql", "db_user", "root");
    std::string db_passwd = ConfigFile::instance()->GetString("mysql", "db_passwd", "123456");
    std::string db_name = ConfigFile::instance()->GetString("mysql", "db_name", "dns");
    std::string db_timeout = ConfigFile::instance()->GetString("mysql", "db_timeout", "30");
    bool db_reconnect = ConfigFile::instance()->GetBool("mysql", "db_reconnect", true);

    // 超时断开
    mysql_options(&db_conn_, MYSQL_OPT_CONNECT_TIMEOUT, (const void*)db_timeout.c_str());
    // 设置mysql链接断开后自动重连
    mysql_options(&db_conn_, MYSQL_OPT_RECONNECT, &db_reconnect);


    // 3 连接数据库
    if ( mysql_real_connect(&db_conn_, db_host.c_str(), db_user.c_str(), db_passwd.c_str(), db_name.c_str(), db_port, NULL, 0) == NULL)  {
        fprintf(stderr, "mysql real connect error\n");
        exit(1);
    }

    std::cout << "connect mysql succeed" << std::endl;
}

void StoreReport::store(lbrss::ReportStatusRequest req)
{
    for (int i = 0; i < req.results_size(); i++) {
        //一条report 调用记录
        const lbrss::HostCallResult &result = req.results(i);
        int overload = result.overload() ? 1: 0;
        char sql[1024];
        
        snprintf(sql, 1024, "INSERT INTO ServerCallStatus"
                "(modid, cmdid, ip, port, caller, succ_cnt, err_cnt, ts, overload) "
                "VALUES (%d, %d, %u, %u, %u, %u, %u, %u, %d) ON DUPLICATE KEY "
                "UPDATE succ_cnt = %u, err_cnt = %u, ts = %u, overload = %d",
                req.modid(), req.cmdid(), result.ip(), result.port(), req.caller(),
                result.succ(), result.err(), req.ts(), overload,
                result.succ(), result.err(), req.ts(), overload);

        mysql_ping(&db_conn_); //ping 测试一下，防止链接断开，会触发重新建立连接

        if (mysql_real_query(&db_conn_, sql, strlen(sql)) != 0) {
            fprintf(stderr, "Failed to Insert into ServerCallStatus %s\n", mysql_error(&db_conn_));
            continue;
        }

        std::cout << "insert ServerCallStatus succeed" << std::endl;
    }
}