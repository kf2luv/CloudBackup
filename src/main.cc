#include "data.hh"
#include "hot.hh"
#include "service.hh"
#include "log/ckflog.hpp"
#include <thread>
#include <memory>

Cloud::BackupInfoManager* _biManager;
ckflogs::Logger::Ptr _logger;

void hotModuleHandler()//热点管理模块
{
    Cloud::HotManager hotManager;
    hotManager.run();
}

void serviceModuleHandler()//业务处理模块
{
    Cloud::Service service;
    service.run();
}

void loggerBuild()
{
    ckflogs::LoggerBuilder::Ptr builder = std::make_shared<ckflogs::GlobalLoggerBuilder>();
    builder->buildSinker<ckflogs::FileLogSinker>("./cloud.log");
    builder->bulidType(ckflogs::Logger::LoggerType::LOGGER_SYNC);
    builder->bulidName("CloudLogger");
    builder->build();
    _logger = ckflogs::getLogger("CloudLogger");
}

int main()
{
    loggerBuild();

    _biManager = new Cloud::BackupInfoManager; //备份文件信息管理模块

    std::thread hot(hotModuleHandler);
    std::thread service(serviceModuleHandler);
    
    hot.join();
    service.join();

    delete _biManager;
    return 0;
}