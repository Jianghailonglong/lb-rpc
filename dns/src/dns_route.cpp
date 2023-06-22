#include <iostream>
#include <cstdio>
#include <string>
#include <stdlib.h>
#include <unistd.h>
#include "reactor.h"
#include "dns_route.h"
#include "string.h"
#include "subscribe.h"

// 单例对象
Route *Route::instance_ = NULL;

// 用于保证创建单例的init方法只执行一次的锁
pthread_once_t Route::once_ = PTHREAD_ONCE_INIT;


Route::Route():version_(0)
{
    //1 初始化锁
    pthread_rwlock_init(&map_lock_, NULL);

    //2 初始化map
    data_pointer_ = new route_map();//RouterDataMap_A
    temp_pointer_ = new route_map();//RouterDataMap_B

    //3 链接数据库
    this->connect_db();

    //4 加载当前版本号
    if (this->load_version() == -1) {
        exit(1);
    }

    //4 查询数据库，创建_data_pointer 与 _temp_pointer 两个map
    this->build_maps();
}

void Route::connect_db() 
{
    // --- mysql数据库配置---
    std::string db_host = ConfigFile::instance()->GetString("mysql", "db_host", "127.0.0.1");
    short db_port = ConfigFile::instance()->GetNumber("mysql", "db_port", 3306);
    std::string db_user = ConfigFile::instance()->GetString("mysql", "db_user", "root");
    std::string db_passwd = ConfigFile::instance()->GetString("mysql", "db_passwd", "12345");
    std::string db_name = ConfigFile::instance()->GetString("mysql", "db_name", "dns");
    std::string db_timeout = ConfigFile::instance()->GetString("mysql", "db_timeout", "30");
    bool db_reconnect = ConfigFile::instance()->GetBool("mysql", "db_reconnect", true);


    mysql_init(&db_conn_);

    // 超时断开
    mysql_options(&db_conn_, MYSQL_OPT_CONNECT_TIMEOUT, (const void*)db_timeout.c_str());
    // 设置mysql链接断开后自动重连
    mysql_options(&db_conn_, MYSQL_OPT_RECONNECT, &db_reconnect);

    if (!mysql_real_connect(&db_conn_, db_host.c_str(), db_user.c_str(), db_passwd.c_str(), db_name.c_str(), db_port, NULL, 0)) {
        fprintf(stderr, "Failed to connect mysql\n");
        exit(1);
    }
}

void Route::build_maps()
{
    int ret = 0;

    snprintf(sql_, 1000, "SELECT * FROM RouteData;");
    ret = mysql_real_query(&db_conn_, sql_, strlen(sql_));
    if ( ret != 0) {
        fprintf(stderr, "failed to find any data, error %s\n", mysql_error(&db_conn_));
        exit(1);
    }

    // 得到结果集
    MYSQL_RES *result = mysql_store_result(&db_conn_);

    // 得到行数
    long line_num = mysql_num_rows(result);

    MYSQL_ROW row;
    for (long i = 0; i < line_num; i++) {
        row = mysql_fetch_row(result);
        int modID = atoi(row[1]);
        int cmdID = atoi(row[2]);
        unsigned ip = atoi(row[3]);
        int port = atoi(row[4]);

        // 组装map的key，有modID/cmdID组合
        uint64_t key = ((uint64_t)modID << 32) + cmdID;
        uint64_t value = ((uint64_t)ip << 32) + port;

        printf("modID = %d, cmdID = %d, ip = %u, port = %d\n", modID, cmdID, ip, port);

        // 插入到RouterDataMap_A中
        (*data_pointer_)[key].insert(value);
    }

    mysql_free_result(result);
}

// 获取modid/cmdid对应的host信息
host_set Route::get_hosts(int modid, int cmdid)
{
    host_set hosts;     

    // 组装key
    uint64_t key = ((uint64_t)modid << 32) + cmdid;

    pthread_rwlock_rdlock(&map_lock_);
    route_map_it it = data_pointer_->find(key);
    if (it != data_pointer_->end()) {
        // 找到对应的ip + port对
        hosts = it->second;
    }
    pthread_rwlock_unlock(&map_lock_);

    return hosts;
}


/*
 *  return 0, 表示 加载成功，version没有改变
 *         1, 表示 加载成功，version有改变
 *         -1 表示 加载失败
 * */
int Route::load_version()
{
    //这里面只会有一条数据
    snprintf(sql_, 1000, "SELECT version from RouteVersion WHERE id = 1;");

    int ret = mysql_real_query(&db_conn_, sql_, strlen(sql_));
    if (ret)
    {
        fprintf(stderr, "load version error: %s\n", mysql_error(&db_conn_));
        return -1;
    }

    MYSQL_RES *result = mysql_store_result(&db_conn_);
    if (!result)
    {
        fprintf(stderr, "mysql store result: %s\n", mysql_error(&db_conn_));
        return -1;
    }

    long line_num = mysql_num_rows(result);
    if (line_num == 0)
    {
        fprintf(stderr, "No version in table RouteVersion: %s\n", mysql_error(&db_conn_));
        return -1;
    }

    MYSQL_ROW row = mysql_fetch_row(result);
    // 得到version

    long new_version = atol(row[0]);

    if (new_version == this->version_)
    {
        // 加载成功但是没有修改
        return 0;
    }
    this->version_ = new_version;
    printf("now route version is %ld\n", this->version_);

    mysql_free_result(result);

    return 1;
}


