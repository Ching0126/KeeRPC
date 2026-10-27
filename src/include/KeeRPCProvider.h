#ifndef KEE_RPC_PROVIDER_H
#define KEE_RPC_PROVIDER_H

#include <google/protobuf/descriptor.h>
#include <google/protobuf/service.h>
#include <muduo/base/Timestamp.h>
#include <muduo/net/Buffer.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/TcpConnection.h>
#include <muduo/net/TcpServer.h>

#include<string>
#include<unordered_map>

class KeeRPCProvider{
public:
    void NotifyService(google::protobuf::Service *service);//发布服务对象，service所有权交给provider

    void Run();//启动网络，等待请求

    ~KeeRPCProvider();

private:
    //为run服务
    void OnConnection(const muduo::net::TcpConnectionPtr &coon);
    void OnMessage(const muduo::net::TcpConnectionPtr &coon,//那一条连接的数据
                    muduo::net::Buffer *buffer,//缓冲区未被取走的子节
                    muduo::Timestamp receive_time);//读取这批数据的时间
    
    void SendRpcResponse(const muduo::net::TcpConnectionPtr &conn,
                    google::protobuf::Message *response,
                    google::protobuf::Message *request);

    //一个已发布的服务结构体
    struct ServiceInfo{
        google::protobuf::Service *service;//服务对象
        
        std::unordered_map<std::string,const google::protobuf::MethodDescriptor *> method_map;//方法名到方法描述符的映射
    };

    //已发布的服务名，建是服务名
    std::unordered_map<std::string,ServiceInfo> service_map;//服务表

    muduo::net::EventLoop event_loop;
};
#endif