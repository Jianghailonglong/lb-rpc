#include "load_balance.h"
#include "main_server.h"

//判断是否已经没有host在当前LB节点中
bool LoadBalance::empty() const
{
    return host_map_.empty();
}

// 从一个host_list中得到一个节点放到GetHostResponse 的HostInfo中
static void get_host_from_list(lbrss::GetHostResponse &rsp, host_list &l)
{
    // 选择list中第一个节点
    HostInfo *host = l.front();

    // HostInfo自定义类型,proto3并没提供set方法,而是通过mutable_接口返回HostInfo的指针,可根据此指针进行赋值操作
    lbrss::HostInfo* hip = rsp.mutable_host();

    hip->set_ip(host->ip);
    hip->set_port(host->port);

    // 将上面处理过的第一个节点放在队列的末尾
    l.pop_front();
    l.push_back(host);
}

// 从两个队列中获取一个host给到上层
int LoadBalance::pick_one_host(lbrss::GetHostResponse &rsp)
{
    // 1 判断idle_list_队列是否已经空,如果空表示没有空闲节点 
    if (idle_list_.empty() == true) {
        // 1.1 判断是否已经超过了probe_num
        if (access_cnt_ >= lb_config.probe_num) {
            access_cnt_ = 0;

            // 从 overload_list中选择一个已经过载的节点
            get_host_from_list(rsp, overload_list_);
        }
        else {
            // 明确返回给API层，已经过载了
            ++access_cnt_;
            return lbrss::RET_OVERLOAD;                
        }
    }
    else {
        // 2 判断过载列表是否为空
        if (!overload_list_.empty()) {
            // 2.1 有部分节点过载
            // 过载队列非空，且达到探测阈值
            if (access_cnt_ >= lb_config.probe_num) {
                access_cnt_ = 0;
                get_host_from_list(rsp, overload_list_);
                return lbrss::RET_SUCC;
            }
        }
        // 2.2 从空闲队列取节点
        ++access_cnt_;
        get_host_from_list(rsp, idle_list_);
    }

    return lbrss::RET_SUCC;
}

// 如果list中没有host信息，需要从远程的DNS Service发送GetRouteHost请求申请
int LoadBalance::pull()
{
    // 请求dns service请求
    lbrss::GetRouteRequest route_req;
    route_req.set_modid(modid_);
    route_req.set_cmdid(cmdid_);
    
    // 通过dns client的thread queue发送请求
    dns_queue->send(route_req);

    // 由于远程会有一定延迟，所以应该给当前的load_balance模块标记一个正在拉取的状态
    status_ = PULLING;  

    return 0;
}

// 根据dns service远程返回的结果，更新host_map_
void LoadBalance::update(lbrss::GetRouteResponse &rsp)
{
    long current_time = time(NULL);

    // 确保dns service返回的结果有host信息
    assert(rsp.host_size() != 0);

    std::set<uint64_t> remote_hosts;
    std::set<uint64_t> need_delete;

    // 1. 插入新增的host信息 到host_map_中
    for (int i = 0; i < rsp.host_size(); i++) {
        // 1.1 得到rsp中的一个host
        const lbrss::HostInfo & h = rsp.host(i);

        // 1.2 得到ip+port的key值
        uint64_t key = ((uint64_t)h.ip() << 32) + h.port();

        remote_hosts.insert(key);

        // 1.3 如果自身的host_map_找不到当下的key，说明是新增 
        // TODO：如果不是新增的，但是可能发生host可能发生改变
        if (host_map_.find(key) == host_map_.end()) {
            // 新增
            HostInfo *hi = new HostInfo(h.ip(), h.port(), lb_config.init_succ_cnt);
            if (hi == NULL) {
                fprintf(stderr, "new HostInfo error!\n");
                exit(1);
            }
            host_map_[key] = hi;

            // 新增的host信息加入到 空闲列表中
            idle_list_.push_back(hi);
        }
    }
   
    // 2. 删除减少的host信息 从host_map_中
    // 2.1 得到哪些节点需要删除
    for (host_map_it it = host_map_.begin(); it != host_map_.end(); it++) {
        if (remote_hosts.find(it->first) == remote_hosts.end())  {
            // 该key在host_map中存在，而在远端返回的结果集不存在，需要锁定被删除
            need_delete.insert(it->first);
        }
    }

    // 2.2 删除
    for (std::set<uint64_t>::iterator it = need_delete.begin();
        it != need_delete.end(); it++)  {
        uint64_t key = *it;

        HostInfo *hi = host_map_[key];

        if (hi->overload == true) {
            // 从过载列表中删除
            overload_list_.remove(hi);
        } 
        else {
            // 从空闲列表删除
            idle_list_.remove(hi);
        }
        delete hi;
    }

    // 更新最后update时间
    last_update_time_ = current_time;
    // 重置状态为NEW
    status_ = NEW;

}

