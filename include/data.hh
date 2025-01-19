#pragma once
#include <unordered_map>
#include "util.hh"
#include "config.hh"

extern ckflogs::Logger::Ptr _logger;

namespace Cloud
{
    typedef struct BackupInfo // 备份文件数据
    {
        bool pack_flag;        // 文件是否已压缩的标志
        bool is_packing;       // 文件正在压缩中
        size_t fsize;          // 文件大小
        time_t atime;          // 最近访问时间
        time_t mtime;          // 最近修改时间
        std::string real_path; // 文件备份包存储路径
        std::string pack_path; // 文件压缩包存储路径
        std::string url;       // 文件url
        int userID;            // 所属用户id

        BackupInfo();
        BackupInfo(const std::string &backupPath, int userId);
    } BackupInfo;

    class BackupInfoManager // 文件数据管理器
    {
    private:
        std::unordered_map<std::string, std::unique_ptr<BackupInfo>> _table; // url映射文件数据的表
        Util::FileUtil _manager_file;                                        // 持久化备份文件数据
        pthread_rwlock_t _rwlock;                                            // 读写锁

    public:
        BackupInfoManager();
        ~BackupInfoManager();

        bool initLoad();                                            // 从备份管理文件中读取文件元信息（初始化）
        bool storage();                                             // 保存文件元信息到备份管理文件（持久化）

        bool insert(const std::string &key, const BackupInfo &val); // 插入一个文件数据
        bool update(const std::string &key, const BackupInfo &val); // 修改一个文件数据
        bool getOneByURL(const std::string &url, BackupInfo *val);
        bool getOneByRealPath(const std::string &realPath, BackupInfo *val);
        bool getAll(std::vector<BackupInfo> *array);
    };
}

// BackupInfo
Cloud::BackupInfo::BackupInfo() : is_packing(false)
{
}

Cloud::BackupInfo::BackupInfo(const std::string &backupPath, int userId)
{
    // 根据文件实际存储路径，填充文件元信息
    Util::FileUtil fu(backupPath);
    if (!fu.isExists())
    {
        DF_ERROR("%s 文件不存在", backupPath.c_str());
        return;
    }

    pack_flag = false;
    is_packing = false;
    fsize = fu.fileSize();
    atime = fu.lastAccessTime();
    mtime = fu.lastModTime();
    real_path = backupPath;
    userID = userId;

    //拼接pack_path和url
    Cloud::Config *conf = Cloud::Config::getInstance();
    std::string suffix = backupPath.substr(conf->getBackupDir().size());
    pack_path = conf->getPackDir() + suffix + conf->getArcSuffix();
    url = conf->getUrlPrefix() + suffix;

    _logger->_debug("real_path: %s, pack_path: %s, url: %s", real_path.c_str(), pack_path.c_str(), url.c_str());
}


// BackupInfoManager
Cloud::BackupInfoManager::BackupInfoManager()
    : _manager_file(Cloud::Config::getInstance()->getManagerFile())
{
    pthread_rwlock_init(&_rwlock, nullptr);
    
    if(!initLoad())//初始化备份文件元信息
    {
        _logger->_error("备份信息初始化失败");
        exit(-1);
    }
    _logger->_debug("数据管理模块-备份信息初始化成功, 当前文件个数 %d", _table.size());
}

Cloud::BackupInfoManager::~BackupInfoManager()
{
    pthread_rwlock_destroy(&_rwlock);
}

