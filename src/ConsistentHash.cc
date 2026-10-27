#include "ConsistentHash.h"

#include <algorithm>
//构造函数
ConsistentHash::ConsistentHash(int relicas):relicas_(relicas){}

uint32_t ConsistentHash::Hash(const std::string &data){//哈希转换 CRC32
    uint32_t crc = 0xFFFFFFFFu;//32个1
    for(unsigned char c : data){//按字符转换
        crc ^= c;//异或
        for(int bit = 0;bit < 8; ++bit){//字节8位按位处理
            uint32_t mask = -(crc & 1u);
            crc = (crc >> 1)  ^ (0xEDB88320u & mask);
        }
    }
    return ~crc;
}

void ConsistentHash::UpdateNodes(const std::vector<std::string> &nodes){//更新结点
    keys_.clear();
    ring_.clear();
    for(const std::string &node : nodes){
        if(node.empty()){
            continue;
        }
        for(int i = 0;i<relicas_;++i){
            uint32_t position = Hash(node + "#" + std::to_string(i));
            if(ring_.count(position)!=0){
                continue;
            }
            ring_.emplace(position,node);
            keys_.push_back(position);
        }
    }
    std::sort(keys_.begin(),keys_.end());
}

std::string ConsistentHash::GetTargetNode(const std::string &key)const{
    if(keys_.empty()){
        return "";
    }
    uint32_t position = Hash(key);
    auto it = std::lower_bound(keys_.begin(),keys_.end(),position);
    if(it == keys_.end()){
        it = keys_.begin();
    }
    return ring_.at(*it);
}