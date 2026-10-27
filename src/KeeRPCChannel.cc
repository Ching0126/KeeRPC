#include "KeeRPCChannel.h"
#include "KeeRPCApplication.h"
#include "KeeRPCLogger.h"
#include "KeeRPCheader.pb.h"
#include "KeeRPCcontroller.h"
#include "ServiceDiscovery.h"
#include "KeeRPCConnectPool.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <string>
#include <sys/socket.h>
#include <vector>
#include <atomic>
//客户端真正远程调用的地方
namespace{

bool send_all(int fd,const char *data,size_t size){//发送所有数据
    size_t sent = 0;
    while(sent < size){
        ssize_t n = ::send(fd,data+sent,size-sent,MSG_NOSIGNAL);
        //对方已经关闭时，不要用 SIGPIPE 把进程杀掉，而是让 send 返回 -1
        //返回值是实际真正发出去的字节数
        if(n<0){
            if(errno == EINTR){
                continue;
            }
            return false;
        }
        if(n==0){
            return false;
        }
        sent += static_cast<size_t>(n);
    }
    return true;
}

bool recv_exact(int fd,char *buf,size_t size){
    size_t got = 0;
    while(got<size){
        ssize_t n = ::recv(fd,buf+got,size-got,0);
        if(n<0){
            if(errno ==  EINTR){
                continue;
            }
            return false;
        }
        if(n==0){
            return false;
        }
        got += static_cast<size_t>(n);
    }
    return true;
}

}

KeeRPCChannel::KeeRPCChannel(bool connectNow){
    (void)connectNow;
}

KeeRPCChannel::~KeeRPCChannel(){}

//客户端发一次远程调用的全程
void KeeRPCChannel::CallMethod(const google::protobuf::MethodDescriptor *method,//方法描述
                                google::protobuf::RpcController *controller,//控制器，写失败原因
                                const google::protobuf::Message *request,//带参的请求
                                google::protobuf::Message *response,//装服务端返回
                                google::protobuf::Closure *done){
    (void)done;
    if(controller == nullptr){
        return ;
    }

    std::string args_str;
    if (!request->SerializeToString(&args_str)) {//参数序列化
        controller->SetFailed("serialize request fail");
        return;
    }

    KEERPC::RpcHeader header;//请求头参数
    header.set_service_name(method->service()->name());
    header.set_method_name(method->name());
    header.set_args_size(args_str.size());

    std::string header_str;
    if(!header.SerializeToString(&header_str)){//请求头序列化处理
        controller->SetFailed("serialize header fail");
        return ;
    }

    uint32_t header_len = static_cast<uint32_t>(header_str.size());
    uint32_t total_len = 4 + header_len + static_cast<uint32_t>(args_str.size());
    uint32_t net_total_len = htonl(total_len);//小端转大端
    uint32_t net_header_len = htonl(header_len);

    std::string frame;//最终的一整块字节
    frame.append(reinterpret_cast<char *>(&net_total_len),4);
    frame.append(reinterpret_cast<char *>(&net_header_len),4);
    frame.append(header_str);
    frame.append(args_str);

    // std::string ip = KeeRPCApplication::GetInstance().GetConfig().Load("rpcserverip");
    // std::string port_text = KeeRPCApplication::GetInstance().GetConfig().Load("rpcserverport");

    //std::string ip_port = ServiceDiscovery::GetInstance().GetTargetNode(method->service()->name());
    //method->service()->name() "UserServiceRpc"

    static std::atomic<uint64_t> request_id{0};//静态计时器
    std::string route_key = std::to_string(request_id.fetch_add(1));    
    std::string ip_port = ServiceDiscovery::GetInstance().GetTargetNode(method->service()->name(), route_key);

    auto colon = ip_port.rfind(':');
    if(colon == std::string::npos){
        controller->SetFailed("no provider for" + method->service()->name());
        return ;
    }
    std::string ip = ip_port.substr(0,colon);
    int port = std::atoi(ip_port.substr(colon + 1).c_str());

    // if(ip.empty()||port_text.empty()){
    //     controller->SetFailed("rpc server address is missing");
    //     return ;
    // }
    // int port = std::atoi(port_text.c_str());//string 转int

    int fd = KeeRPCConnectPool::GetInstance().BorrowConnection(
        ip,static_cast<uint16_t>(port));
    if(fd<0){
        controller->SetFailed("borrow connection fail");
        return ;
    }

    // int fd = ::socket(AF_INET,SOCK_STREAM,0);
    // if(fd<0){
    //     controller->SetFailed("creat socket fail");
    //     return ;
    // }

    // sockaddr_in addr{};
    // addr.sin_family = AF_INET;
    // addr.sin_port = htons(static_cast<uint16_t>(port));
    // if(::inet_pton(AF_INET,ip.c_str(),&addr.sin_addr)!=1){
    //     ::close(fd);
    //     controller->SetFailed("invalid server ip");
    //     return ;
    // }
    // if(::connect(fd,reinterpret_cast<sockaddr *>(&addr),sizeof(addr))!=0){
    //     ::close(fd);
    //     controller->SetFailed("connect server fail");
    //     return ;
    // }
    bool bad = false;
    if(!send_all(fd,frame.data(),frame.size())){
        bad = true;
        controller->SetFailed("send rpc request fail");
    } else{
        uint32_t net_response_len = 0;
        if(!recv_exact(fd,reinterpret_cast<char *>(&net_response_len),4)){
            bad = true;
            controller->SetFailed("recv response length fail");
        }else{
            uint32_t response_len = ntohl(net_response_len);
            if(response_len > 8*1024*1024){
                 bad = true;
                 controller->SetFailed("response too large");
            }else{
                std::vector<char> body(response_len);
                if(!recv_exact(fd,body.data(),response_len)){
                bad = true;
                controller->SetFailed("parse response fail");
                }else if(!response->ParseFromArray(body.data(),static_cast<int>(response_len))){
                    controller->SetFailed("parse response fail");
                }
            }   
        }
    }
    // if(!send_all(fd,frame.data(),frame.size())){//发送数据
    //     ::close(fd);
    //     controller->SetFailed("send rpc request fail");
    //     return ;
    // }

    // uint32_t net_response_len = 0;
    // if(!recv_exact(fd,reinterpret_cast<char *>(&net_response_len),4)){//取出长度字节
    //     ::close(fd);
    //     controller->SetFailed("recv reponse length fail");
    //     return ;
    // }

    // uint32_t response_len = ntohl(net_response_len);
    // if(response_len > 8*1024*1024){
    //     ::close(fd);
    //     controller->SetFailed("response too large");
    //     return ;
    // }

    // std::vector<char> body(response_len);
    // if(!recv_exact(fd,body.data(),response_len)){//读取数据
    //     ::close(fd);
    //     controller->SetFailed("recv response body fail");
    //     return ;
    // }
    // ::close(fd);

    // if(!response->ParseFromArray(body.data(),static_cast<int>(response_len))){//反序列化，将数据填进response
    //     controller->SetFailed("parse response fail");
    // }
    KeeRPCConnectPool::GetInstance().ReturnConnection(
        ip,static_cast<uint16_t>(port),fd,bad);
}