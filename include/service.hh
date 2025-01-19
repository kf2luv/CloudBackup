#pragma once
#include "util.hh"
#include "config.hh"
#include "data.hh"
#include "httplib.h"
#include "user.hh"

extern Cloud::BackupInfoManager *_biManager;
extern ckflogs::Logger::Ptr _logger;

namespace Cloud
{

    class Service
    {
    public:
        Service();
        void run();

    private:
        static void index(const httplib::Request &req, httplib::Response &resp);         // 登录索引界面
        static void registerIndex(const httplib::Request &req, httplib::Response &resp); // 注册索引界面

        static void signup(const httplib::Request &req, httplib::Response &resp); // 用户注册
        static void login(const httplib::Request &req, httplib::Response &resp);  // 用户登录

        static void upload(const httplib::Request &req, httplib::Response &resp);   // 文件上传
        static void download(const httplib::Request &req, httplib::Response &resp); // 文件下载

        static void listShow(const httplib::Request &req, httplib::Response &resp);   // 文件列表展示
        static void uploadShow(const httplib::Request &req, httplib::Response &resp); // 上传页面展示
        static void updateList(const httplib::Request &req, httplib::Response &resp); // 前端更新文件列表

        static std::string getETag(const std::string &url);

    private:
        int _svr_port;                   // 端口号
        std::string _svr_ip;             // 服务端ip
        httplib::Server _svr;            // 服务器
        static UserManager _userManager; // 用户管理
    };
    UserManager Service::_userManager;
}

Cloud::Service::Service()
{
    Config *conf = Config::getInstance();
    _svr_port = conf->getSvrPort();
    _svr_ip = conf->getSvrIP();

    // 在当前工作目录创建 备份文件目录 和 压缩文件目录（如果不存在的话）
    Util::FileUtil backupDir(conf->getBackupDir());
    Util::FileUtil packDir(conf->getPackDir());
    backupDir.createDirectory();
    packDir.createDirectory();
}

void Cloud::Service::run()
{
    _svr.Get("/", index);                 // 登录索引
    _svr.Get("/register", registerIndex); // 注册索引

    _svr.Post("/login", login);   // 用户登录
    _svr.Post("/signup", signup); // 用户注册

    _svr.Post("/upload", upload);       // 文件上传
    _svr.Get("/download/.*", download); // 文件下载

    _svr.Get("/uploadShow", uploadShow); // 文件上传展示页面
    _svr.Get("/list", listShow);         // 文件列表展示
    _svr.Get("/file-list", updateList);  // 前端页面更新文件列表

    if (!_svr.listen("0.0.0.0", _svr_port))
    {
        _logger->_fatal("服务器监听失败 %s", strerror(errno));
        exit(-2);
    }
}

void Cloud::Service::index(const httplib::Request &req, httplib::Response &resp)
{
    resp.set_file_content("../www/index.html");
    resp.set_header("Content-Type", "text/html");
    resp.status = 200;
}

inline void Cloud::Service::registerIndex(const httplib::Request &req, httplib::Response &resp)
{
    resp.set_file_content("../www/registerIndex.html");
    resp.set_header("Content-Type", "text/html");
    resp.status = 200;
}

void Cloud::Service::signup(const httplib::Request &req, httplib::Response &resp)
{
    // 从请求体中获取用户名和密码
    Json::Value root;
    Util::JsonUtil::unserialize(req.body, &root);
    std::string username = root["username"].asCString();
    std::string password = root["password"].asCString();

    // 检查用户是否已经存在
    if (_userManager.checkUser(username, password) == true)
    {
        resp.status = 409;
        resp.set_content("Username already exists", "text/plain");
        return;
    }

    // 新增用户, 并获取用户ID
    int userId = 0;
    _userManager.addUser(username, password, userId);

    // 为用户新建专属目录 (用户ID作为目录名)
    Config *conf = Config::getInstance();
    std::string dirName = _userManager.getDirName(userId);

    Util::FileUtil backupDir(conf->getBackupDir() + dirName);
    Util::FileUtil packDir(conf->getPackDir() + dirName);

    if (!backupDir.createDirectory())
    {
        _logger->_warn("用户备份目录创建失败, id: %d", userId);
        return;
    }
    if (!packDir.createDirectory())
    {
        _logger->_warn("用户压缩目录创建失败, id: %d", userId);
        return;
    }

    // 返回注册成功的响应
    resp.status = 201;
    resp.set_content("User registered successfully", "text/plain");
}

