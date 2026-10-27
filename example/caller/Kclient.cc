#include "KeeRPCApplication.h"
#include "KeeRPCChannel.h"
#include "KeeRPCConnectPool.h"
#include "KeeRPCcontroller.h"
#include "KeeRPCLogger.h"
#include "../user.pb.h"

#include <atomic>
#include <iostream>
#include <cstdlib>
#include <thread>
#include <vector>

void SendRequest(int thread_id,std::atomic<int> &sucess_count,
                std::atomic<int> &fail_count,int request_per_thread){
    (void)thread_id;
    Kuser::UserServiceRpc_Stub stub(
        new KeeRPCChannel(false),
        google::protobuf::Service::STUB_OWNS_CHANNEL);
    
    Kuser::LoginRequest request;
    request.set_name("zhangsan");
    request.set_pwd("123456");
    Kuser::LoginResponse response;
    KeeRPCcontroller controller;

    for(int i=0;i<request_per_thread;++i){
        controller.Reset();
        stub.Login(&controller,&request,&response,nullptr);
        if(controller.Failed()||response.result().errcode()!=0){
            fail_count.fetch_add(1);
        }else{
            sucess_count.fetch_add(1);
        }
    }
}

int main(int argc,char **argv){
    //读取配置，初始化日志
    KeeRPCApplication::Init(argc,argv);
    KeeRPCLogger logger("KeeRPCClient");

    std::string ip = KeeRPCApplication::GetConfig().Load("rpcserverip");
    int port = std::atoi(KeeRPCApplication::GetConfig().Load("rpcserverport").c_str());
    
    const int thread_count = 8;
    const int request_per_thread = 200;

    KeeRPCConnectPool::GetInstance().WarmUp(ip,static_cast<uint16_t>(port),thread_count);

    std::atomic<int> sucess_count(0);
    std::atomic<int> fail_count(0);
    std::vector<std::thread> threads;
    auto start_time = std::chrono::steady_clock::now();

    for(int i=0;i<thread_count;++i){
        threads.emplace_back(SendRequest,i,std::ref(sucess_count),std::ref(fail_count),request_per_thread);
    }

    for(std::thread &thread : threads){
        thread.join();
    }

    auto end_time = std::chrono::steady_clock::now();
    double seconds = std::chrono::duration<double>(end_time-start_time).count();
    int total = thread_count * request_per_thread;
    std::cout<<"total"<<total<<"sucess:"<<sucess_count<<"fail"<<fail_count<<"seconds"<<seconds<<"qps="<<total/seconds<<std::endl;

    //UserServiceRpc_Stub(RpcChannel* channel, Service::ChannelOwnership ownership);Stub同一套构造函数
    // Kuser::UserServiceRpc_Stub stub(//创建客户端代理,
    //     new KeeRPCChannel(false),
    //     google::protobuf::Service::STUB_OWNS_CHANNEL);//通道归stub删除，stub销毁时，会删除new出来的通道
    // //发起一次Login
    // Kuser::LoginRequest request;
    // request.set_name("zhangsan");
    // request.set_pwd("123456");

    // Kuser::LoginResponse response;
    // KeeRPCcontroller controller;

    // // stub.Login(&controller,&request,&response,nullptr);

    // // if(controller.Failed()){
    // //     std::cout<<controller.ErrorText()<<std::endl;
    // //     return 1;
    // // }
    // // //打印结果
    // // std::cout<<"login sucess="<<response.sucess()<<std::endl;
    // stub.Login(&controller, &request, &response, nullptr);
    // if (controller.Failed()) {
    //     std::cout << controller.ErrorText() << std::endl;
    //     return 1;
    // }
    // std::cout << "login sucess=" << response.sucess() << std::endl;

    // controller.Reset();
    // stub.Login(&controller, &request, &response, nullptr);
    // if (controller.Failed()) {
    //     std::cout << controller.ErrorText() << std::endl;
    //     return 1;
    // }
    // std::cout << "login sucess=" << response.sucess() << std::endl;
    return fail_count == 0 ?0 :1;
}