#include "route_lb.h"
#include "lbrss.pb.h"
#include "main_server.h"

// 构造初始化
RouteLB::RouteLB(int id):id_(id)
{
    pthread_mutex_init(&mutex_, NULL);
}

// 将全部的load_balance都重置为NEW状态
void RouteLB::reset_lb_status()
{
    pthread_mutex_lock(&mutex_);
    for (route_map_it it = route_lb_map_.begin();
        it != route_lb_map_.end(); it++) {

        LoadBalance *lb = it->second;
        if (lb->status_ == LoadBalance::PULLING) {
            lb->status_ = LoadBalance::NEW;
        }
    }
    pthread_mutex_unlock(&mutex_);
}

// agent获取一个host主机，将返回的主机结果存放在rsp中
int RouteLB::get_host(int modid, int cmdid, lbrss::GetHostResponse &rsp)
{
    int ret = lbrss::RET_SUCC;

    // 1. 得到key
    uint64_t key = ((uint64_t)modid << 32) + cmdid;

    pthread_mutex_lock(&mutex_);
    // 2. 当前key已经存在_route_lb_map中
    if (route_lb_map_.find(key) != route_lb_map_.end()) {
        // 2.1 取出对应的load_balance
        LoadBalance *lb = route_lb_map_[key];

        if (lb->empty() == true) {
            // 存在lb里面的host为空，说明正在pull()中，还没有从dns_service返回来,所以直接回复不存在
            assert(lb->status_ == LoadBalance::PULLING);
            rsp.set_retcode(lbrss::RET_NOEXIST);
        }
        else {
            ret = lb->pick_one_host(rsp);
            rsp.set_retcode(ret);

            // 超时重拉路由
            // 检查是否要重新拉路由信息
            // 若路由并没有处于PULLING状态，且有效期已经超时，则重新拉取
            if (lb->status_ == LoadBalance::NEW && time(NULL) - lb->last_update_time_ > lb_config.update_timeout) {
                lb->pull();
            }
        }
    }
    // 3. 当前key不存在_route_lb_map中
    else {
        // 3.1 新建一个load_balance
        LoadBalance *lb = new LoadBalance(modid, cmdid);
        if (lb == NULL) {
            fprintf(stderr, "no more space to create loadbalance\n");
            exit(1);
        }

        // 3.2 新建的load_balance加入到map中
        route_lb_map_[key] = lb;

        // 3.3 从dns service服务拉取具体的host信息
        lb->pull();

        // 3.4 设置rsp的回执retcode
        rsp.set_retcode(lbrss::RET_NOEXIST);

        ret = lbrss::RET_NOEXIST;
    }
    pthread_mutex_unlock(&mutex_);

    return ret;
}

// agent获取某个modid/cmdid的全部主机，将返回的主机结果存放在rsp中
int RouteLB::get_route(int modid, int cmdid, lbrss::GetRouteResponse &rsp)
{
    int ret = lbrss::RET_SUCC;

    // 1. 得到key
    uint64_t key = ((uint64_t)modid << 32) + cmdid;

    pthread_mutex_lock(&mutex_);

    // 2. 当前key已经存在route_lb_map中
    if (route_lb_map_.find(key) != route_lb_map_.end()) {
        // 2.1 取出对应的load_balance
        LoadBalance *lb = route_lb_map_[key];

        std::vector<HostInfo*> vec;
        lb->get_all_hosts(vec);

        for (std::vector<HostInfo*>::iterator it = vec.begin(); it != vec.end(); it++) {
            lbrss::HostInfo host;
            host.set_ip((*it)->ip);
            host.set_port((*it)->port);
            rsp.add_host()->CopyFrom(host);
        }

        // 超时重拉路由
        // 检查是否要重新拉路由信息
        // 若路由并没有处于PULLING状态，且有效期已经超时，则重新拉取
        if (lb->status_ == LoadBalance::NEW && time(NULL) - lb->last_update_time_ > lb_config.update_timeout) {
            lb->pull();
        }
    }
    // 3. 当前key不存在_route_lb_map中
    else {
        // 3.1 新建一个LoadBalance
        LoadBalance *lb = new LoadBalance(modid, cmdid);
        if (lb == NULL) {
            fprintf(stderr, "no more space to create loadbalance\n");
            exit(1);
        }

        // 3.2 新建的load_balance加入到map中
        route_lb_map_[key] = lb;

        // 3.3 从dns service服务拉取具体的host信息
        lb->pull();


        ret = lbrss::RET_NOEXIST;
    }
    pthread_mutex_unlock(&mutex_);

    return ret;
}

// 根据Dns Service返回的结果更新自己的route_lb_map
int RouteLB::update_host(int modid, int cmdid, lbrss::GetRouteResponse &rsp)
{
    // 1. 得到key
    uint64_t key = ((uint64_t)modid << 32) + cmdid;

    pthread_mutex_lock(&mutex_);

    // 2. 在route_lb_map_中找到对应的key 
    if (route_lb_map_.find(key) != route_lb_map_.end()) {
        LoadBalance *lb = route_lb_map_[key];

        if (rsp.host_size() == 0) {
            // 2.1 如果返回的结果 lb下已经没有任何host信息，则删除该key
            delete lb;
            lb = nullptr;
            route_lb_map_.erase(key);
        }
        else {
            // 2.2 更新新host信息
            lb->update(rsp);
        }
    }
    
    pthread_mutex_unlock(&mutex_);

    return 0;
}

// agent 上报某主机的获取结果
void RouteLB::report_host(lbrss::ReportRequest req)
{
    int modid = req.modid();
    int cmdid = req.cmdid();
    int retcode = req.retcode();
    int ip = req.host().ip();
    int port = req.host().port();
    
    uint64_t key = ((uint64_t)modid << 32) + cmdid;

    pthread_mutex_lock(&mutex_);

    if (route_lb_map_.find(key) != route_lb_map_.end()) {
        LoadBalance *lb = route_lb_map_[key];

        lb->report(ip, port, retcode);

        // 上报信息给远程reporter服务器
        lb->commit();
    }

    pthread_mutex_unlock(&mutex_);
}

