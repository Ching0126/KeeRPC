#include "KeeRPCconfig.h"

#include<cstdio>//读文件
#include<cstdlib>//退出程序
#include<memory>//智能指针

void KeeRPCconfig::LoadConfigFile(const char *config_file){//打开配置文件，放入config_map
    FILE *raw = std::fopen(config_file,"r");//打开文件,文件仍在磁盘，还没进内存。
    if(raw == nullptr){
        std::fprintf(stderr,"open config file failed:%s\n",config_file);
        std::exit(EXIT_FAILURE);
    }
    std::unique_ptr<FILE,decltype(&std::fclose)> pf(raw,&std::fclose);//，使用智能指针，指定析构方式

    char buf[1024];
    //std::fgets(buf, sizeof(buf), raw)//如果不用指针
    while(std::fgets(buf,sizeof(buf),pf.get()) != nullptr){//fgets第一行
        std::string read_buf(buf);
        Trim(read_buf);

        if(read_buf.empty() || read_buf[0] == '#'){
            continue;
        }

        size_t equal_pos = read_buf.find('=');
        if(equal_pos == std::string::npos){
            continue;
        }

        std::string key = read_buf.substr(0,equal_pos);
        Trim(key);

        std::string value = read_buf.substr(equal_pos + 1);
        Trim(value);

        config_map.insert({key, value});
    }
}

std::string KeeRPCconfig::Load(const std::string &key){//按key查值
    auto it = config_map.find(key);
    if(it == config_map.end()){
        return "";
    }
    return it->second;
}

void KeeRPCconfig::Trim(std::string &read_buf){//删除空格，制表符，回车，换行。中间空白保留
    const char *blank = " \t\r\n";//空格，指标，回车，换行
    size_t start = read_buf.find_first_not_of(blank);//按照转义后的4个拆开
    if(start == std::string::npos){
        read_buf.clear();
        return ;
    }
    size_t end = read_buf.find_last_not_of(blank);
    read_buf = read_buf.substr(start,end-start+1);
}