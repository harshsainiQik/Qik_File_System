#ifndef STORAGE_MANAGER_H
#define STORAGE_MANAGER_H

#include "dag_node.h"
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <leveldb/db.h>

class StorageManager {
private:
    std::string dataDirectory;
    std::string blocksDirectory;
    std::string leveldbDirectory;
    leveldb::DB* database;
    std::mutex storageMutex;

    // Internal helper methods
    std::string getBlockPath(const std::string& cid, size_t chunkIndex);
    std::string getShardDirectory(const std::string& cid);
    bool initializeDirectories();
    bool initializeLevelDB();
    bool initializeEmergencyMode();
    void decrementOldFileReferences(const std::string& oldCid);

    // Emergency mode state
    bool emergencyMode;
    std::unordered_map<std::string, std::string> emergencyMetadata;

public:
    StorageManager(const std::string& dataDir = "data");
    ~StorageManager();

    // Initialization
    bool initialize();
    void cleanup();

    // DAG Node operations
    bool storeRootNode(const RootNode& rootNode);
    bool storeProtoNode(const ProtoNode& protoNode);
    bool storeRawNode(const RawNode& rawNode);

    std::unique_ptr<RootNode> getRootNode(const std::string& cid);
    std::unique_ptr<ProtoNode> getProtoNode(const std::string& cid);
    std::unique_ptr<RawNode> getRawNode(const std::string& cid);

    bool deleteRootNode(const std::string& cid);
    bool deleteProtoNode(const std::string& cid);
    bool deleteRawNode(const std::string& cid);

    // Block storage operations
    bool storeChunkData(const std::string& cid, size_t chunkIndex,
                       const std::vector<uint8_t>& data);
    std::vector<uint8_t> getChunkData(const std::string& cid, size_t chunkIndex);
    bool deleteChunkData(const std::string& cid, size_t chunkIndex);

    // File operations
    struct FileStoreResult {
        bool success;
        std::string cid;
        size_t chunksCount;
    };

    FileStoreResult storeFile(const std::string& filename, const std::vector<uint8_t>& fileData);
    FileStoreResult updateFile(const std::string& oldCid, const std::string& filename, const std::vector<uint8_t>& fileData);
    std::vector<uint8_t> retrieveFile(const std::string& cid);
    bool deleteFile(const std::string& cid);

    // Utility functions
    bool nodeExists(const std::string& cid);
    DAGNodeType getNodeType(const std::string& cid);
    std::vector<std::string> listAllFiles();
    size_t getTotalStorageUsed();

    // Statistics
    struct StorageStats {
        size_t totalFiles;
        size_t totalChunks;
        size_t totalSizeBytes;
        size_t metadataEntries;
    };

    StorageStats getStorageStats();

    // Validation and integrity
    bool validateFileIntegrity(const std::string& cid);
    std::vector<std::string> findCorruptedChunks();
    bool repairChunk(const std::string& cid, size_t chunkIndex);
};

#endif // STORAGE_MANAGER_H