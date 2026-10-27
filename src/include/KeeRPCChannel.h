#ifndef KEERPC_CHANNEL_H
#define KEERPC_CHANNEL_H

#include <google/protobuf/service.h>
class KeeRPCChannel:public google::protobuf::RpcChannel{
public:

    explicit KeeRPCChannel(bool connectNow);
    ~KeeRPCChannel() override;

    void CallMethod(const google::protobuf::MethodDescriptor *method,//调用哪个方法
                    google::protobuf::RpcController *controller,//记录调用是否失败
                    const google::protobuf::Message *request,//参数
                    google::protobuf::Message *response,//服务端返回的结果
                    google::protobuf::Closure *done)override;//调用完成之后的回调
};
#endif