// 上报当前host主机调用情况给远端repoter service
// 1、主机计数统计
// 2、idle、overload队列更新
// 3、窗口机制更新
void LoadBalance::report(int ip, int port, int retcode)
{
    // 定义当前时间
    long current_time = time(NULL);

    uint64_t key = ((uint64_t)ip << 32)  + port;

    if (host_map_.find(key) == host_map_.end()) {
        return;
    }

    // 1 计数统计
    HostInfo *hi = host_map_[key];
    if (retcode == lbrss::RET_SUCC) { 
        // 更新虚拟成功、真实成功次数
        hi->vsucc++;
        hi->rsucc++;

        // 连续成功增加
        hi->con_succ++; 
        // 连续失败次数归零
        hi->con_err = 0;
    }
    else {
        // 更新虚拟失败、真实失败次数 
        hi->verr++;
        hi->rerr++;

        // 连续失败个数增加
        hi->con_err++;
        // 连续成功次数归零
        hi->con_succ = 0;
    }

    // 2.检查节点状态

    // 检查idle节点是否满足overload条件
    // 或者overload节点是否满足idle条件
     
    // --> 如果是idle节点,则只有调用失败才有必要判断是否达到overload条件
    if (hi->overload == false && retcode != lbrss::RET_SUCC) {

        bool overload = false;

        // idle节点，检查是否达到判定为overload的状态条件 
        // (1).计算失败率,如果大于预设值失败率，则为overload
        double err_rate = hi->verr * 1.0 / (hi->vsucc + hi->verr);

        if (err_rate > lb_config.err_rate) {
            overload = true;            
        }

        // (2).连续失败次数达到阈值，判定为overload
        if( overload == false && hi->con_err >= (uint32_t)lb_config.con_err_limit) {
            overload = true;
        }

        // 判定overload需要做的更改流程
        if (overload) {
            struct in_addr saddr;
            
            saddr.s_addr = htonl(hi->ip);
            printf("[%d, %d] host %s:%d change overload, succ %u err %u\n", 
                    modid_, cmdid_, inet_ntoa(saddr), hi->port, hi->vsucc, hi->verr);

            // 设置hi为overload状态 
            hi->set_overload();
            // 移出_idle_list,放入_overload_list
            idle_list_.remove(hi);
            overload_list_.push_back(hi);
            return;
        }

    }
    // --> 如果是overload节点，则只有调用成功才有必要判断是否达到idle条件
    else if (hi->overload == true && retcode == lbrss::RET_SUCC) {
        bool idle = false;

        // overload节点，检查是否达到回到idle状态的条件
        // (1).计算成功率，如果大于预设值的成功率，则为idle
        double succ_rate = hi->vsucc * 1.0 / (hi->vsucc + hi->verr);

        if (succ_rate > lb_config.succ_rate) {
            idle = true;
        }

        // (2).连续成功次数达到阈值，判定为idle
        if (idle == false && hi->con_succ >= (uint32_t)lb_config.con_succ_limit) {
            idle = true;
        }

        // 判定为idle需要做的更改流程
        if (idle) {
            struct in_addr saddr;
            saddr.s_addr = htonl(hi->ip);
            printf("[%d, %d] host %s:%d change idle, succ %u err %u\n", 
                    modid_, cmdid_, inet_ntoa(saddr), hi->port, hi->vsucc, hi->verr);

            // 设置为idle状态
            hi->set_idle();
            // 移出overload_list, 放入_idle_list
            overload_list_.remove(hi);
            idle_list_.push_back(hi);
            return;
        }
    }
    
    // 窗口检查和超时机制 
    if (hi->overload == false) {
        // 节点是idle状态
        if (current_time - hi->idle_ts >= lb_config.idle_timeout) {
            // 时间窗口到达，需要对idle节点清理负载均衡数据
            if (hi->check_window() == true)   {
                // 将此节点设置为过载
                struct in_addr saddr;
                saddr.s_addr = htonl(hi->ip);

                printf("[%d, %d] host %s:%d change to overload cause windows err rate too high, read succ %u, real err %u\n",
                        modid_, cmdid_, inet_ntoa(saddr), hi->port, hi->rsucc, hi->rerr);

                // 设置为overload状态
                hi->set_overload();
                // 移出idle_list,放入overload_list
                idle_list_.remove(hi);
                overload_list_.push_back(hi);
            }
            else {
                // 重置窗口,回复负载默认信息
                hi->set_idle();
            }
        }
    }
    else {
        // 节点为overload状态
        // 那么处于overload的状态时间是否已经超时
        if (current_time - hi->overload_ts >= lb_config.overload_timeout) {
            struct in_addr saddr;
            saddr.s_addr = htonl(hi->ip);
            printf("[%d, %d] host %s:%d reset to idle, vsucc %u,  verr %u\n",
                    modid_, cmdid_, inet_ntoa(saddr), hi->port, hi->vsucc, hi->verr);

            hi->set_idle();
            // 移出overload_list, 放入_idle_list
            overload_list_.remove(hi);
            idle_list_.push_back(hi);
        }
    }
}

