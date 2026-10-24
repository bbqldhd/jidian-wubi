#include "log.h"

#include <unistd.h>

#include <cstdlib>
#include <mutex>
#include <system_error>
#include <vector>

#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

std::atomic<bool> FreewbLog::s_cleaned{false};
std::atomic<spdlog::logger *> FreewbLog::s_activeLogger{nullptr};

FreewbLog::FreewbLog(const std::string &logFilePath) : m_logFilePath(logFilePath)
{
    init(LogOption());
}

FreewbLog::~FreewbLog()
{
    cleanUp();
}

void FreewbLog::init(const LogOption &option)
{
    spdlog::level::level_enum level = logLevel();

    std::vector<spdlog::sink_ptr> sinks;
    spdlog::sink_ptr console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    sinks.push_back(console_sink);

    try
    {
#ifdef SPDLOG_HAS_ROTATE_ON_OPEN
        spdlog::sink_ptr file_sink =
            std::make_shared<spdlog::sinks::rotating_file_sink_mt>(m_logFilePath, option.fileSize, option.fileCounts,
                                                                   option.rotateEnable);
#else
        spdlog::sink_ptr file_sink =
            std::make_shared<spdlog::sinks::rotating_file_sink_mt>(m_logFilePath, option.fileSize, option.fileCounts);
#endif
        sinks.push_back(file_sink);
    }
    catch (const spdlog::spdlog_ex &e)
    {
        std::cerr << "Failed to create log file: " << m_logFilePath << " Error: " << e.what()
                  << " Falling back to console output only." << std::endl;
        m_logger = nullptr;
        return;
    }

    m_logger = std::make_shared<spdlog::logger>("freewb", sinks.begin(), sinks.end());
    m_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%s:%!:%#] %v");
    m_logger->set_level(level);
    m_logger->flush_on(level);

    s_activeLogger.store(m_logger.get());
    s_cleaned.store(false);
}

void FreewbLog::cleanUp()
{
    static std::mutex cleanup_mutex;
    std::lock_guard<std::mutex> lock(cleanup_mutex);
    if (m_logger == nullptr)
    {
        return;
    }

    s_cleaned.store(true);
    try
    {
        m_logger->flush();
        m_logger.reset();
        s_activeLogger.store(nullptr);
    }
    catch (const std::system_error &e)
    {
        std::cerr << "Error during log cleanup (system): " << e.what() << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error during log cleanup: " << e.what() << std::endl;
    }
    catch (...)
    {
        std::cerr << "Unknown error during log cleanup" << std::endl;
    }
}

bool FreewbLog::isCleaned()
{
    return s_cleaned.load() == true;
}

spdlog::logger *FreewbLog::activeLogger()
{
    if (s_cleaned.load())
    {
        return nullptr;
    }
    return s_activeLogger.load();
}

spdlog::level::level_enum FreewbLog::logLevel() const
{
    const char *home = std::getenv("HOME");
    if (!home || !home[0])
    {
        return spdlog::level::warn;
    }
    const std::string path = std::string(home) + "/.local/freewb/enable_debug.txt";
    if (access(path.c_str(), F_OK) == 0)
    {
        return spdlog::level::debug;
    }

    return spdlog::level::warn;
}
