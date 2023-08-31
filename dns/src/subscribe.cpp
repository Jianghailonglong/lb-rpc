#include "subscribe.h"

extern TCPServer *server;

//单例对象
SubscribeList *SubscribeList::_instance = NULL;

//用于保证创建单例的init方法只执行一次的锁
pthread_once_t SubscribeList::_once = PTHREAD_ONCE_INIT;

void push_change_task(EventLoop *loop, void *args);

SubscribeList::SubscribeList()
{
}

// 订阅
void SubscribeList::subscribe(uint64_t mod, int fd)
{
    // 将mod->fd的关系加入到_book_list中
    pthread_mutex_lock(&_book_list_lock);
    _book_list[mod].insert(fd);
    pthread_mutex_unlock(&_book_list_lock);
}

// 取消订阅
void SubscribeList::unsubscribe(uint64_t mod, int fd)
{
    // 将mod->fd关系从_book_list中删除
    pthread_mutex_lock(&_book_list_lock);
    if (_book_list.find(mod) != _book_list.end()) {
        _book_list[mod].erase(fd);
        if (_book_list[mod].empty()) {
            _book_list.erase(mod);
        }
    }
    pthread_mutex_unlock(&_book_list_lock);
}

// 根据在线用户fd得到需要发布的列表
void SubscribeList::make_publish_map(listen_fd_set &online_fds, publish_map &need_publish)
{
    publish_map::iterator it;

    pthread_mutex_lock(&_push_list_lock);
    // 遍历_push_list 找到 online_fds匹配的数据，放到need_publish中
    for (it = _push_list.begin(); it != _push_list.end(); it++)  {
        // it->first 是 fd
        // it->second 是 modid/cmdid
        // 在线用户和要发布的列表中对应的fd相同
        if (online_fds.find(it->first) != online_fds.end()) {
            // 匹配到
            // 当前的键值对移动到need_publish中
            need_publish[it->first] = _push_list[it->first];
            // 当该组数据从_push_list中删除掉
            _push_list.erase(it);
        }
    }

    pthread_mutex_unlock(&_push_list_lock);
}


// 发布
void SubscribeList::publish(std::unordered_set<uint64_t> &change_mods)
{
    // 1 将change_mods已经修改的mod->fd 放到发布清单_push_list中 
    pthread_mutex_lock(&_book_list_lock);
    pthread_mutex_lock(&_push_list_lock);

    std::unordered_set<uint64_t>::iterator it;

    // 遍历变化的modid/cmdid
    for (it = change_mods.begin(); it != change_mods.end(); it++) {
        uint64_t mod = *it;
        if (_book_list.find(mod) != _book_list.end()) {
            // 将订阅列表mod下面的fd set集合迁移到 发布列表中
            hash_set<int>::iterator fds_it;
            for (fds_it = _book_list[mod].begin(); fds_it != _book_list[mod].end(); fds_it++) {
                int fd = *fds_it;
                _push_list[fd].insert(mod);
            }
        }
    }

    pthread_mutex_unlock(&_push_list_lock);
    pthread_mutex_unlock(&_book_list_lock);

    // 2 通知各个线程去执行推送任务
    server->get_thread_pool()->send_task(push_change_task, this);
}

void push_change_task(EventLoop *loop, void *args)
{
    SubscribeList *subscribe = (SubscribeList*)args;

    // 1 获取全部的在线客户端fd
    listen_fd_set online_fds;
    loop->get_listen_fds(online_fds);
    
    // 2 从subscribe的push_list中 找到与online_fds集合匹配，放在一个新的publish_map里
    publish_map need_publish;
    subscribe->make_publish_map(online_fds, need_publish);

    // 3 依次从need_publish取出数据 发送给对应客户端链接
    publish_map::iterator it; 
    for (it = need_publish.begin(); it != need_publish.end(); it++) {
        int fd = it->first; //fd

        // 遍历 fd对应的 modid/cmdid集合
        hash_set<uint64_t>::iterator st;
        for (st = it->second.begin(); st != it->second.end(); st++) {
            // 一个modid/cmdid
            int modid = int((*st) >> 32);
            int cmdid = int(*st);

            // 组装pb消息，发送给客户
            lbrss::GetRouteResponse rsp; 
            rsp.set_modid(modid);
            rsp.set_cmdid(cmdid);

            // 通过route查询对应的host ip/port信息 进行组装
            host_set hosts = Route::instance()->get_hosts(modid, cmdid) ;
            for (host_set_it hit = hosts.begin(); hit != hosts.end(); hit++) {
                uint64_t ip_port_pair = *hit;
                lbrss::HostInfo host_info;
                host_info.set_ip((uint32_t)(ip_port_pair >> 32));
                host_info.set_port((int)ip_port_pair);

                // 添加到rsp中
                rsp.add_host()->CopyFrom(host_info);
            }

            // 给当前fd 发送一个更新消息
            std::string responseString;
            rsp.SerializeToString(&responseString);

            // 通过fd取出链接信息
            NetConnection *conn = TCPServer::conns[fd];
            if (conn != NULL) {
                conn->send_message(responseString.c_str(), responseString.size(), lbrss::ID_GetRouteResponse);
            }
        }
    }
}