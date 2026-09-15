#pragma once

#include "spdlog/spdlog.h"
#include <memory>
#include <mutex>
#include <sstream>
#include <string>

class XLogger
{
public:
    static void init(const std::string& logFilePath = "logs/TaskControlLog.log", int level = 1);

    static std::shared_ptr<spdlog::logger> getInstance();

private:
    static std::shared_ptr<spdlog::logger> createLogger(const std::string& path, int level = 1);

    XLogger() = delete;
    ~XLogger() = delete;
    XLogger(const XLogger&) = delete;
    XLogger(XLogger&&) = delete;
    XLogger& operator=(const XLogger&) = delete;
    XLogger& operator=(XLogger&&) = delete;

private:
    static std::shared_ptr<spdlog::logger> m_logger;
    static std::mutex m_mutex;
};

// 定义输出宏
#define xTrace(...) XLogger::getInstance()->trace(__VA_ARGS__)
#define xDebug(...) XLogger::getInstance()->debug(__VA_ARGS__)
#define xInfo(...) XLogger::getInstance()->info(__VA_ARGS__)
#define xWarning(...) XLogger::getInstance()->warn(__VA_ARGS__)
#define xError(...) XLogger::getInstance()->error(__VA_ARGS__)
#define xCritical(...) XLogger::getInstance()->critical(__VA_ARGS__)
