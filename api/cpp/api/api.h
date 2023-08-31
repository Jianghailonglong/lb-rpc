#pragma once

#include "reactor.h"
#include <string>
#include <vector>

typedef std::pair<std::string, int> ip_port;
typedef std::vector<ip_port> route_set;
typedef route_set::iterator route_set_it;

class APIClient
{
public:
    APIClient();
    ~APIClient();

    int reg_init(int modid, int cmdid);

    // 系统获取host信息 得到可用host的ip和port
    int get_host(int modid, int cmdid, std::string& ip, int &port);

    // 系统获取route信息 得到当前modid/cmdid所有host的ip和port
    int get_route(int modid, int cmdid, route_set &route);

    // 系统上报host调用信息
    void report(int modid, int cmdid, const std::string &ip, int port, int retcode);

private:
    int sockfd_[3];     // 3个udp socket fd 对应agent 3个udp server
    uint32_t seqid_;    // 消息的序列号
};