#include "KeeRPCApplication.h"
#include "KeeRPCLogger.h"
#include "KeeRPCProvider.h"
#include "../user.pb.h"

#include<string>

class UserService : public Kuser::UserServiceRpc{
public:
    bool Login(std::string name,std::string pwd){
        (void)name;//（void)表示当前未用，请你 编译器不要报错
        (void)pwd;
        return true;
    }
    //不同的参数列表，不同的调用
    void Login(google::protobuf::RpcController *controller,//protobuf默认自带参数，记录调用是否失败
                const Kuser::LoginRequest *request,//自定义参数
                Kuser::LoginResponse *response,//自定义返回值
                google::protobuf::Closure *done) override{//默认参数，处理完调用done->Run
        (void)controller;
        std::string name = request->name();
        std::string pwd = request->pwd();
        bool ok = Login(name,pwd);//模拟真实调用

        Kuser::ResultCode *code = response->mutable_result();
        //>mutable_result()是proto根据result字段生成的，返回LoginResponse中得到变量：result的值
        code->set_errcode(0);
        code->set_errmsg("");
        response->set_sucess(ok);
        done->Run();
}
};
//运行命令：/root/KeeRPC/build/example/callee/server -i /root/KeeRPC/bin/test.conf 
//argv[0] argv[1] argv[2]
int main(int argc,char **argv){
    KeeRPCApplication::Init(argc,argv);//初始化配置
    KeeRPCLogger logger("KeeRPCServer");//初始化日志

    KeeRPCProvider provider;
    provider.NotifyService(new UserService());//读取protobuf配置
    provider.Run();//网络监听
    return 0;
}