#ifndef CONSISTENT_HASH_H
#define CONSISTENT_HASH_H

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

class ConsistentHash{
public:

    explicit ConsistentHash(int replicas = 10);//构造函数

    void UpdateNodes(const std::vector<std::string> &nodes);//用一批机器地址重新做环

    std::string GetTargetNode(const std::string &key) const;//找相应哈希值对应地址

private:

    static uint32_t Hash(const std::string &data);//字符串到哈希值的映射

    int relicas_;//每个真实机器在圆环上放了几个虚拟点，默认10 

    std::vector<uint32_t> keys_;//圆环上所有虚拟点的位置，相当于0 到 2^32-1围成一圈

    std::unordered_map<uint32_t,std::string> ring_;//虚拟位置和机器地址映射表
};

#endif