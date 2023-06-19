#pragma once
#include "io_buf.h"
#include "buf_pool.h"
#include <assert.h>
#include <unistd.h>


/*
 * 给业务层提供的最后tcp_buffer结构
 * */
class ReactorBuf {
public:
    ReactorBuf();
    ~ReactorBuf();

    const int length() const;
    void pop(int len);
    void clear();

protected:
    IOBuf *_buf;
};


//读(输入) 缓存buffer
class InputBuf : public ReactorBuf 
{
public:
    //从一个fd中读取数据到ReactorBuf中
    int read_data(int fd);

    //取出读到的数据
    const char *data() const;

    //重置缓冲区
    void adjust();
};

//写(输出)  缓存buffer
class OutputBuf : public ReactorBuf 
{
public:
    //将一段数据 写到一个ReactorBuf中
    int send_data(const char *data, int datalen);

    //将ReactorBuf中的数据写到一个fd中
    int write2fd(int fd);
};