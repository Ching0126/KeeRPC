#include "KeeRPCApplication.h"

#include<cstdlib>
#include<iostream>
#include<unistd.h>

KeeRPCconfig KeeRPCApplication::m_config;
std::mutex KeeRPCApplication::m_mutex;
KeeRPCApplication *KeeRPCApplication::m_application = nullptr;

void KeeRPCApplication::Init(int argc,char **argv){
    if(argc<2){
        std::cout<<"格式：command -i <配置文件路径>"<<std::endl;
        std::exit(EXIT_FAILURE);
    }

    int option = 0;
    std::string config_file;
    while((option = getopt(argc,argv,":i:")) != -1){
        //函数作用：找到下一个以-开头的选项
        //前一个： -i之后没有路径的返回值
        //i：需要识别的-后面的字符 就是-i
        //后面的； -i后面必须有参数，也就是配置文件路径
        switch(option){
            case 'i':
                config_file = optarg;//optarg是全局变量，自动指向-i之后的地址
                break;
            case '?'://不是-i 可能是-x
                std::cout<<"格式：command -i <配置文件路径>"<<std::endl;
                std::exit(EXIT_FAILURE);
                break;
            case ':':
                std::cout<<"格式：command -i <配置文件路径>"<<std::endl;
                std::exit(EXIT_FAILURE);
                break;
            default:
                break;
        }
    }

    if(config_file.empty()){
        std::cout<<"格式：command -i <配置文件路径>"<<std::endl;
        std::exit(EXIT_FAILURE);
    }

    m_config.LoadConfigFile(config_file.c_str());//路径字符串
}

KeeRPCApplication &KeeRPCApplication::GetInstance(){//获取实例
    std::lock_guard<std::mutex>lock(m_mutex);
    if(m_application == nullptr){
        m_application = new KeeRPCApplication();
        std::atexit(deleteInstance);//登记函数，程序正常结束时调用deleteinstance
    }
    return *m_application;
}

void KeeRPCApplication::deleteInstance(){
    if(m_application != nullptr){
       delete m_application;
       m_application = nullptr;
    }
}

KeeRPCconfig &KeeRPCApplication::GetConfig(){
    return m_config;
}