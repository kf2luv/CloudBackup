#pragma once
#include <iostream>
#include <fstream>
#include <memory>
#include <sstream>
#include <sys/stat.h>
#include <vector>
#include <experimental/filesystem>
#include <pthread.h>
#include <cassert>

#include "jsoncpp/json/json.h"
#include "bundle.h"
#include "log/ckflog.hpp"

//mysql
#include <cppconn/driver.h>
#include <cppconn/connection.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>
#include <cppconn/exception.h>

namespace Util
{
    namespace fs = std::experimental::filesystem;

    bool checkUser(std::shared_ptr<sql::Connection> conn, const std::string &username, const std::string &password)
    {
        try
        {
            // 创建预编译语句
            std::unique_ptr<sql::PreparedStatement> pstmt(
                conn->prepareStatement("SELECT COUNT(*) FROM users WHERE username = ? AND password = ?"));

            // 设置参数
            pstmt->setString(1, username);
            pstmt->setString(2, password);

            // 执行查询
            std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());

            // 检查结果
            if (res->next())
            {
                return res->getInt(1) > 0; // 如果匹配的用户数大于 0，则验证成功
            }
        }
        catch (sql::SQLException &e)
        {
            return false;
        }
        return false;
    }

    // 文件工具类
    class FileUtil
    {
    public:
        FileUtil(const std::string &path);
        ~FileUtil();
        size_t fileSize();       // 获取文件大小
        time_t lastModTime();    // 文件最近修改时间
        time_t lastAccessTime(); // 文件最近访问时间
        std::string fileName();  // 获取文件名

        bool getContent(std::string &content);                        // 获取文件内容
        bool getPosLen(std::string &content, size_t pos, size_t len); // 获取文件的部分内容
        bool setContent(const std::string &content);                  // 设置文件内容

        bool compress(const std::string &packname);   // 压缩
        bool uncompress(const std::string &filename); // 解压

        bool isExists();                                     // 判断文件是否存在
        bool createDirectory();                              // 创建目录
        bool scanDirectory(std::vector<std::string> &array); // 扫描目录中所有文件名称
        bool remove();

    private:
        std::string _path;       // 文件路径
        struct stat *_stat;      // 文件属性
        void upDateFileStatus(); // 更新文件属性
    };

    // Json工具类
    class JsonUtil
    {
    public:
        static bool serialize(const Json::Value &root, std::string *str);
        static bool unserialize(const std::string &str, Json::Value *root);
    };

    class RDLockGuard
    {
    public:
        RDLockGuard(pthread_rwlock_t *rdlock)
            : _rdlock(rdlock)
        {
            pthread_rwlock_rdlock(_rdlock);
        }
        ~RDLockGuard()
        {
            pthread_rwlock_destroy(_rdlock);
        }

    private:
        pthread_rwlock_t *_rdlock;
    };

    class WRLockGuard
    {
    public:
        WRLockGuard(pthread_rwlock_t *wrlock)
            : _wrlock(wrlock)
        {
            pthread_rwlock_rdlock(_wrlock);
        }
        ~WRLockGuard()
        {
            pthread_rwlock_destroy(_wrlock);
        }

    private:
        pthread_rwlock_t *_wrlock;
    };
}

Util::FileUtil::FileUtil(const std::string &path)
    : _path(path), _stat(nullptr)
{
}

Util::FileUtil::~FileUtil()
{
    if (_stat)
        free(_stat);
}

size_t Util::FileUtil::fileSize()
{
    upDateFileStatus();
    return _stat->st_size;
}

time_t Util::FileUtil::lastModTime()
{
    upDateFileStatus();
    return _stat->st_mtime;
}

time_t Util::FileUtil::lastAccessTime()
{
    upDateFileStatus();
    return _stat->st_atime;
}

void Util::FileUtil::upDateFileStatus()
{
    if (_stat == nullptr)
    {
        _stat = (struct stat *)malloc(sizeof(struct stat));
    }
    stat(_path.c_str(), _stat);
}

