#include "../include/logger.h"
#include <iostream>
#include <iomanip>

Logger g_logger;

Logger::Logger(const std::string& filename, LogLevel level)
    : logFilePath(filename), currentLevel(level) {
    try {
        logFile.open(logFilePath, std::ios::app);
        if (!logFile.is_open()) {
            std::cerr << "Failed to open log file: " << logFilePath << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Logger initialization error: " << e.what() << std::endl;
    }
}

Logger::~Logger() {
    try {
        if (logFile.is_open()) {
            logFile.close();
        }
    } catch (...) {
        // Ignore exceptions in destructor
    }
}

std::string Logger::getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}

std::string Logger::levelToString(LogLevel level) {
    switch (level) {
        case LogLevel::INFO: return "INFO";
        case LogLevel::WARNING: return "WARN";
        case LogLevel::ERROR_LEVEL: return "ERROR";
        case LogLevel::DEBUG: return "DEBUG";
        default: return "UNKNOWN";
    }
}

void Logger::log(LogLevel level, const std::string& message) {
    if (level < currentLevel) {
        return;
    }

    std::lock_guard<std::mutex> lock(logMutex);

    try {
        std::string logEntry = "[" + getCurrentTimestamp() + "] [" +
                              levelToString(level) + "] " + message;

        if (logFile.is_open()) {
            logFile << logEntry << std::endl;
            logFile.flush();
        }

        // Also output to console for errors and warnings
        if (level == LogLevel::ERROR_LEVEL || level == LogLevel::WARNING) {
            std::cerr << logEntry << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Logging error: " << e.what() << std::endl;
    }
}

void Logger::info(const std::string& message) {
    log(LogLevel::INFO, message);
}

void Logger::warning(const std::string& message) {
    log(LogLevel::WARNING, message);
}

void Logger::error(const std::string& message) {
    log(LogLevel::ERROR_LEVEL, message);
}

void Logger::debug(const std::string& message) {
    log(LogLevel::DEBUG, message);
}

void Logger::setLogLevel(LogLevel level) {
    currentLevel = level;
}

bool Logger::isOpen() const {
    return logFile.is_open();
}