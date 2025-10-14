#include "../include/async_db_manager.h"
#include "../include/logger.h"
#include <iostream>

// Concrete task implementations
template<typename T>
class DatabaseTaskImpl : public DatabaseTask {
private:
    std::function<T()> operation;
    std::promise<AsyncResult<T>> promise;
    std::chrono::steady_clock::time_point deadline;
    std::string description;

public:
    DatabaseTaskImpl(std::function<T()> op, const std::string& desc, int timeoutMs)
        : operation(std::move(op)),
          deadline(std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs)),
          description(desc) {}

    void execute() override {
        auto start = std::chrono::steady_clock::now();
        try {
            if (isExpired()) {
                AsyncResult<T> result("Task expired before execution");
                promise.set_value(std::move(result));
                return;
            }

            T data = operation();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start);

            AsyncResult<T> result(std::move(data));
            result.duration = duration;
            promise.set_value(std::move(result));

        } catch (const std::exception& e) {
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start);

            AsyncResult<T> result("Database operation failed: " + std::string(e.what()));
            result.duration = duration;
            promise.set_value(std::move(result));
        }
    }

    bool isExpired() const override {
        return std::chrono::steady_clock::now() > deadline;
    }

    std::string getDescription() const override {
        return description;
    }

    std::future<AsyncResult<T>> getFuture() {
        return promise.get_future();
    }
};

AsyncDatabaseManager::AsyncDatabaseManager(std::shared_ptr<StorageManager> storage)
    : storageManager(storage), shutdown(false) {
}

AsyncDatabaseManager::~AsyncDatabaseManager() {
    stop();
}

bool AsyncDatabaseManager::initialize() {
    try {
        int workerCount = CONFIG.getWorkerThreads();
        g_logger.info("Starting AsyncDatabaseManager with " + std::to_string(workerCount) + " worker threads");

        // Start worker threads
        for (int i = 0; i < workerCount; ++i) {
            workerThreads.emplace_back(&AsyncDatabaseManager::workerFunction, this);
        }

        g_logger.info("AsyncDatabaseManager initialized successfully");
        return true;

    } catch (const std::exception& e) {
        g_logger.error("Failed to initialize AsyncDatabaseManager: " + std::string(e.what()));
        return false;
    }
}

void AsyncDatabaseManager::stop() {
    shutdown = true;
    queueCondition.notify_all();

    for (auto& thread : workerThreads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    workerThreads.clear();

    // Clear remaining tasks
    std::lock_guard<std::mutex> lock(queueMutex);
    while (!taskQueue.empty()) {
        taskQueue.pop();
    }
}

void AsyncDatabaseManager::workerFunction() {
    while (!shutdown) {
        std::unique_ptr<DatabaseTask> task;

        {
            std::unique_lock<std::mutex> lock(queueMutex);
            queueCondition.wait(lock, [this] { return !taskQueue.empty() || shutdown; });

            if (shutdown) break;

            if (!taskQueue.empty()) {
                task = std::move(taskQueue.front());
                taskQueue.pop();
            }
        }

        if (task) {
            if (task->isExpired()) {
                g_logger.warning("Skipping expired task: " + task->getDescription());
                continue;
            }

            try {
                task->execute();
            } catch (const std::exception& e) {
                g_logger.error("Task execution failed: " + task->getDescription() + " - " + e.what());
            }
        }
    }
}

std::future<AsyncResult<bool>> AsyncDatabaseManager::asyncNodeExists(const std::string& cid, int timeoutMs) {
    auto task = std::make_unique<DatabaseTaskImpl<bool>>(
        [this, cid]() { return storageManager->nodeExists(cid); },
        "nodeExists(" + cid + ")",
        timeoutMs
    );

    auto future = task->getFuture();

    {
        std::lock_guard<std::mutex> lock(queueMutex);
        taskQueue.push(std::move(task));
    }
    queueCondition.notify_one();

    return future;
}

std::future<AsyncResult<std::shared_ptr<RawNode>>> AsyncDatabaseManager::asyncGetRawNode(const std::string& cid, int timeoutMs) {
    auto task = std::make_unique<DatabaseTaskImpl<std::shared_ptr<RawNode>>>(
        [this, cid]() { return storageManager->getRawNode(cid); },
        "getRawNode(" + cid + ")",
        timeoutMs
    );

    auto future = task->getFuture();

    {
        std::lock_guard<std::mutex> lock(queueMutex);
        taskQueue.push(std::move(task));
    }
    queueCondition.notify_one();

    return future;
}

std::future<AsyncResult<std::shared_ptr<RootNode>>> AsyncDatabaseManager::asyncGetRootNode(const std::string& cid, int timeoutMs) {
    auto task = std::make_unique<DatabaseTaskImpl<std::shared_ptr<RootNode>>>(
        [this, cid]() { return storageManager->getRootNode(cid); },
        "getRootNode(" + cid + ")",
        timeoutMs
    );

    auto future = task->getFuture();

    {
        std::lock_guard<std::mutex> lock(queueMutex);
        taskQueue.push(std::move(task));
    }
    queueCondition.notify_one();

    return future;
}

std::future<AsyncResult<std::shared_ptr<ProtoNode>>> AsyncDatabaseManager::asyncGetProtoNode(const std::string& cid, int timeoutMs) {
    auto task = std::make_unique<DatabaseTaskImpl<std::shared_ptr<ProtoNode>>>(
        [this, cid]() { return storageManager->getProtoNode(cid); },
        "getProtoNode(" + cid + ")",
        timeoutMs
    );

    auto future = task->getFuture();

    {
        std::lock_guard<std::mutex> lock(queueMutex);
        taskQueue.push(std::move(task));
    }
    queueCondition.notify_one();

    return future;
}

std::future<AsyncResult<StorageManager::FileStoreResult>> AsyncDatabaseManager::asyncUpdateFile(const std::string& oldCid, const std::string& filename, const std::vector<uint8_t>& fileData, int timeoutMs) {
    auto task = std::make_unique<DatabaseTaskImpl<StorageManager::FileStoreResult>>(
        [this, oldCid, filename, fileData]() { return storageManager->updateFile(oldCid, filename, fileData); },
        "updateFile(" + oldCid + ", " + filename + ")",
        timeoutMs
    );

    auto future = task->getFuture();

    {
        std::lock_guard<std::mutex> lock(queueMutex);
        taskQueue.push(std::move(task));
    }
    queueCondition.notify_one();

    return future;
}

std::future<AsyncResult<bool>> AsyncDatabaseManager::asyncDeleteFile(const std::string& cid, int timeoutMs) {
    auto task = std::make_unique<DatabaseTaskImpl<bool>>(
        [this, cid]() { return storageManager->deleteFile(cid); },
        "deleteFile(" + cid + ")",
        timeoutMs
    );

    auto future = task->getFuture();

    {
        std::lock_guard<std::mutex> lock(queueMutex);
        taskQueue.push(std::move(task));
    }
    queueCondition.notify_one();

    return future;
}

bool AsyncDatabaseManager::isHealthy() const {
    std::lock_guard<std::mutex> lock(queueMutex);
    return !shutdown && taskQueue.size() < 1000; // Reasonable queue limit
}

size_t AsyncDatabaseManager::getQueueSize() const {
    std::lock_guard<std::mutex> lock(queueMutex);
    return taskQueue.size();
}