#ifndef ZOOKEEPERUTIL_H
#define ZOOKEEPERUTIL_H

#define THREADED
#include<zookeeper/zookeeper.h>

#include<string>
#include<vector>
//Zookeeper客户端
//负责连接注册中心，创建结点，读取结点和子节点列表
class ZkClient{
public:
    ZkClient();

    ~ZkClient();//关闭与zookeeper的连接

    void Start();//读取地址，完成连接

    void Create(const char *path,const char *data,int datalen,int state = 0);

    std::string GetData(const char *path);//读取结点的数据

    std::vector<std::string>GetChildren(const char *path,watcher_fn fn,void *cbContext);//读取每一个子节点爆保存的数据

private:
    zhandle_t *m_zhandle;//客户端句柄
};

#endif