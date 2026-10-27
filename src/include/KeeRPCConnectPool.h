#ifndef KEERPC_CONNECT_POOL_H
#define KEERPC_CONNECT_POOL_H

#include "LockFreeQueue.h"

#include <memory>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <queue>
#include <string>
#include <unordered_map>
//连接池。同一个 ip:port 的空闲 TCP 连接放在一起，避免每次调用都重新握手。
class KeeRPCConnectPool{
public:
    static KeeRPCConnectPool &GetInstance();//整个进程公用一个池

    int BorrowConnection(const std::string &ip , uint16_t port);//// 借一条到 ip:port 的连接。没有空闲连接时新建一条。

    void ReturnConnection(const std::string &ip , uint16_t port, int fd, bool is_bad = false);//把连接放回池子

    void WarmUp(const std::string &ip,uint16_t port,int count);

private:
    KeeRPCConnectPool() = default;
    ~KeeRPCConnectPool();

    struct Bucket{//某一个地址下的连接
        //std::queue<int> free_fds;//空闲套接字队列
        std::unique_ptr<MPMCQueue<int>> free_fds;

        std::atomic<int> active_count;

        explicit Bucket(size_t capacity)
            :free_fds(new MPMCQueue<int>(capacity))
            ,active_count(0)
            {}
        //int active_count = 0;// // 这个地址已经建立、还没关闭的连接数，包含正在使用的。
    };

    Bucket *GetBucket(const std::string &key);

    std::mutex mutex_;

    std::unordered_map<std::string,std::unique_ptr<Bucket>> pools_;

    int max_per_node_ = 32;// // 同一个地址最多保留多少条连接。
};

#endif