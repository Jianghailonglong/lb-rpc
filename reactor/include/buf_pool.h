#pragma once

#include <ext/hash_map>
#include "io_buf.h"

typedef __gnu_cxx::hash_map<int, IOBuf*> pool_t;

enum MEM_CAP {
    m4K     = 4096,
    m16K    = 16384,
    m64K    = 65536,
    m256K   = 262144,
    m1M     = 1048576,
    m4M     = 4194304,
    m8M     = 8388608
};


//总内存池最大限制 单位是Kb 所以目前限制是 5GB
#define EXTRA_MEM_LIMIT (5U *1024 *1024) 

/*
 *  定义buf内存池
 *  设计为单例
 * */
class BufPool 
{
public:
    // 初始化单例对象
    static void init() {
        //创建单例
        _instance = new BufPool();
    }

    //获取单例方法
    static BufPool *instance() {
        //保证init方法在这个进程执行中 只被执行一次
        pthread_once(&_once, init);
        return _instance;
    }

    //开辟一个IOBuf
    IOBuf *alloc_buf(int N);
    IOBuf *alloc_buf() { return alloc_buf(m4K); }


    //重置一个IOBuf
    void revert(IOBuf *buffer);

    
private:
    BufPool();

    // 拷贝构造私有化
    BufPool(const BufPool&);
    const BufPool& operator=(const BufPool&);

    // 所有buffer的一个map集合句柄
    pool_t _pool;

    // 总buffer池的内存大小 单位为KB
    uint64_t _total_mem;

    // 单例对象
    static BufPool *_instance;

    // 用于保证创建单例的init方法只执行一次的锁
    static pthread_once_t _once;

    // 用户保护内存池链表修改的互斥锁
    static pthread_mutex_t _mutex;
};