// 加载全量RouteData到_temp_pointer
int Route::load_route_data() 
{
    temp_pointer_->clear();

    snprintf(sql_, 100, "SELECT * FROM RouteData;");

    int ret = mysql_real_query(&db_conn_, sql_, strlen(sql_));
    if (ret)
    {
        fprintf(stderr, "load version error: %s\n", mysql_error(&db_conn_));
        return -1;
    }

    MYSQL_RES *result = mysql_store_result(&db_conn_);
    if (!result)
    {
        fprintf(stderr, "mysql store result: %s\n", mysql_error(&db_conn_));
        return -1;
    }

    long line_num = mysql_num_rows(result);
    MYSQL_ROW row;
    for (long i = 0;i < line_num; ++i)
    {
        row = mysql_fetch_row(result);
        int modid = atoi(row[1]);
        int cmdid = atoi(row[2]);
        unsigned ip = atoi(row[3]);
        int port = atoi(row[4]);

        uint64_t key = ((uint64_t)modid << 32) + cmdid;
        uint64_t value = ((uint64_t)ip << 32) + port;

        (*temp_pointer_)[key].insert(value);
    }
    printf("load data to temp succ! size is %lu\n", line_num);

    mysql_free_result(result);

    return 0;
}


// 将temp_pointer的数据更新到data_pointer
void Route::swap()
{
    pthread_rwlock_wrlock(&map_lock_);
    route_map *temp = data_pointer_;
    data_pointer_ = temp_pointer_;
    temp_pointer_ = temp;
    pthread_rwlock_unlock(&map_lock_);
}


// 加载RouteChange得到修改的modid/cmdid，将结果放在vector中
void Route::load_changes(std::unordered_set<uint64_t> &change_list) 
{
    // 读取当前版本之前的全部修改 
    snprintf(sql_, 1000, "SELECT modid,cmdid FROM RouteChange WHERE version <= %ld;", version_);

    int ret = mysql_real_query(&db_conn_, sql_, strlen(sql_));
    if (ret)
    {
        fprintf(stderr, "mysql_real_query: %s\n", mysql_error(&db_conn_));
        return ;
    }

    MYSQL_RES *result = mysql_store_result(&db_conn_);
    if (!result)
    {
        fprintf(stderr, "mysql_store_result %s\n", mysql_error(&db_conn_));
        return ;
    }

    long lineNum = mysql_num_rows(result);
    if (lineNum == 0)
    {
        fprintf(stderr,  "No version in table ChangeLog: %s\n", mysql_error(&db_conn_));
        return ;
    }
    MYSQL_ROW row;
    for (long i = 0;i < lineNum; ++i)
    {
        row = mysql_fetch_row(result);
        int modid = atoi(row[0]);
        int cmdid = atoi(row[1]);
        uint64_t key = (((uint64_t)modid) << 32) + cmdid;
        change_list.insert(key);
    }
    mysql_free_result(result);    
}

// 删除RouteChange的全部修改记录数据,remove_all为全部删除
// 否则默认删除当前版本之前的全部修改
void Route::remove_changes(bool remove_all)
{
    if (remove_all == false)
    {
        snprintf(sql_, 1000, "DELETE FROM RouteChange WHERE version <= %ld;", version_);
    }
    else
    {
        snprintf(sql_, 1000, "DELETE FROM RouteChange;");
    }
    int ret = mysql_real_query(&db_conn_, sql_, strlen(sql_));
    if (ret != 0)
    {
        fprintf(stderr, "delete RouteChange: %s\n", mysql_error(&db_conn_));
        return ;
    } 

    return;
}

// 后台线程周期性检查db的route信息的更新变化业务
void *check_route_changes(void *args)
{
    int wait_time = ConfigFile::instance()->GetNumber("dns", "back_sche_time", 10); // 10s自动修改一次
    long last_load_time = time(NULL);

    // 清空全部的RouteChange
    Route::instance()->remove_changes(true);

    // 1 判断是否有修改
    while (true) {
        // 定时每秒检查RouteData表版本号是否发生变化
        sleep(1);
        long current_time = time(NULL);

        // 1.1 加载RouteVersion得到当前版本号
        int ret = Route::instance()->load_version();
        if (ret == 1) {
            // version改版 有modid/cmdid修改
            // 2 如果有修改

            // 2.1 将最新的RouteData加载到_temp_pointer中
            if (Route::instance()->load_route_data() == 0) {
                // 2.2 更新_temp_pointer数据到_data_pointer map中
                Route::instance()->swap();
                last_load_time = current_time; // 更新最后加载时间
            }

            // 2.3 获取被修改的modid/cmdid对应的订阅客户端,进行推送   
            // TODO，获取改变的modid/cmdid，是不是可以使用SET来简化      
            std::unordered_set<uint64_t> changes;
            Route::instance()->load_changes(changes);

            // 推送
            SubscribeList::instance()->publish(changes);

            // 2.4 删除当前版本之前的修改记录
            Route::instance()->remove_changes();

        }
        else {
            // 3 如果没有修改
            if (current_time - last_load_time >= wait_time) {
                // 3.1 超时,加载最新的temp_pointer
                if (Route::instance()->load_route_data() == 0) {
                    // 3.2 _temp_pointer数据更新到_data_pointer map中
                    Route::instance()->swap();
                    last_load_time = current_time;
                }
            }
        }
    }

    return NULL;
}