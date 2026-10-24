#ifndef _LOG_H_
#define _LOG_H_

#include <atomic>
#include <iostream>
#include <memory>

#include "spdlog/spdlog.h"

#define FREEWB_DEBUG(...)                                                                                                        \
    do                                                                                                                           \
    {                                                                                                                            \
        if (auto *logger = FreewbLog::activeLogger())                                                                            \
        {                                                                                                                        \
            SPDLOG_LOGGER_CALL(logger, spdlog::level::debug, __VA_ARGS__);                                                       \
        }                                                                                                                        \
    } while (0)

#define FREEWB_WARN(...)                                                                                                         \
    do                                                                                                                           \
    {                                                                                                                            \
        if (auto *logger = FreewbLog::activeLogger())                                                                            \
        {                                                                                                                        \
            SPDLOG_LOGGER_WARN(logger, __VA_ARGS__);                                                                             \
        }                                                                                                                        \
    } while (0)

#define FREEWB_ERROR(...)                                                                                                        \
    do                                                                                                                           \
    {                                                                                                                            \
        if (auto *logger = FreewbLog::activeLogger())                                                                            \
        {                                                                                                                        \
            SPDLOG_LOGGER_ERROR(logger, __VA_ARGS__);                                                                            \
        }                                                                                                                        \
    } while (0)

class FreewbLog
{
public:
    explicit FreewbLog(const std::string &logFilePath = "/tmp/freewb.log");
    ~FreewbLog();

    class LogOption
    {
    public:
        long long fileSize = 1024 * 1024 * 10;
        int fileCounts = 1;
        bool rotateEnable = false;
    };

    static bool isCleaned();
    static spdlog::logger *activeLogger();

private:
    void init(const LogOption &option);
    void cleanUp();
    spdlog::level::level_enum logLevel() const;

private:
    std::shared_ptr<spdlog::logger> m_logger;
    static std::atomic<bool> s_cleaned;
    static std::atomic<spdlog::logger *> s_activeLogger;
    std::string m_logFilePath;
};

#endif
