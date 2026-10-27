#include "ServiceDiscovery.h"
#include "ConsistentHash.h"
#include "KeeRPCLogger.h"

ServiceDiscovery &ServiceDiscovery::GetInstance(){
    static ServiceDiscovery instance;
    return instance;
}

void ServiceDiscovery::Init(){
    if(m_started){
        return ;
    }
    m_zkClient.Start();
    m_started = true;
}

std::string ServiceDiscovery::GetTargetNode(const std::string &service_name){
    Init();
    std::string path = "/"+service_name;
    std::vector<std::string> nodes = m_zkClient.GetChildren(path.c_str(),nullptr,nullptr);
    if(nodes.empty()){
        LOG(ERROR)<<"no providers for"<<service_name;
        return "";
    }
    return nodes[0];
}

std::string ServiceDiscovery::GetTargetNode(const std::string &service_name,
                                            const std::string &key){
    Init();
    std::string path = "/" + service_name;
    std::vector<std::string> nodes = m_zkClient.GetChildren(path.c_str(),nullptr,nullptr);//获取子结点数据
    if(nodes.empty()){
        LOG(ERROR)<<"no providers for"<<service_name;
        return "";
    }
    ConsistentHash ring;
    ring.UpdateNodes(nodes);
    return ring.GetTargetNode(key);
}