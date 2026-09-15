#pragma once

// 禁用 spdlog DLL 接口警告
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4251 4275)
#endif

#include "spdlog/fmt/fmt.h"
#include "spdlog/spdlog.h"

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include <memory>
#include <mutex>
#include <QString>
#include <sstream>
#include <string>
#include <type_traits>

using LogLevel = spdlog::level::level_enum;

class XLogger
{
public:
    static void init(const std::string& logFilePath = "logs/daliy_log.log", LogLevel loglevel = LogLevel::info);

    static std::shared_ptr<spdlog::logger> getInstance();

private:
    static std::shared_ptr<spdlog::logger> createLogger(const std::string& path, LogLevel loglevel = LogLevel::info);

    XLogger() = delete;
    ~XLogger() = delete;
    XLogger(const XLogger&) = delete;
    XLogger(XLogger&&) = delete;
    XLogger& operator=(const XLogger&) = delete;
    XLogger& operator=(XLogger&&) = delete;

private:
    static std::shared_ptr<spdlog::logger> m_logger;
    static std::mutex m_mutex;
    static std::once_flag m_onceFlag;
};

// 辅助命名空间，包含类型转换函数
namespace XLoggerDetail {
    template<typename T>
    inline std::string toFormatString(T&& fmt)
    {
        using RawT = std::decay_t<T>; // 关键：char[N] -> const char*
        if constexpr (std::is_same_v<RawT, QString>) {
            return fmt.toStdString();
        }
        else if constexpr (std::is_same_v<RawT, std::string>) {
            return std::forward<T>(fmt);
        }
        else if constexpr (std::is_same_v<RawT, const char*> || std::is_same_v<RawT, char*>) {
            return fmt ? std::string(fmt) : std::string();
        }
        else {
            static_assert(!sizeof(T), "Unsupported format string type for toFormatString");
        }
    }

    template<typename T>
    inline auto convertArg(T&& arg)
    {
        using RawT = std::remove_cv_t<std::remove_reference_t<T>>;
        if constexpr (std::is_same_v<RawT, QString>) {
            return arg.toStdString();              // 返回 std::string
        }
        else {
            return std::forward<T>(arg);           // 保持原类型
        }
    }

    // 统一日志接口，自动转换格式字符串和参数
    template<typename... Args>
    inline void logTrace(const std::shared_ptr<spdlog::logger>& logger, const auto& fmt, Args&&... args)
    {
        if (!logger) return;
        logger->trace(
            spdlog::fmt_lib::runtime(toFormatString(fmt)),
            convertArg(std::forward<Args>(args))...   // 关键：逐个参数转换
        );
    }

    template<typename... Args>
    inline void logDebug(const std::shared_ptr<spdlog::logger>& logger, const auto& fmt, Args&&... args)
    {
        if (!logger) return;
        logger->debug(
            spdlog::fmt_lib::runtime(toFormatString(fmt)),
            convertArg(std::forward<Args>(args))...   // 关键：逐个参数转换
        );
    }

    template<typename... Args>
    inline void logInfo(const std::shared_ptr<spdlog::logger>& logger, const auto& fmt, Args&&... args)
    {
        if (!logger) return;
        logger->info(
            spdlog::fmt_lib::runtime(toFormatString(fmt)),
            convertArg(std::forward<Args>(args))...   // 关键：逐个参数转换
        );
    }

    template<typename... Args>
    inline void logWarning(const std::shared_ptr<spdlog::logger>& logger, const auto& fmt, Args&&... args)
    {
        if (!logger) return;
        logger->warn(
            spdlog::fmt_lib::runtime(toFormatString(fmt)),
            convertArg(std::forward<Args>(args))...   // 关键：逐个参数转换
        );
    }

    template<typename... Args>
    inline void logError(const std::shared_ptr<spdlog::logger>& logger, const auto& fmt, Args&&... args)
    {
        if (!logger) return;
        logger->error(
            spdlog::fmt_lib::runtime(toFormatString(fmt)),
            convertArg(std::forward<Args>(args))...   // 关键：逐个参数转换
        );
    }