void Cloud::Service::login(const httplib::Request &req, httplib::Response &resp)
{
    // 获取用户名和密码
    Json::Value root;
    Util::JsonUtil::unserialize(req.body, &root);
    std::string username = root["username"].asCString();
    std::string password = root["password"].asCString();

    // 查看用户数据库，验证[用户名-密码]是否合法
    if (_userManager.checkUser(username, password) == true)
    {
        // 为该用户生成一个sessionID，并保存当前会话
        std::string sessionID = _userManager.generateSessionID();
        int userID = _userManager.userId(username);

        _userManager.storeSession(sessionID, userID);

        _logger->_debug("sessionID: %s 已保存, 用户id: %d", sessionID.c_str(), userID);

        // 设置响应的Cookie头
        resp.set_header("Set-Cookie", "session_id=" + sessionID);

        // 返回 HTTP 重定向的地址
        Json::Value response;
        response["redirect"] = "/list";
        resp.status = 200;
        resp.set_content(response.toStyledString(), "application/json");
    }
    else
    {
        // 用户验证失败，返回错误信息
        resp.status = 401; // Unauthorized
        resp.set_content("Invalid username or password", "text/plain");
    }
}

void Cloud::Service::upload(const httplib::Request &req, httplib::Response &resp)
{
    // 0.获取sessionID
    auto it = req.headers.find("Cookie");
    std::string sessionID = it->second.substr(it->second.find("=") + 1);

    // sessionID不存在，重新登录，以获取新的sessionID
    if (!_userManager.checkSessionID(sessionID))
    {
        resp.set_redirect("/");
        return;
    }

    // 处理请求
    // 1.获取上传的文件内容  (一次只传一个文件)
    if (!req.has_file("file"))
    {
        resp.status = 400;
        resp.set_content("File not exists", "text/plain");
        return;
    }
    auto fileData = req.get_file_value("file"); // 获取一个文件

    // 2.根据sessionID获取当前用户的userID，得到用户对应的目录名
    int userID = _userManager.sessionUserID(sessionID);
    if (userID < 0)
    {
        assert(false);
    }

    _logger->_debug("用户会话存在: sessionID: %s, 用户id: %d", sessionID.c_str(), userID);

    std::string dirName = _userManager.getDirName(userID) + "/";

    // 3.添加新文件 (持久化存储)
    // 根据上传文件的用户名，存储到对应用户的文件中（注册时已创建），构建对应的文件路径
    std::string backupPath = Config::getInstance()->getBackupDir() + dirName + fileData.filename;

    Util::FileUtil fu(backupPath);
    if (!fu.setContent(fileData.content))
    {
        _logger->_warn("用户上传文件保存失败: %s", backupPath.c_str());
        return;
    }

    _logger->_debug("用户文件上传成功: %s", backupPath.c_str());

    // 4.添加备份信息（文件元信息）
    BackupInfo newbi(backupPath, userID);
    if (!_biManager->update(newbi.url, newbi))
    {
        _logger->_warn("用户文件备份信息添加失败: %s", backupPath.c_str());
        return;
    }

    // 5.返回响应
    resp.status = 200;
    resp.set_content("Upload successful", "text/plain");

    _logger->_debug("用户上传文件已存入: %s", backupPath.c_str());
}

void Cloud::Service::download(const httplib::Request &req, httplib::Response &resp)
{
    // 0.获取sessionID
    auto it = req.headers.find("Cookie");
    std::string sessionID = it->second.substr(it->second.find("=") + 1);

    // sessionID不存在，重新登录，以获取新的sessionID
    if (!_userManager.checkSessionID(sessionID))
    {
        resp.set_redirect("/");
        return;
    }

    // 1.ETag缓存判断机制
    if (req.has_header("If-None-Match"))
    {
        std::string oldETag = req.get_header_value("If-None-Match");

        // 对比请求头中的ETag和服务器本地文件的ETag是否匹配
        if (oldETag == getETag(req.path))
        {
            // 匹配
            resp.status = 304;
            resp.reason = "Not Modified";
            return;
        }
    }

    // 2.以URL查找文件
    BackupInfo bi;
    if (!_biManager->getOneByURL(req.path, &bi))
    {
        // 文件不存在
        resp.status = 404;
        resp.set_content("File not found", "text/plain");
        return;
    }

    // 3.判断文件是否为热点文件，若不是，需要先解压
    // 若文件正在压缩中，需要等待其压缩结束，再解压

    if (bi.pack_flag == true)
    {
        // 非热点文件 -> 热点文件
        Util::FileUtil fu(bi.pack_path);
        fu.uncompress(bi.real_path);
        bi.pack_flag = false;
        fu.remove();
        _biManager->update(bi.url, bi);

        _logger->_debug("热点文件: %s 处理成功", bi.real_path.c_str());
    }

    // 判断是否为断点续传请求
    if (req.has_header("If-Range"))
    {
        // 判断客户端的etag是否与当前etag相等
        // 若不相等，表示服务端对文件进行了修改，客户端不能进行断点续传，需要重新下载
        std::string old_etag = req.get_header_value("If-Range");
        if (old_etag == getETag(req.path))
        {
            Util::FileUtil fu(bi.real_path);
            fu.getContent(resp.body); // cpp-httplib库内置处理断点续传

            resp.set_header("ETag", getETag(req.path));
            resp.set_header("Accept-Ranges", "bytes");
            resp.status = 206;
            resp.reason = "Partial Content";
            return;
        }
    }

    // 4.获取文件内容，填充响应
    resp.set_file_content(bi.real_path);
    // 设置 Content-Disposition 以便下载文件而不是直接在浏览器显示
    std::string filename = Util::FileUtil(bi.real_path).fileName();
    resp.set_header("Content-Disposition", "attachment; filename=" + filename);
    // 设置ETag
    resp.set_header("ETag", getETag(req.path));
    // 设置接受断点续传
    resp.set_header("Accept-Ranges", "bytes");
    // 设置状态码和状态描述
    resp.status = 200;
    resp.reason = "OK";
}

