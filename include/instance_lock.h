#ifndef INSTANCE_LOCK_H
#define INSTANCE_LOCK_H

#include <string>
#include <fstream>
#include <memory>

#ifdef _WIN32
#include <windows.h>
#include <processthreadsapi.h>
#else
#include <sys/file.h>
#include <unistd.h>
#include <signal.h>
#endif

class InstanceLock {
private:
    std::string lockFilePath;
    std::string pidFilePath;

#ifdef _WIN32
    HANDLE lockFileHandle;
    HANDLE pidFileHandle;
#else
    int lockFileHandle;
    std::unique_ptr<std::ofstream> pidFile;
#endif

    bool isLocked;

    // Helper methods
    bool createLockFiles();
    bool writePidFile();
    bool checkExistingProcess();
    void cleanupLockFiles();
    bool isProcessRunning(int pid);

public:
    InstanceLock(const std::string& dataDirectory);
    ~InstanceLock();

    // Main interface
    bool acquireLock();
    void releaseLock();
    bool isLockAcquired() const { return isLocked; }

    // Process information
    int getCurrentPid();
    int getLockedPid();

    // Utilities
    static std::string getCurrentTimestamp();
    static void killProcess(int pid);
};

#endif // INSTANCE_LOCK_H