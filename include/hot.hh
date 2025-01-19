#pragma once
#include "util.hh"
#include "config.hh"
#include "data.hh"
#include "threadpool.hh"

extern Cloud::BackupInfoManager *_biManager;
extern ckflogs::Logger::Ptr _logger;

namespace Cloud
{
    // 获取备份文件夹目录，遍历其中所有备份文件，对每一个备份文件进行热点判断
    // 热点判断：当前时间 与 文件最近一次修改时间的差值，是否小于热点时间，是则为热点文件
    // 若备份文件是非热点文件，对其进行压缩，删除原文件，修改备份数据pack_flag

    class HotManager // 热点管理器
    {
    public:
        HotManager();
        bool run(); // 运行热点管理器

    private:
        bool isHot(const std::string &realPath);           // 热点判断
        bool NotHotHandler(Cloud::BackupInfo backupInfo); // 非热点文件的处理函数
    };
}

Cloud::HotManager::HotManager()
{
}

// 运行热点管理模块
bool Cloud::HotManager::run()
{
    //不断扫描backup_dir文件夹中
    //发现非热点文件，进行压缩

    // 1.获取备份文件目录
    Util::FileUtil dir(Config::getInstance()->getBackupDir());

    //等待备份文件目录backup_dir被创建
    while(!dir.isExists()){
        sleep(1);
    }

    std::vector<std::string> backupDirs;
    while (true)
    {
        // 2.获取backup_dir中的所有子目录
        dir.scanDirectory(backupDirs);
        if (backupDirs.empty())
        {
            usleep(1000);
            continue;
        }

        // 3.遍历每个子目录的每一个文件的路径
        std::vector<std::string> backups;
        for (const std::string &backupDir : backupDirs)
        {
            Util::FileUtil subDir(backupDir);
            subDir.scanDirectory(backups);
            
            for (const std::string &backupPath : backups)
            {
                // 获取备份信息
                BackupInfo bi;
                _biManager->getOneByRealPath(backupPath, &bi);
                
                // if (_biManager->getOneByRealPath(backupPath, &bi) == false)
                // {
                //     // 备份信息不存在
                //     bi = BackupInfo(backupPath);
                // }

                // 三种情况，不用处理
                // 文件不存在 or 正在进行压缩 or 是热点文件

                // 为什么遍历到文件，文件还可能出现不存在的情况？
                // 因为这里获取完备份信息bi（副本）时，可能刚好bi (本体) 被（处理压缩工作的线程）修改了
                // 即文件异步压缩完成，从backup_dir中删除，所以文件不存在于扫描的文件夹中了

                if (!Util::FileUtil(backupPath).isExists() || bi.is_packing || isHot(backupPath))
                {
                    continue;
                }

                // 进入非热点文件的处理
                bi.is_packing = true;
                if (_biManager->update(bi.url, bi))
                {
                    // 异步处理：将非热点文件处理工作（包括压缩、删除）交给线程池，由线程池中的工作线程处理压缩逻辑
                    auto func = std::bind(&Cloud::HotManager::NotHotHandler, this, std::placeholders::_1);
                    auto ret = ckf::ThreadPool::getInstance().submit(ckf::ThreadPool::LV1, func, bi);
                }
            }
            backups.clear();
        }

        backupDirs.clear();
        usleep(1000);
    }
    return true;
}

bool Cloud::HotManager::NotHotHandler(Cloud::BackupInfo bi)
{
    _logger->_debug("非热点文件 %s, 开始处理", bi.real_path.c_str());
    time_t begin = time(nullptr);

    Util::FileUtil fu(bi.real_path);

    // 1.压缩，并放入压缩包文件夹
    if (!fu.compress(bi.pack_path))
        return false;

    // 2.修改备份信息
    bi.pack_flag = true;

    // 3.删除原备份文件
    if (!fu.remove())
        return false;

    // 4.压缩工作结束
    bi.is_packing = false;

    // 5.更新备份信息
    _biManager->update(bi.url, bi);

    time_t end = time(nullptr);
    _logger->_debug("非热点文件 %s, 处理成功 - 用时: %d", bi.pack_path.c_str(), end - begin);
    return true;
}

bool Cloud::HotManager::isHot(const std::string &realPath) // 判断path是否为热点文件
{
    // 1.获取热点时间
    time_t hot_time = Config::getInstance()->getHotTime();
    // 2.获取文件的最近访问时间
    Util::FileUtil fu(realPath);
    time_t mtime = fu.lastModTime();

    // 3.获取当前时间
    time_t cur_time = std::time(nullptr);
    // 4.判断
    if (cur_time - mtime <= hot_time)
        return true;
    else
        return false;
}