#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <fstream>
#include <mutex>
#include <chrono>
#include <sstream>

enum class LogLevel {
    INFO,
    WARNING,
    ERROR_LEVEL,  // Changed from ERROR to avoid Windows macro conflict
    DEBUG
};

class Logger {
private:
    std::ofstream logFile;
    std::mutex logMutex;
    std::string logFilePath;
    LogLevel currentLevel;

    std::string getCurrentTimestamp();
    std::string levelToString(LogLevel level);

public:
    Logger(const std::string& filename = "data/logs/qfs.log", LogLevel level = LogLevel::INFO);
    ~Logger();

    void log(LogLevel level, const std::string& message);
    void info(const std::string& message);
    void warning(const std::string& message);
    void error(const std::string& message);
    void debug(const std::string& message);

    void setLogLevel(LogLevel level);
    bool isOpen() const;
};

// Global logger instance
extern Logger g_logger;

#endif // LOGGER_H