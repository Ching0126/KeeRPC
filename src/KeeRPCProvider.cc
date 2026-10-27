#include "KeeRPCApplication.h"
#include "KeeRPCProvider.h"
#include "KeeRPCLogger.h"
#include "KeeRPCheader.pb.h"
#include "zookeeperutil.h"
#include<arpa/inet.h>
#include<cstring>
#include<string>
#include <cstdlib>
#include <functional>
#include <memory>

class KeeRPCClosure : public google::protobuf::Closure{
public:
    explicit KeeRPCClosure(std::function<void()> callback) : callback_(std::move(callback)){}

    void Run() override{
        callback_();
        delete this;
    }

private:
    std::function<void()> callback_;
};

void KeeRPCProvider::NotifyService(google::protobuf::Service *service){
    if(service == nullptr){
        return ;
    }

    ServiceInfo service_info;//自定义的结构体
    const google::protobuf::ServiceDescriptor *service_desc = service->GetDescriptor();//返回服务描述
    std::string service_name = service_desc->name();//服务名

    int method_count = service_desc->method_count();//rpc方法个数 ，目前是2
    for(int i=0 ; i<method_count; ++i){//按照循环取各个方法的描述
        const google::protobuf::MethodDescriptor *method_desc = service_desc->method(i);//方法描述
        std::string method_name = method_desc->name();
        service_info.method_map.emplace(method_name,method_desc);
        LOG(INFO)<<"method_name = "<<method_name;
    }

    service_info.service = service;
    auto result = service_map.emplace(service_name,service_info);
    //result.first 是指向该元素的迭代器，result.second 是 bool。
    if(!result.second){//检查是否插入成功。
        LOG(ERROR)<<"service already resgistered: "<<service_name;
        delete service;
        return ;
    }
    LOG(INFO)<<"service_name = "<<service_name;
}

void KeeRPCProvider::Run(){
    //获取ip host
    std::string ip = KeeRPCApplication::GetInstance().GetConfig().Load("rpcserverip");
    std::string port_text = KeeRPCApplication::GetInstance().GetConfig().Load("rpcserverport");
    if(ip.empty()||port_text.empty()){
        LOG(ERROR)<<"rpc server address is missing";
        return ;
    }
    int port = std::atoi(port_text.c_str());//char转int 

    muduo::net::InetAddress address(ip,static_cast<uint16_t>(port));

    //初始化服务器，设置回调函数
    auto server = std::make_shared<muduo::net::TcpServer>(&event_loop,address,"KeeRPCProvider");
    //设置回调函数
    /*
        server->setConnectionCallback(
    [this](const muduo::net::TcpConnectionPtr& conn) {
        OnConnection(conn);
    });
    */
    //。std::bind 来自 <functional>，作用是把一个已有函数和它的一部分参数捆成一个新的可调用对象。
    //std::placeholders::_1 占位标记，调用时再把第一个参数填到这里
    server->setConnectionCallback(
        std::bind(&KeeRPCProvider::OnConnection,this,std::placeholders::_1));
    server->setMessageCallback(
        std::bind(&KeeRPCProvider::OnMessage,this,
                    std::placeholders::_1,std::placeholders::_2,std::placeholders::_3));

    LOG(INFO)<<"KeeRPCProvider listen at "<<ip<<":"<<port;
    server->start();//登记监听，将端口套接字交给eventloop

    ZkClient zkclient;
    zkclient.Start();
    std::string ip_port = ip + ":" + std::to_string(port);
    for(auto &item : service_map){
        std::string service_path = "/" + item.first;
        zkclient.Create(service_path.c_str(),nullptr,0);

        std::string instance_path = service_path + "/" + ip_port;
        zkclient.Create(instance_path.c_str(),
                        ip_port.c_str(),//写入数据，第一个参数是路径，第二个参数是数据，第三个是大小
                        static_cast<int>(ip_port.size()),ZOO_EPHEMERAL);
        LOG(INFO)<<"registered"<<instance_path;
    }

    event_loop.loop();//循环等待
}

