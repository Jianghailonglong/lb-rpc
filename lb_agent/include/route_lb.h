#pragma once
#include "load_balance.h"

// key: modid+cmdid    value: LoadBalance
typedef __gnu_cxx::hash_map<uint64_t, LoadBalance*> route_map;
typedef __gnu_cxx::hash_map<uint64_t, LoadBalance*>::iterator route_map_it;

/*
 * 针对多组modid/cmdid，route_lb是管理多个load_balanace模块的
 * 目前设计有3个，和udp-server的数量一致，每个route_lb分别根据
 * modid/cmdid的值做hash，分管不同的modid/cmdid
 *
 * */
class RouteLB {
public:
    // 构造初始化
    RouteLB(int id);
    
    void reset_lb_status();

    // agent获取一个host主机，将返回的主机结果存放在rsp中
    int get_host(int modid, int cmdid, lbrss::GetHostResponse &rsp);

    int get_route(int modid, int cmdid, lbrss::GetRouteResponse &rsp);

    // 根据Dns Service返回的结果更新自己的route_lb_map
    int update_host(int modid, int cmdid, lbrss::GetRouteResponse &rsp);

    void report_host(lbrss::ReportRequest req);

private:
    route_map route_lb_map_;  // 当前route_lb下的管理的loadbalance
    pthread_mutex_t mutex_; 
    int id_; // 当前route_lb的ID编号
};