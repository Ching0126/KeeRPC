#ifndef KEERPC_CONTROLLER_H
#define KEERPC_CONTROLLER_H

#include<google/protobuf/service.h>
#include<string>

class KeeRPCcontroller:public google::protobuf::RpcController{
public:
    KeeRPCcontroller();//构造

    void Reset()override;//清空错误标志和文本

    bool Failed()const override;

    std::string ErrorText()const override;//返会错误原因

    void SetFailed(const std::string &reason)override;

    void StartCancel()override;//请求取消这次调用

    bool IsCanceled()const override;//是否被取消

    void NotifyOnCancel(google::protobuf::Closure *callback)override;//注册取消完成后的回调
private:
    bool m_failed;//true:fail
    std::string m_errText;//失败原因
};
#endif