    template<typename... Args>
    inline void logCritical(const std::shared_ptr<spdlog::logger>& logger, const auto& fmt, Args&&... args)
    {
        if (!logger) return;
        logger->critical(
            spdlog::fmt_lib::runtime(toFormatString(fmt)),
            convertArg(std::forward<Args>(args))...   // 关键：逐个参数转换
        );
    }

    // ===== 新增：流式日志辅助类 =====

    // 流式日志构建器
    class LogStreamBuilder
    {
    public:
        LogStreamBuilder(std::shared_ptr<spdlog::logger> logger, spdlog::level::level_enum level)
            : m_logger(logger), m_level(level)
        {}

        ~LogStreamBuilder()
        {
            // 析构时输出日志
            if (m_logger)
            {
                m_logger->log(m_level, m_stream.str());
            }
        }

        // 禁止拷贝
        LogStreamBuilder(const LogStreamBuilder&) = delete;
        LogStreamBuilder& operator=(const LogStreamBuilder&) = delete;

        // 允许移动
        LogStreamBuilder(LogStreamBuilder&&) = default;
        LogStreamBuilder& operator=(LogStreamBuilder&&) = default;

        // 重载 << 操作符，支持各种类型
        template<typename T>
        LogStreamBuilder& operator<<(const T& value)
        {
            m_stream << value;
            return *this;
        }

        // 特化：支持 QString
        LogStreamBuilder& operator<<(const QString& qstr)
        {
            m_stream << qstr.toStdString();
            return *this;
        }

        // 特化：支持 std::endl 等流操纵符
        LogStreamBuilder& operator<<(std::ostream& (*manip)(std::ostream&))
        {
            manip(m_stream);
            return *this;
        }

    private:
        std::shared_ptr<spdlog::logger> m_logger;
        spdlog::level::level_enum m_level;
        std::ostringstream m_stream;
    };
} // namespace XLoggerDetail

// ===== 原有的格式化风格宏 =====

#define xTrace(fmt, ...) \
    do { \
        auto _logger = XLogger::getInstance(); \
        XLoggerDetail::logTrace(_logger, fmt, ##__VA_ARGS__); \
    } while(0)

#define xDebug(fmt, ...) \
    do { \
        auto _logger = XLogger::getInstance(); \
        XLoggerDetail::logDebug(_logger, fmt, ##__VA_ARGS__); \
    } while(0)

#define xInfo(fmt, ...) \
    do { \
        auto _logger = XLogger::getInstance(); \
        XLoggerDetail::logInfo(_logger, fmt, ##__VA_ARGS__); \
    } while(0)

#define xWarning(fmt, ...) \
    do { \
        auto _logger = XLogger::getInstance(); \
        XLoggerDetail::logWarning(_logger, fmt, ##__VA_ARGS__); \
    } while(0)

#define xError(fmt, ...) \
    do { \
        auto _logger = XLogger::getInstance(); \
        XLoggerDetail::logError(_logger, fmt, ##__VA_ARGS__); \
    } while(0)

#define xCritical(fmt, ...) \
    do { \
        auto _logger = XLogger::getInstance(); \
        XLoggerDetail::logCritical(_logger, fmt, ##__VA_ARGS__); \
    } while(0)

// ===== 新增：流式风格宏 =====

#define xTraceS() \
    XLoggerDetail::LogStreamBuilder(XLogger::getInstance(), spdlog::level::trace)

#define xDebugS() \
    XLoggerDetail::LogStreamBuilder(XLogger::getInstance(), spdlog::level::debug)

#define xInfoS() \
    XLoggerDetail::LogStreamBuilder(XLogger::getInstance(), spdlog::level::info)

#define xWarningS() \
    XLoggerDetail::LogStreamBuilder(XLogger::getInstance(), spdlog::level::warn)

#define xErrorS() \
    XLoggerDetail::LogStreamBuilder(XLogger::getInstance(), spdlog::level::err)

#define xCriticalS() \
    XLoggerDetail::LogStreamBuilder(XLogger::getInstance(), spdlog::level::critical)