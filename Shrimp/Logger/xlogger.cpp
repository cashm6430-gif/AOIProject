#include "xlogger.h"

#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/daily_file_sink.h"
#include "spdlog/sinks/dist_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/spdlog.h"

std::shared_ptr<spdlog::logger> XLogger::m_logger = nullptr;
std::mutex XLogger::m_mutex;
std::once_flag XLogger::m_onceFlag;

void XLogger::init(const std::string& logFilePath, LogLevel loglevel)
{
    std::call_once(m_onceFlag, [&]() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_logger) {
            m_logger = createLogger(logFilePath, loglevel);
        }
        });
}

std::shared_ptr<spdlog::logger> XLogger::getInstance()
{
    if (!m_logger) {
        init();
    }

    return m_logger;
}

std::shared_ptr<spdlog::logger> XLogger::createLogger(const std::string& path, LogLevel loglevel)
{
    auto dist_sink = std::make_shared<spdlog::sinks::dist_sink_mt>();

    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_pattern("[%H:%M:%S] [%^%l%$.%e] %v");

    auto file_sink = std::make_shared<spdlog::sinks::daily_file_sink_mt>(path, 0, 0);
    file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");

    dist_sink->add_sink(console_sink);
    dist_sink->add_sink(file_sink);

    auto logger = std::make_shared<spdlog::logger>("multi_sink_logger", dist_sink);
    spdlog::register_logger(logger);

    logger->set_level(loglevel);
    logger->flush_on(spdlog::level::info);

    return logger;
}