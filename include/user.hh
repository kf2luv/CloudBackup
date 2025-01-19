#include <iostream>
#include <memory>
#include <sstream>
#include <iomanip>
#include <unordered_map>
#include <chrono>
#include <random>
#include "log/ckflog.hpp"

// mysql
#include <cppconn/driver.h>
#include <cppconn/connection.h>
#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>
#include <cppconn/exception.h>

extern ckflogs::Logger::Ptr _logger;

// 用户操作工具类
class UserManager
{
public:
    UserManager()
        : _conn(get_driver_instance()->connect("tcp://123.249.9.114:3306", "kf", "123456"))
    {
        // 连接用户数据库
        if (!_conn->isValid())
        {
            abort();
        }
        // _logger->_debug("用户数据库连接成功");//此时_logger还没有初始化，不能使用

        // 选择数据库
        _conn->setSchema("cloud");
    }
    //验证用户是否合法（检查用户是否存在，存在的话密码是否正确）
    bool checkUser(const std::string &username, const std::string &password);
    //新增用户
    bool addUser(const std::string &username, const std::string &password, int &userId);
    //根据用户id生成目录名称
    std::string getDirName(int userId);
    //根据用户名查找用户id（从数据库中）
    int userId(const std::string &username);
    //根据用户id查找用户名（从数据库中）
    std::string userName(int userId);


    //生成一个唯一的sessionId
    std::string generateSessionID();
    //存储用户会话信息
    void storeSession(const std::string& sessionID, int userID);
    //返回当前会话的用户id，不存在返回-1
    int sessionUserID(const std::string& sessionID);
    //检查sessionID是否存在
    bool checkSessionID(const std::string& sessionID);
    
private:
    std::unique_ptr<sql::Connection> _conn;
    std::unordered_map<std::string, int> _sessions; // 存储session信息 <seesionID, 用户ID>
    std::mutex _mtx;                                // 保护_sessions
};

bool UserManager::checkUser(const std::string &username, const std::string &password)
{
    if (username.empty() || password.empty())
    {
        return false;
    }
    try
    {
        // 创建预编译语句
        std::unique_ptr<sql::PreparedStatement> pstmt(
            _conn->prepareStatement("SELECT COUNT(*) FROM users WHERE username = ? AND password = ?"));

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

bool UserManager::addUser(const std::string &username, const std::string &password, int &userId)
{
    if (username.empty() || password.empty())
    {
        return false;
    }
    std::unique_ptr<sql::PreparedStatement> insertStmt(_conn->prepareStatement(
        "INSERT INTO users (username, password) VALUES (?, ?)"));
    insertStmt->setString(1, username);
    insertStmt->setString(2, password);
    insertStmt->executeUpdate();

    // 查询最后插入的 ID
    std::unique_ptr<sql::PreparedStatement> idStmt(
        _conn->prepareStatement("SELECT LAST_INSERT_ID()"));
    std::unique_ptr<sql::ResultSet> result(idStmt->executeQuery());
    if (result->next())
    {
        userId = result->getInt(1);
    }
    return true;
}

std::string UserManager::getDirName(int userId)
{
    std::ostringstream oss;
    oss << std::setw(5) << std::setfill('0') << userId;
    return oss.str();
}

int UserManager::userId(const std::string &username)
{
    assert (!username.empty());

    std::unique_ptr<sql::PreparedStatement> pstmt(
        _conn->prepareStatement("SELECT id FROM users WHERE username = ?"));

    pstmt->setString(1, username);

    std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());

    if (res->next())
    {
        return res->getInt(1);
    }
    return -1;
}

std::string UserManager::userName(int userId)
{
    assert(userId > 0);

    std::unique_ptr<sql::PreparedStatement> pstmt(
        _conn->prepareStatement("SELECT username FROM users WHERE id = ?"));

    pstmt->setInt(1, userId);
    std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());

    if (res->next())
    {
        return res->getString(1);
    }
    return "";
}

std::string UserManager::generateSessionID()
{
    // 获取当前时间戳
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();

    // 生成随机数
    std::random_device rd;
    std::mt19937 generator(rd());
    std::uniform_int_distribution<int> dist(0, 99999);

    // 拼接时间戳和随机数
    std::ostringstream oss;
    oss << millis << dist(generator);
    return oss.str();
}

void UserManager::storeSession(const std::string& sessionID, int userID)
{
    std::unique_lock<std::mutex> lockGuard(_mtx);

    _sessions[sessionID] = userID;
}

int UserManager::sessionUserID(const std::string& sessionID)
{
    std::unique_lock<std::mutex> lockGuard(_mtx);

    auto it = _sessions.find(sessionID);
    if (it != _sessions.end())
    {
        return it->second;
    }
    return -1;
}

inline bool UserManager::checkSessionID(const std::string &sessionID)
{
    std::unique_lock<std::mutex> lockGuard(_mtx);

    return _sessions.find(sessionID) != _sessions.end();
}
