#ifndef SERVICE_DISCOVERT_H
#define SERVICE_DISCOVERT_H

#include "zookeeperutil.h"
#include "ConsistentHash.h"
#include <string>

class ServiceDiscovery{
public:

    static ServiceDiscovery &GetInstance();

    void Init();

    std::string GetTargetNode(const std::string &service_name);

    std::string GetTargetNode(const std::string &service_name , const std::string &key);

private:
    ServiceDiscovery() = default;

    ZkClient m_zkClient;

    bool m_started = false;
};

#endif