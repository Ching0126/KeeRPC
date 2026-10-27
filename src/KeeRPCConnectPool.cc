#include "KeeRPCConnectPool.h"
#include "KeeRPCLogger.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

KeeRPCConnectPool &KeeRPCConnectPool::GetInstance(){//返回连接池全局实例
    static KeeRPCConnectPool instance;
    return instance;
}

KeeRPCConnectPool::Bucket *KeeRPCConnectPool::GetBucket(const std::string &key){
    std::lock_guard<std::mutex>lock(mutex_);
    auto it = pools_.find(key);
    if(it==pools_.end()){
        std::unique_ptr<Bucket> bucket(new Bucket(max_per_node_));
        Bucket *raw = bucket.get();
        pools_.emplace(key,std::move(bucket));
        return raw;
    }
    return it->second.get();
}

//std::unordered_map<std::string,Bucket> pools_;
KeeRPCConnectPool::~KeeRPCConnectPool(){
    std::lock_guard<std::mutex> lock(mutex_);
    for(auto &item : pools_){//释放fd
        int fd =-1;
        while(item.second->free_fds->pop(fd)){
            ::close(fd);
        }
    }
}

int KeeRPCConnectPool::BorrowConnection(const std::string &ip,uint16_t port){
    std::string key = ip + ":" + std::to_string(port);
    Bucket *bucket = GetBucket(key);

    int fd = -1;
    if(bucket->free_fds->pop(fd)){
        LOG(INFO)<<"reuse connection"<<fd<<"for"<<key;
        return fd;
    }
    
    if(bucket->active_count.fetch_add(1)>=max_per_node_){
        bucket->active_count.fetch_sub(1);
        return -1;
    }
    // {
    //     std::lock_guard<std::mutex> lock(mutex_);
    //     Bucket &bucket = pools_[key];
    //     if(!bucket.free_fds.empty()){//存在空闲fd
    //         int fd = bucket.free_fds.front();
    //         bucket.free_fds.pop();
    //         LOG(INFO)<<"reuse connection"<<fd<<"for"<<key;
    //         return fd;
    //     }
    //     if(bucket.active_count >= max_per_node_){
    //         return -1;
    //     }
    //     ++bucket.active_count;
    // }

    fd = ::socket(AF_INET,SOCK_STREAM,0);//创建套接字
    if(fd<0){
        // std::lock_guard<std::mutex> lock(mutex_);
        // --pools_[key].active_count;
        bucket->active_count.fetch_sub(1);
        return -1;
    }

    int opt = 1;
    ::setsockopt(fd,IPPROTO_TCP,TCP_NODELAY,&opt,sizeof(opt));//不攒数据，有就发

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if(::inet_pton(AF_INET,ip.c_str(),&addr.sin_addr) != 1 ||
        ::connect(fd,reinterpret_cast<sockaddr *>(&addr),sizeof(addr)) != 0){
        ::close(fd);
        // std::lock_guard<std::mutex> lock(mutex_);
        // --pools_[key].active_count;
        bucket->active_count.fetch_sub(1);
        return -1;
    }

    LOG(INFO)<<"new connection"<<fd<<"for"<<key;
    return fd;
}

void KeeRPCConnectPool::ReturnConnection(const std::string &ip,uint16_t port,
                                            int fd,bool is_bad){
    std::string key = ip + ":" + std::to_string(port);
    Bucket *bucket = nullptr;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = pools_.find(key);
        if(it!=pools_.end()){
            bucket=it->second.get();
        }
    }
    
    //if(it==pools_.end() || is_bad){//该ipbucket已删除或者接收失败
    if(bucket ==nullptr || is_bad){
        if(fd >= 0){
            ::close(fd);
        }
        if(bucket != nullptr){
        //if(it!= pools_.end() && it->second.active_count >0){//坏链接不归队，滚出去。
            //--it->second.active_count;
            bucket->active_count.fetch_sub(1);
        }
        return ;
    }
    //it->second.free_fds.push(fd);
    if(!bucket->free_fds->push(fd)){
        ::close(fd);
        bucket->active_count.fetch_sub(1);
    }
}

void KeeRPCConnectPool::WarmUp(const std::string &ip,uint16_t port,int count){
    std::string key = ip + ":" +std::to_string(port);
    Bucket *bucket = GetBucket(key);
    int ready = 0;
    for(int i=0;i<count;++i){
        if(bucket->active_count.load()>=max_per_node_){
            break;
        }
        int fd = ::socket(AF_INET,SOCK_STREAM,0);
        if(fd<0){
            continue;
        }
        int opt =1;
        ::setsockopt(fd,IPPROTO_TCP,TCP_NODELAY,&opt,sizeof(opt));

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        if(::inet_pton(AF_INET,ip.c_str(),&addr.sin_addr)!=1 ||
            ::connect(fd,reinterpret_cast<sockaddr *>(&addr),sizeof(addr))!=0){
                ::close(fd);
                continue;
            }
            bucket->active_count.fetch_add(1);
            if(!bucket->free_fds->push(fd)){
                ::close(fd);
                bucket->active_count.fetch_sub(1);
                continue;
            }
            ++ready;
    }
    LOG(INFO)<<"warmup"<<ready<<"connection for"<<key;
}