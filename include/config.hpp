#pragma once
#include <iostream>
#include <mutex>
#include "util.hpp"
#include "log/ckflog.hpp"

#define CONFIG_FILE "../config/cloud.conf"

namespace Cloud
{
    class Config
    {
    private:
        static Config *_instance;
        static std::mutex _mutex;
        Config();
        bool readConfigFile();

    private:
        time_t _hot_time;          // 热点判断时间
        std::string _url_prefix;   // 文件下载url前缀
        std::string _arc_suffix;   // 压缩包后缀
        std::string _backup_dir;   // 服务端备份文件存储目录
        std::string _pack_dir;     // 服务端压缩文件存储目录
        std::string _svr_ip;       // 服务端ip地址
        unsigned _svr_port;        // 服务端端口号
        std::string _manager_file; // 备份信息

    public:
        time_t getHotTime() const;
        std::string getUrlPrefix() const;
        std::string getArcSuffix() const;
        std::string getBackupDir() const;
        std::string getPackDir() const;
        std::string getSvrIP() const;
        unsigned getSvrPort() const;
        std::string getManagerFile() const;

    public:
        static Config *getInstance();
    };

}
// 静态成员初始化
Cloud::Config *Cloud::Config::_instance = nullptr;
std::mutex Cloud::Config::_mutex;

Cloud::Config::Config()
{
    if (!this->readConfigFile())
    {
        exit(-1);
    }
}

bool Cloud::Config::readConfigFile()
{
    // 读取配置文件内容
    Util::FileUtil fu(CONFIG_FILE);
    std::string content;
    if (!fu.getContent(content))
    {
        DF_ERROR("Read config file failed");
        return false;
    }

    // 反序列化为Json对象
    Json::Value conf;
    if (!Util::JsonUtil::unserialize(content, &conf))
    {
        DF_ERROR("Config file - json unserialize failed");
        return false;
    }

    // 利用获得Json对象，初始化Config
    _hot_time = (time_t)conf["hot_time"].asUInt();
    _url_prefix = conf["url_prefix"].asString();
    _arc_suffix = conf["arc_suffix"].asString();
    _backup_dir = conf["backup_dir"].asString();
    _pack_dir = conf["pack_dir"].asString();
    _svr_ip = conf["svr_ip"].asString();
    _svr_port = conf["svr_port"].asUInt();
    _manager_file = conf["manager_file"].asString();
    return true;
}

Cloud::Config *Cloud::Config::getInstance()
{
    if (_instance == nullptr)
    {
        std::unique_lock<std::mutex> lck(_mutex);
        if (_instance == nullptr)
            _instance = new Config; // 创建一个单例对象
    }
    return _instance;
}

// Getters implementation
time_t Cloud::Config::getHotTime() const
{
    return _hot_time;
}

std::string Cloud::Config::getUrlPrefix() const
{
    return _url_prefix;
}

std::string Cloud::Config::getArcSuffix() const
{
    return _arc_suffix;
}

std::string Cloud::Config::getBackupDir() const
{
    return _backup_dir;
}

std::string Cloud::Config::getPackDir() const
{
    return _pack_dir;
}

std::string Cloud::Config::getSvrIP() const
{
    return _svr_ip;
}

unsigned Cloud::Config::getSvrPort() const
{
    return _svr_port;
}

std::string Cloud::Config::getManagerFile() const
{
    return _manager_file;
}