// 原来Json可以当作数组用！遂修改如下
bool Cloud::BackupInfoManager::initLoad()
{
    if (!_manager_file.isExists())//没有备份信息
        return true;

    // 1.从备份文件中读出文件数据
    std::string backup;
    if (!_manager_file.getContent(backup))
    {
        DF_ERROR("Get backup file failed");
        return false;
    }

    if(backup.empty())//备份信息为空
        return true;

    // 2.反序列为Json
    Json::Value root;
    if (!Util::JsonUtil::unserialize(backup, &root))
    {
        DF_ERROR("Json unserialize failed");
        return false;
    }

    // 3.初始化
    for (int i = 0; i < root.size(); i++)
    {
        Json::Value item = root[i];

        BackupInfo bi;
        bi.atime = item["atime"].asUInt();
        bi.mtime = item["mtime"].asUInt();
        bi.fsize = item["fsize"].asUInt();
        bi.pack_flag = item["pack_flag"].asBool();
        bi.pack_path = item["pack_path"].asString();
        bi.real_path = item["real_path"].asString();
        bi.url = item["url"].asString();
        bi.userID = item["userID"].asInt();
        
        insert(bi.url, bi);
    }

    return true;
}

bool Cloud::BackupInfoManager::storage()
{
    if (_table.empty())
    {
        DF_WARN("No BackupInfo need to storage");
        return false;
    }
    // 1.获取所有文件元信息
    std::vector<BackupInfo> array;
    getAll(&array);

    // 2.将文件元信息转化为json对象（root视为Json数组）
    Json::Value root;

    Util::RDLockGuard(&this->_rwlock);
    for (auto v : array)
    {
        Json::Value item;
        item["pack_flag"] = v.pack_flag;
        item["fsize"] = static_cast<Json::UInt64>(v.fsize);
        item["atime"] = static_cast<Json::Int64>(v.atime);
        item["mtime"] = static_cast<Json::Int64>(v.mtime);
        item["real_path"] = v.real_path;
        item["pack_path"] = v.pack_path;
        item["url"] = v.url;
        item["userID"] = v.userID;

        root.append(item);
    }

    // 3.序列化json
    std::string str;
    if (!Util::JsonUtil::serialize(root, &str))
    {
        DF_ERROR("Json serialize failed");
        return false;
    }

    // 4.持久化存储
    if (!_manager_file.setContent(str))
    {
        DF_ERROR("Set backup file failed");
        return false;
    }

    return true;
}

bool Cloud::BackupInfoManager::insert(const std::string &key, const BackupInfo &val)
{
    Util::WRLockGuard(&this->_rwlock); // 读写锁，不能并行读写

    if (_table.count(key) != 0) // 已存在
    {
        DF_WARN("BackupInfo exists")
        return false;
    }
    BackupInfo *newbi = new BackupInfo(val);
    _table[key] = std::unique_ptr<BackupInfo>(newbi);
    storage(); // 持久化文件元信息
    return true;
}

// 有则替换，无则插入
bool Cloud::BackupInfoManager::update(const std::string &key, const BackupInfo &val)
{
    Util::WRLockGuard(&this->_rwlock); // 读写锁，不能并行读写

    if (_table.count(key) == 0) // 不存在
    {
        _table[key] = std::unique_ptr<BackupInfo>(new BackupInfo(val));
    }
    else // 存在
    {
        *_table[key] = val;
    }
    storage(); // 持久化文件元信息
    return true;
}

bool Cloud::BackupInfoManager::getOneByURL(const std::string &url, BackupInfo *val)
{
    Util::RDLockGuard(&this->_rwlock); //读锁，可以并行读

    if (_table.count(url) == 0) // 不存在
    {
        DF_WARN("BackupInfo not exists")
        return false;
    }

    *val = *_table[url].get();
    return true;
}

bool Cloud::BackupInfoManager::getOneByRealPath(const std::string &realPath, BackupInfo *val)
{
    Util::RDLockGuard(&this->_rwlock);//读锁，可以并行读

    for (auto &[k, v] : _table)
    {
        if (v->real_path == realPath)
        {
            *val = *v.get();
            return true;
        }
    }
    return false;
}

bool Cloud::BackupInfoManager::getAll(std::vector<BackupInfo> *array)
{
    Util::RDLockGuard(&this->_rwlock);//读锁，可以并行读

    for (auto &[k, v] : _table)
    {
        (*array).push_back(*v.get());
    }
    return true;
}
