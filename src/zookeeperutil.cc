#include "zookeeperutil.h"

#include "KeeRPCApplication.h"
#include "KeeRPCLogger.h"

#include<chrono>
#include<condition_variable>
#include<cstdlib>
#include<string>

namespace{
//三个变量服务于整个进程的一次连接
std::mutex g_connect_mutex;
std::condition_variable g_connect_cv;
bool g_is_connected = false;
//通知会话状态变化
void GlobalWatcher(zhandle_t *zh,int type,int status,const char *path,void *watcherCtx){//回调函数
    (void)zh;
    (void)path;
    (void)watcherCtx;
    if(type ==ZOO_SESSION_EVENT && status ==ZOO_CONNECTED_STATE){
        std::lock_guard<std::mutex>lock(g_connect_mutex);
        g_is_connected = true;
    }
    g_connect_cv.notify_all();
}
}

ZkClient::ZkClient():m_zhandle(nullptr){}

ZkClient::~ZkClient(){
    if(m_zhandle != nullptr){
        zookeeper_close(m_zhandle);
    }
}

void ZkClient::Start(){
    //读取配置
    std::string host = KeeRPCApplication::GetInstance().GetConfig().Load("zookeeperip");
    std::string port = KeeRPCApplication::GetInstance().GetConfig().Load("zookeeperport");

    {
        std::lock_guard<std::mutex> lock(g_connect_mutex);
        g_is_connected = false;
    }
    std::string connstr = host + ":" + port;

    m_zhandle = zookeeper_init(connstr.c_str(),GlobalWatcher,6000,nullptr,nullptr,0);//发起连接，第二个是回调函数，第三个超时时间
    if(m_zhandle == nullptr){
        LOG(ERROR)<<"zookepper_init error";
        std::exit(EXIT_FAILURE);
    }

    std::unique_lock<std::mutex>lock(g_connect_mutex);
    bool connected = g_connect_cv.wait_for(lock,std::chrono::seconds(10),[]{//等待10s，回调会设置标志位。
        return g_is_connected;
    });
    if(!connected){
        LOG(ERROR)<<"zookepper connect timeout";
        std::exit(EXIT_FAILURE);
    }
    LOG(INFO)<<"zookepper_init sucess";
}

std::string ZkClient::GetData(const char *path){//从地址中读取一个结点的数据
    char buf[128] = {0};
    int bufferlen = sizeof(buf);

    int flag = zoo_get(m_zhandle,path,0,buf,&bufferlen,nullptr);//读取path的数据，并且调整buf大小
    if(flag != ZOK ||bufferlen <=0){
        LOG(ERROR)<<"zoo_get error,path:"<<path;
        return "";
    }
    if(bufferlen>static_cast<int>(sizeof(buf))){
        bufferlen = static_cast<int>(sizeof(buf));
    }
    return std::string(buf,bufferlen);
}

std::vector<std::string> ZkClient::GetChildren(const char *path,watcher_fn fn,void *cbContext){//读一个目录下，每一个子节点的数据
    struct String_vector nodes;
    nodes.count = 0;
    nodes.data = nullptr;//data数组
    std::vector<std::string> values;

    int flag = zoo_wget_children(m_zhandle,path,fn,cbContext,&nodes);//查询结点
    if(flag!=ZOK){//zok表示成功
        LOG(ERROR)<<"zoo_wget_children error,path"<<path;
        return values;
    }

    for(int i =0;i<nodes.count;++i){//循环处理结点
        std::string child_path = path;
        if(child_path.empty() || child_path.back()!='/'){
            child_path +="/";
        }
        child_path += nodes.data[i];

        std::string data = GetData(child_path.c_str());//Getdata只保留了数据
        if(!data.empty()){
            values.push_back(data);
        }
    }
    deallocate_String_vector(&nodes);
    return values;
}

void ZkClient::Create(const char *path, const char *data, int datalen, int state) {//创建结点
    char path_buffer[128];
    int bufferlen = sizeof(path_buffer);

    int flag = zoo_exists(m_zhandle, path, 0, nullptr);
    if (flag == ZOK) {
        return;
    }
    if (flag != ZNONODE) {
        LOG(ERROR) << "zoo_exists failed, path: " << path;
        std::exit(EXIT_FAILURE);
    }

    flag = zoo_create(m_zhandle, path, data, datalen,
                      &ZOO_OPEN_ACL_UNSAFE, state, path_buffer, bufferlen);
    if (flag == ZOK) {
        LOG(INFO) << "znode create success, path: " << path;
        return;
    }
    LOG(ERROR) << "znode create failed, path: " << path;
    std::exit(EXIT_FAILURE);
}