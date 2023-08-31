#pragma once

#include <ext/hash_map>
#include <list>
#include "host_info.h"
#include "lbrss.pb.h"

// ip + port为主键的 host信息集合
typedef __gnu_cxx::hash_map<uint64_t, HostInfo*>   host_map;   // key:uint64(ip+port), value:HostInfo
typedef __gnu_cxx::hash_map<uint64_t, HostInfo*>::iterator host_map_it;

// HostInfo list集合
typedef std::list<HostInfo*> host_list; 
typedef std::list<HostInfo*>::iterator host_list_it;


/*
 * 负载均衡算法核心模块
 * 针对一组(modid/cmdid)下的全部host节点的负载规则
 */
class LoadBalance {
public:
    LoadBalance(int modid, int cmdid):
        status_(PULLING),
        last_update_time_(0),
        modid_(modid),
        cmdid_(cmdid)
    {
        // LoadBalance 初始化构造
    }

    // 判断是否已经没有host在当前LB节点中
    bool empty() const;

    // 从当前的双队列中获取host信息
    int pick_one_host(lbrss::GetHostResponse &rsp);

    // 如果list中没有host信息，需要从远程的DNS Service发送GetRouteHost请求申请
    int pull();

    // 根据dns service远程返回的结果，更新_host_map
    void update(lbrss::GetRouteResponse &rsp);

    // report进行计数、操作队列
    void report(int ip, int port, int retcode);

    // 提交上报信息给到report client
    void commit();

    // 获取当前挂载下的全部host信息 添加到vec中
    void get_all_hosts(std::vector<HostInfo*> &vec);

    // 当前load_balance模块的状态
    enum STATUS
    {
        PULLING, // 正在从远程dns service通过网络拉取
        NEW      // 正在创建新的load_balance模块
    };
    STATUS status_;  // 当前的状态
    
    long last_update_time_; // 最后更新host_map时间戳

private:

    int modid_;
    int cmdid_;
    int access_cnt_;    // 请求次数，每次请求+1,判断是否超过probe_num阈值

    host_map host_map_; // 当前load_balance模块所管理的全部ip + port为主键的 host信息集合

    host_list idle_list_;       // 空闲队列
    host_list overload_list_;   // 过载队列
};