void KeeRPCProvider::OnConnection(const muduo::net::TcpConnectionPtr &conn){//连接回调，处理连接和断开
    if(conn->connected()){
        LOG(INFO)<<"connection up:"<<conn->peerAddress().toIpPort();
        return ;
    }
    LOG(INFO)<<"connection down: "<<conn->peerAddress().toIpPort();
    conn->shutdown();
}
//在一条连接上，有新字节时，被muduo调用，把缓冲区已经到齐的请求一条一条拆开，找到对应的服务方法，再用Login发回去。
//一个tcp 请求：total_len(4字节)| header_len(4字节)|RpcHeader|LoginRequest
void KeeRPCProvider::OnMessage(const muduo::net::TcpConnectionPtr &conn,
                                muduo::net::Buffer *buffer,
                                muduo::Timestamp receive_time){
    (void)receive_time;

    while(buffer->readableBytes()>=4){//4字节是 total_len，是后面的总字节数
        uint32_t total_len = 0;
        std::memcpy(&total_len,buffer->peek(),4);//peek()指向未娶走的字节
        total_len = ntohl(total_len);//字节序小端转大端

        if(buffer->readableBytes()<4 + total_len){//如果缓冲区字节没收齐，下次再来。
            break;
        }
        buffer->retrieve(4);//将total_len 四个字节丢掉

        uint32_t header_len = 0;
        std::memcpy(&header_len,buffer->peek(),4);
        header_len = ntohl(header_len);
        if(header_len > total_len - 4 || total_len < 4){
            LOG(ERROR)<<"bad header length";
            conn->shutdown();
            return ;
        }
        buffer->retrieve(4);

        std::string header_str(buffer->peek(),header_len);//读取header
        buffer->retrieve(header_len);

        uint32_t args_size = total_len - 4 - header_len;//剩余的参数字节数
        std::string args_str(buffer->peek(),args_size);
        buffer->retrieve(args_size);

        KEERPC::RpcHeader header;
        if(!header.ParseFromString(header_str)){//ParseFromString 把二进制还原成 RpcHeader。
            LOG(ERROR)<<"header parse error";
            continue;
        }

        auto service_it = service_map.find(header.service_name());//提取服务名
        if(service_it == service_map.end()){
            LOG(ERROR)<<"service not found :"<<header.service_name();
            continue;
        }
        auto method_it = service_it->second.method_map.find(header.method_name());//提取方法名
        if(method_it == service_it->second.method_map.end()){
            LOG(ERROR)<<"method not found"<<header.method_name();
            continue;
        }

        google::protobuf::Service *service =service_it->second.service;//服务
        const google::protobuf::MethodDescriptor *method = method_it->second;//方法描述

        google::protobuf::Message *request = service->GetRequestPrototype(method).New();// 返回 Login 应该收到的那种消息的样品
        if(!request->ParseFromString(args_str)){//填参数
            LOG(ERROR)<<"request parse error";
            delete request;
            continue;
        }
        google::protobuf::Message *response = service->GetResponsePrototype(method).New();

        google::protobuf::Closure *done = new KeeRPCClosure(//做完的回调
            [this,conn,response,request]{
                this->SendRpcResponse(conn,response,request);
            });
        service->CallMethod(method,nullptr,request,response,done);
    }

    LOG(INFO)<<"receive bytes: "<<buffer->readableBytes();
}
//把 Login 填好的结果变回字节，前面加上 4 字节长度，通过原来的那条连接发回客户端，然后释放这次调用用的两个堆对象。
void KeeRPCProvider::SendRpcResponse(   const muduo::net::TcpConnectionPtr &conn,//为onmessage服务
                                        google::protobuf::Message *response,
                                        google::protobuf::Message *request){
    std::string response_str;
    if(response->SerializeToString(&response_str)){//反序列化
        uint32_t net_len = htonl(static_cast<uint32_t>(response_str.size()));//小端转大端
        std::string send_buf;
        send_buf.resize(4+response_str.size());//加4个字节

        std::memcpy(&send_buf[0],&net_len,4);
        std::memcpy(&send_buf[4],response_str.data(),response_str.size());
        conn->send(send_buf);
    }else{//报错
        LOG(ERROR)<<"serialize response error";
    }
    //释放
    delete response;
    delete request;
}

KeeRPCProvider::~KeeRPCProvider(){
    for(auto &item : service_map){
        delete item.second.service;
        item.second.service = nullptr;
    }
}