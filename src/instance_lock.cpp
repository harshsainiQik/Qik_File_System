#include "../include/instance_lock.h"
#include "../include/logger.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>
#include <chrono>
#include <iomanip>

#ifdef _WIN32
#include <tlhelp32.h>
#include <psapi.h>
#else
#include <sys/stat.h>
#include <csignal>
#include <sys/types.h>
#include <dirent.h>
#endif

InstanceLock::InstanceLock(const std::string& dataDirectory)
    : isLocked(false) {

    lockFilePath = dataDirectory + "/qfs.lock";
    pidFilePath = dataDirectory + "/qfs.pid";

#ifdef _WIN32
    lockFileHandle = INVALID_HANDLE_VALUE;
    pidFileHandle = INVALID_HANDLE_VALUE;
#else
    lockFileHandle = -1;
#endif

    g_logger.info("InstanceLock initialized for directory: " + dataDirectory);
}

InstanceLock::~InstanceLock() {
    releaseLock();
}

bool InstanceLock::acquireLock() {
    try {
        g_logger.info("Attempting to acquire instance lock...");

        // Check if there's an existing process running
        if (std::filesystem::exists(pidFilePath)) {
            g_logger.info("Found existing PID file, checking if process is running");

            if (checkExistingProcess()) {
                g_logger.error("Another QFS instance is already running!");
                return false;
            } else {
                g_logger.info("Stale lock files found, cleaning up...");
                cleanupLockFiles();
            }
        }

        // Create and acquire lock
        if (!createLockFiles()) {
            g_logger.error("Failed to create lock files");
            return false;
        }

        if (!writePidFile()) {
            g_logger.error("Failed to write PID file");
            cleanupLockFiles();
            return false;
        }

        isLocked = true;
        g_logger.info("Instance lock acquired successfully with PID: " + std::to_string(getCurrentPid()));
        return true;

    } catch (const std::exception& e) {
        g_logger.error("Exception while acquiring lock: " + std::string(e.what()));
        return false;
    }
}

void InstanceLock::releaseLock() {
    if (!isLocked) return;

    try {
        g_logger.info("Releasing instance lock...");

#ifdef _WIN32
        if (lockFileHandle != INVALID_HANDLE_VALUE) {
            CloseHandle(lockFileHandle);
            lockFileHandle = INVALID_HANDLE_VALUE;
        }
        if (pidFileHandle != INVALID_HANDLE_VALUE) {
            CloseHandle(pidFileHandle);
            pidFileHandle = INVALID_HANDLE_VALUE;
        }
#else
        if (lockFileHandle >= 0) {
            close(lockFileHandle);
            lockFileHandle = -1;
        }
        pidFile.reset();
#endif

        cleanupLockFiles();
        isLocked = false;
        g_logger.info("Instance lock released successfully");

    } catch (const std::exception& e) {
        g_logger.error("Exception while releasing lock: " + std::string(e.what()));
    }
}

bool InstanceLock::createLockFiles() {
    try {
#ifdef _WIN32
        // Create lock file with exclusive access on Windows
        lockFileHandle = CreateFileA(
            lockFilePath.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            0, // No sharing - exclusive access
            NULL,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE,
            NULL
        );

        if (lockFileHandle == INVALID_HANDLE_VALUE) {
            DWORD error = GetLastError();
            g_logger.error("Failed to create lock file. Error code: " + std::to_string(error));
            return false;
        }

        // Write lock identifier
        std::string lockContent = "QFS_LOCK_" + std::to_string(getCurrentPid()) + "_" + getCurrentTimestamp();
        DWORD written;
        if (!WriteFile(lockFileHandle, lockContent.c_str(), lockContent.length(), &written, NULL)) {
            g_logger.error("Failed to write to lock file");
            return false;
        }

#else
        // Unix file locking with flock
        lockFileHandle = open(lockFilePath.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0644);
        if (lockFileHandle < 0) {
            g_logger.error("Failed to create lock file: " + std::string(strerror(errno)));
            return false;
        }

        // Acquire exclusive lock
        if (flock(lockFileHandle, LOCK_EX | LOCK_NB) < 0) {
            g_logger.error("Failed to acquire file lock: " + std::string(strerror(errno)));
            close(lockFileHandle);
            lockFileHandle = -1;
            return false;
        }

        // Write lock content
        std::string lockContent = "QFS_LOCK_" + std::to_string(getCurrentPid()) + "_" + getCurrentTimestamp();
        if (write(lockFileHandle, lockContent.c_str(), lockContent.length()) < 0) {
            g_logger.error("Failed to write to lock file");
            return false;
        }
#endif

        g_logger.info("Lock file created successfully: " + lockFilePath);
        return true;

    } catch (const std::exception& e) {
        g_logger.error("Exception creating lock files: " + std::string(e.what()));
        return false;
    }
}

