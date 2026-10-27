//完全全局函数类
#ifndef KEERPC_APPLICATION_H
#define KEERPC_APPLICATION_H

#include "KeeRPCconfig.h"

#include<mutex>

class KeeRPCApplication{
public:
    static void Init(int argc,char **argv);//初始化

    static KeeRPCApplication &GetInstance();//返回应用对象

    static void deleteInstance();

    static KeeRPCconfig &GetConfig();
private:
    static KeeRPCconfig m_config;//配置对象

    static KeeRPCApplication *m_application;//单例指针

    static std::mutex m_mutex;//单例锁

    KeeRPCApplication(){}
    ~KeeRPCApplication(){}

    KeeRPCApplication (const KeeRPCApplication &) = delete;
    KeeRPCApplication &operator=(const KeeRPCApplication &) = delete;
    KeeRPCApplication &operator=(KeeRPCApplication &&) = delete;
};

#endif