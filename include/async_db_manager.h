#ifndef ASYNC_DB_MANAGER_H
#define ASYNC_DB_MANAGER_H

#include <future>
#include <memory>
#include <functional>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include "storage_manager.h"
#include "config_manager.h"

// Task result wrapper
template<typename T>
struct AsyncResult {
    bool success;
    T data;
    std::string error_message;
    std::chrono::milliseconds duration;

    AsyncResult() : success(false), duration(0) {}
    AsyncResult(T&& d) : success(true), data(std::move(d)), duration(0) {}
    AsyncResult(const std::string& error) : success(false), error_message(error), duration(0) {}
};

// Database task wrapper
class DatabaseTask {
public:
    virtual ~DatabaseTask() = default;
    virtual void execute() = 0;
    virtual bool isExpired() const = 0;
    virtual std::string getDescription() const = 0;
};

// Async Database Manager for non-blocking operations
class AsyncDatabaseManager {
private:
    std::shared_ptr<StorageManager> storageManager;
    std::queue<std::unique_ptr<DatabaseTask>> taskQueue;
    std::vector<std::thread> workerThreads;
    mutable std::mutex queueMutex;
    std::condition_variable queueCondition;
    std::atomic<bool> shutdown;

    // Worker thread function
    void workerFunction();

public:
    AsyncDatabaseManager(std::shared_ptr<StorageManager> storage);
    ~AsyncDatabaseManager();

    // Initialize async manager
    bool initialize();
    void stop();

    // Async database operations with timeout protection
    std::future<AsyncResult<bool>> asyncNodeExists(const std::string& cid, int timeoutMs = 5000);
    std::future<AsyncResult<std::shared_ptr<RawNode>>> asyncGetRawNode(const std::string& cid, int timeoutMs = 10000);
    std::future<AsyncResult<std::shared_ptr<RootNode>>> asyncGetRootNode(const std::string& cid, int timeoutMs = 10000);
    std::future<AsyncResult<std::shared_ptr<ProtoNode>>> asyncGetProtoNode(const std::string& cid, int timeoutMs = 10000);
    std::future<AsyncResult<StorageManager::FileStoreResult>> asyncUpdateFile(const std::string& oldCid, const std::string& filename, const std::vector<uint8_t>& fileData, int timeoutMs = 600000);
    std::future<AsyncResult<bool>> asyncDeleteFile(const std::string& cid, int timeoutMs = 300000);

    // Health check
    bool isHealthy() const;
    size_t getQueueSize() const;
};

#endif // ASYNC_DB_MANAGER_H