void Cloud::Service::listShow(const httplib::Request &req, httplib::Response &resp)
{
    // 获取sessionID
    auto it = req.headers.find("Cookie");
    std::string sessionID = it->second.substr(it->second.find("=") + 1);

    // sessionID不存在，重新登录，以获取新的sessionID
    if (!_userManager.checkSessionID(sessionID))
    {
        resp.set_redirect("/");
        return;
    }

    resp.set_file_content("../www/list.html");
    resp.set_header("Content-Type", "text/html");
    resp.status = 200;
}

void Cloud::Service::uploadShow(const httplib::Request &req, httplib::Response &resp)
{
    // 获取sessionID
    auto it = req.headers.find("Cookie");
    std::string sessionID = it->second.substr(it->second.find("=") + 1);

    // sessionID不存在，重新登录，以获取新的sessionID
    if (!_userManager.checkSessionID(sessionID))
    {
        resp.set_redirect("/");
        return;
    }

    resp.set_file_content("../www/upload.html");
    resp.set_header("Content-Type", "text/html");
    resp.status = 200;
}

void Cloud::Service::updateList(const httplib::Request &req, httplib::Response &resp)
{
    // 1.获取可下载的文件列表(热点 or 非热点都可下载)
    std::vector<Cloud::BackupInfo> list;
    _biManager->getAll(&list);

    auto time_tToDateString = [](time_t time)
    {
        struct tm *timeinfo = std::localtime(&time);
        char buffer[80];
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);
        return std::string(buffer);
    };

    auto size_tToString = [](size_t sz)
    {
        static const size_t B = 1; // 1字节
        static const size_t K = 1024 * B;
        static const size_t M = 1024 * K;
        static const size_t G = 1024 * M;

        if (sz < K)
            return std::to_string(sz) + "B";
        else if (sz >= K && sz < M)
            return std::to_string(sz / K) + "KB";
        else if (sz >= M && sz < G)
            return std::to_string(sz / M) + "MB";
        else
            return std::to_string(sz / G) + "GB";
    };

    // 2.获取sessionID，表明当前用户，只返回当前用户的文件信息
    auto it = req.headers.find("Cookie");
    std::string sessionID = it->second.substr(it->second.find("=") + 1);

    int userID = _userManager.sessionUserID(sessionID);
    if (userID < 0)
    {
        assert(false);
    }

    Json::Value root;

    // 3.获取用户名
    std::string username = _userManager.userName(userID);
    root["username"] = username;

    // 3.遍历文件信息，组织成json
    Json::Value fileList;
    for (auto &info : list)
    {
        if (info.userID == userID)
        {
            Json::Value item;
            Config *conf = Config::getInstance();
            item["downloadUrl"] = info.url;

            item["fileName"] = info.url.substr(info.url.find_last_of('/') + 1);
            item["fileSize"] = size_tToString(info.fsize);
            item["lastModified"] = time_tToDateString(info.mtime);

            fileList.append(item);
        }
    }
    root["files"] = fileList;

    std::string jsonStr;
    Util::JsonUtil::serialize(root, &jsonStr);
    resp.set_content(jsonStr, "application/json");
}

std::string Cloud::Service::getETag(const std::string &url)
{
    Cloud::BackupInfo bi;
    _biManager->getOneByURL(url, &bi);

    // 文件名-文件大小-最近修改时间
    std::string fileName = Util::FileUtil(bi.real_path).fileName();
    std::string fileSize = std::to_string(bi.fsize);
    std::string lastMTime = std::to_string(bi.mtime);
    return fileName + '-' + fileSize + '-' + lastMTime;
}