bool InstanceLock::writePidFile() {
    try {
        int currentPid = getCurrentPid();

#ifdef _WIN32
        // Create PID file with exclusive access
        pidFileHandle = CreateFileA(
            pidFilePath.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ, // Allow reading but not writing
            NULL,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            NULL
        );

        if (pidFileHandle == INVALID_HANDLE_VALUE) {
            g_logger.error("Failed to create PID file");
            return false;
        }

        std::string pidContent = std::to_string(currentPid);
        DWORD written;
        if (!WriteFile(pidFileHandle, pidContent.c_str(), pidContent.length(), &written, NULL)) {
            g_logger.error("Failed to write PID to file");
            return false;
        }
#else
        pidFile = std::make_unique<std::ofstream>(pidFilePath);
        if (!pidFile->is_open()) {
            g_logger.error("Failed to create PID file");
            return false;
        }
        *pidFile << currentPid << std::flush;
#endif

        g_logger.info("PID file written: " + pidFilePath + " with PID: " + std::to_string(currentPid));
        return true;

    } catch (const std::exception& e) {
        g_logger.error("Exception writing PID file: " + std::string(e.what()));
        return false;
    }
}

bool InstanceLock::checkExistingProcess() {
    try {
        int existingPid = getLockedPid();
        if (existingPid <= 0) {
            g_logger.info("Invalid PID in lock file");
            return false;
        }

        g_logger.info("Checking if process PID " + std::to_string(existingPid) + " is running");

        bool processExists = isProcessRunning(existingPid);
        if (processExists) {
            g_logger.error("Process PID " + std::to_string(existingPid) + " is still running!");
            return true;
        } else {
            g_logger.info("Process PID " + std::to_string(existingPid) + " is not running (stale lock)");
            return false;
        }

    } catch (const std::exception& e) {
        g_logger.error("Exception checking existing process: " + std::string(e.what()));
        return false;
    }
}

void InstanceLock::cleanupLockFiles() {
    try {
        if (std::filesystem::exists(lockFilePath)) {
            std::filesystem::remove(lockFilePath);
            g_logger.info("Cleaned up lock file: " + lockFilePath);
        }

        if (std::filesystem::exists(pidFilePath)) {
            std::filesystem::remove(pidFilePath);
            g_logger.info("Cleaned up PID file: " + pidFilePath);
        }
    } catch (const std::exception& e) {
        g_logger.warning("Failed to cleanup lock files: " + std::string(e.what()));
    }
}

bool InstanceLock::isProcessRunning(int pid) {
    if (pid <= 0) return false;

    try {
#ifdef _WIN32
        HANDLE processHandle = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
        if (processHandle == NULL) {
            // Process doesn't exist or access denied
            return false;
        }

        // Check if process is still running
        DWORD exitCode;
        bool result = false;
        if (GetExitCodeProcess(processHandle, &exitCode)) {
            result = (exitCode == STILL_ACTIVE);
        }

        // Additional check: try to get process name to ensure it's actually QFS
        if (result) {
            char processName[MAX_PATH];
            HMODULE hMod;
            DWORD cbNeeded;

            if (EnumProcessModules(processHandle, &hMod, sizeof(hMod), &cbNeeded)) {
                if (GetModuleBaseNameA(processHandle, hMod, processName, sizeof(processName))) {
                    std::string name = processName;
                    // Check if it's actually a QFS process
                    if (name.find("qfs") != std::string::npos) {
                        g_logger.info("Confirmed QFS process running: " + name);
                    } else {
                        g_logger.info("PID exists but not QFS process: " + name);
                        result = false; // PID reused by different process
                    }
                }
            }
        }

        CloseHandle(processHandle);
        return result;
#else
        // Unix: send signal 0 to check if process exists
        if (kill(pid, 0) == 0) {
            // Process exists, try to check if it's QFS by reading /proc/pid/comm
            std::string commPath = "/proc/" + std::to_string(pid) + "/comm";
            std::ifstream commFile(commPath);
            if (commFile.is_open()) {
                std::string processName;
                std::getline(commFile, processName);
                if (processName.find("qfs") != std::string::npos) {
                    g_logger.info("Confirmed QFS process running: " + processName);
                    return true;
                } else {
                    g_logger.info("PID exists but not QFS process: " + processName);
                    return false;
                }
            }
            return true; // Process exists but can't determine name
        } else {
            return false; // Process doesn't exist
        }
#endif
    } catch (const std::exception& e) {
        g_logger.error("Exception checking process: " + std::string(e.what()));
        return false;
    }
}

int InstanceLock::getCurrentPid() {
#ifdef _WIN32
    return static_cast<int>(GetCurrentProcessId());
#else
    return static_cast<int>(getpid());
#endif
}

int InstanceLock::getLockedPid() {
    try {
        std::ifstream pidFile(pidFilePath);
        if (!pidFile.is_open()) {
            return -1;
        }

        int pid;
        pidFile >> pid;
        return pid;
    } catch (const std::exception& e) {
        g_logger.error("Error reading PID file: " + std::string(e.what()));
        return -1;
    }
}

std::string InstanceLock::getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);

    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

void InstanceLock::killProcess(int pid) {
    if (pid <= 0) return;

    try {
#ifdef _WIN32
        HANDLE processHandle = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
        if (processHandle != NULL) {
            TerminateProcess(processHandle, 1);
            CloseHandle(processHandle);
            g_logger.info("Terminated process PID: " + std::to_string(pid));
        }
#else
        if (kill(pid, SIGTERM) == 0) {
            g_logger.info("Sent SIGTERM to process PID: " + std::to_string(pid));
        }
#endif
    } catch (const std::exception& e) {
        g_logger.error("Error killing process: " + std::string(e.what()));
    }
}