std::string Util::FileUtil::fileName()
{
    size_t x = _path.find_last_of('/');
    if (x == std::string::npos)
        return _path;
    return _path.substr(x + 1);
}

bool Util::FileUtil::getContent(std::string &content)
{
    return getPosLen(content, 0, fileSize());
}

bool Util::FileUtil::getPosLen(std::string &content, size_t pos, size_t len)
{
    std::ifstream ifs(_path, std::ios::binary);
    if (!ifs.is_open())
    {
        DF_WARN("%s: File open fail", _path.c_str());
        return false;
    }

    size_t fileSz = this->fileSize();
    if (pos + len > fileSz)
    {
        DF_WARN("%s: The read length is too long", _path.c_str());
        ifs.close();
        return false;
    }

    content.resize(len);
    ifs.seekg(pos, ifs.beg);
    ifs.read(&content[0], len);
    if (!ifs.good())
    {
        DF_WARN("%s: Read file failed", _path.c_str());
        ifs.close();
        return false;
    }

    ifs.close();
    return true;
}

bool Util::FileUtil::setContent(const std::string &content)
{
    // 如果目录不存在，先创建目录backup_dir

    // content -> 文件
    std::ofstream ofs;
    ofs.open(_path, std::ios::binary);
    if (!ofs.is_open())
    {
        return false;
    }

    ofs.write(content.c_str(), content.size());
    if (!ofs.good())
    {
        ofs.close();
        return false;
    }
    ofs.close();
    return true;
}

bool Util::FileUtil::compress(const std::string &packname)
{
    // 压缩当前文件的内容
    std::string cont;
    if (!getContent(cont))
    {
        DF_WARN("%s: Get file content failed", _path.c_str());
        return false;
    }
    std::string packed = bundle::pack(bundle::LZIP, cont);

    // 打开压缩包文件
    std::ofstream ofs(packname, std::ios::binary);
    if (!ofs.is_open())
    {
        DF_WARN("%s: File open fail", _path.c_str());
        return false;
    }

    // 写入压缩包文件
    ofs.write(packed.c_str(), packed.size());
    if (!ofs.good())
    {
        DF_WARN("%s: Write file failed", _path.c_str());
        ofs.close();
        return false;
    }
    ofs.close();

    return true;
}

bool Util::FileUtil::uncompress(const std::string &filename)
{
    std::string cont;
    if (!getContent(cont))
    {
        DF_WARN("Get file content failed");
        return false;
    }
    std::string unpacked = bundle::unpack(cont);

    std::ofstream ofs(filename, std::ios::binary);
    if (!ofs.is_open())
    {
        DF_WARN("%s: File open fail", _path.c_str());
        return false;
    }

    ofs.write(unpacked.c_str(), unpacked.size());
    if (!ofs.good())
    {
        DF_WARN("%s: Write file failed", _path.c_str());
        ofs.close();
        return false;
    }
    ofs.close();

    return true;
}

bool Util::FileUtil::isExists()
{
    return fs::exists(_path);
}

bool Util::FileUtil::createDirectory()
{
    if (isExists())
        return true;
    return fs::create_directories(_path);
}

bool Util::FileUtil::scanDirectory(std::vector<std::string> &array)
{
    fs::directory_iterator dirIt(_path);
    for (auto &entry : dirIt)
        array.push_back(entry.path().string());
    return true;
}

bool Util::FileUtil::remove()
{
    return fs::remove(_path);
}


bool Util::JsonUtil::serialize(const Json::Value &root, std::string *str)
{
    Json::StreamWriterBuilder swb;

    std::unique_ptr<Json::StreamWriter> writer(swb.newStreamWriter());

    std::ostringstream oss;
    writer->write(root, &oss);
    if (!oss.good())
        return false;
    *str = oss.str();

    return true;
}

bool Util::JsonUtil::unserialize(const std::string &str, Json::Value *root)
{
    Json::CharReaderBuilder crb;

    std::unique_ptr<Json::CharReader> reader(crb.newCharReader());

    std::string err;
    bool ok = reader->parse(str.c_str(), str.c_str() + str.size(), root, &err);
    if (!ok)
        return false;

    return true;
}