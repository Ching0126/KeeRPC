#ifndef KEERPC_LOG_H
#define KEERPC_LOG_H

#include<glog/logging.h>
#include<string>

class KeeRPCLogger{
public:
    //构造函数和析构函数
    explicit KeeRPCLogger(const char *argv0){//程序名，做标识
        google::InitGoogleLogging(argv0);
        //打印到标准错误，并且带颜色
        FLAGS_colorlogtostderr = true;
        FLAGS_logtostderr = true;
    }

    ~KeeRPCLogger(){
        google::ShutdownGoogleLogging();//关闭 glog，并把还没写出的日志刷出去。
    }

    //日志打印函数
    static void Info(const std::string &message){
        LOG(INFO)<<message;
    }

    static void Warning(const std::string &message){
        LOG(WARNING)<<message;
    }

    static void Error(const std::string &message){
        LOG(ERROR)<<message;
    }

    static void Fatal(const std::string &message){
        LOG(FATAL)<<message;
    }
private:
    KeeRPCLogger(const KeeRPCLogger &) = delete;//禁止拷贝构造

    KeeRPCLogger &operator=(const KeeRPCLogger &) = delete;//禁止拷贝赋值
};

#endif