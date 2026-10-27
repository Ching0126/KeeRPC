#ifndef KEERPC_CONFIG_H
#define KEERPC_CONFIG_H

#include<string>
#include<unordered_map>

class KeeRPCconfig{
public:

    void LoadConfigFile(const char *config_file);//读取并解析配置

    std::string Load(const std::string &key);//按键取值
private:

    std::unordered_map<std::string,std::string> config_map;
    // 解析结果。键是配置名，值是去掉首尾空白后的内容。

    void Trim(std::string &read_buf);//为 LoadConfigFile服务 删除空格，制表符，回车，换行。中间空白保留
};

#endif