#include "api.h"
#include <iostream>

void usage()
{
    printf("usage: ./get_host [modid] [cmdid]\n");
}

int main(int argc, char **argv)
{
    int ret = 0;

    if (argc != 3) {
        usage();
        return 1;
    }

    int modid = atoi(argv[1]);
    int cmdid = atoi(argv[2]);
    APIClient api;

    std::string ip; 
    int port;


    //1. api 初始化(只调用一次)
    ret = api.reg_init(modid, cmdid);
    if (ret != 0) {
        std::cout << "modid " << modid << ", cmdid " << cmdid << " still not exist host, after register, ret = " << ret << std::endl;
    }

    //2. 获取一个host的ip+port
    ret = api.get_host(modid, cmdid, ip, port);
    if (ret == 0) 
    {
        std::cout << "host is " << ip << ":" << port << std::endl;
    } 
    else
    {
        std::cout << "get host failed" << std::endl;
    }
    while(1){
        sleep(10);
    }
    return 0;
}