// 提交host的调用结果给远程reporter service
void LoadBalance::commit()
{
    if (this->empty() == true) {
        return;
    }

    // 1. 封装请求消息
    lbrss::ReportStatusRequest req;
    req.set_modid(modid_);
    req.set_cmdid(cmdid_);
    req.set_ts(time(NULL));
    req.set_caller(lb_config.local_ip);

    // 2. 从idle_list取值
    for (host_list_it it = idle_list_.begin(); it != idle_list_.end(); it++) {
        HostInfo *hi = *it;    
        lbrss::HostCallResult call_res;
        call_res.set_ip(hi->ip);
        call_res.set_port(hi->port);
        call_res.set_succ(hi->rsucc);
        call_res.set_err(hi->rerr);
        call_res.set_overload(false);
    
        req.add_results()->CopyFrom(call_res);
    }

    // 3. 从over_list取值
    for (host_list_it it = overload_list_.begin(); it != overload_list_.end(); it++) {
        HostInfo *hi = *it;
        lbrss::HostCallResult call_res;
        call_res.set_ip(hi->ip);
        call_res.set_port(hi->port);
        call_res.set_succ(hi->rsucc);
        call_res.set_err(hi->rerr);
        call_res.set_overload(true);
    
        req.add_results()->CopyFrom(call_res);
    }

    // 4. 发送给report_client 的消息队列
    report_queue->send(req);
}

// 获取当前挂载下的全部host信息 添加到vec中
void LoadBalance::get_all_hosts(std::vector<HostInfo*> &vec)
{
    for (host_map_it it = host_map_.begin(); it != host_map_.end(); it++) {
        HostInfo *hi = it->second;
        vec.push_back(hi);
    }
}
