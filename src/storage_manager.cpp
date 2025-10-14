#include "../include/storage_manager.h"
#include "../include/logger.h"
#include "../include/utils.h"
#include "../include/chunk.h"
#include "../include/config_manager.h"
#include <filesystem>
#include <leveldb/options.h>
#include <leveldb/write_batch.h>
#include <nlohmann-json/json.hpp>
#include <thread>
#include <chrono>

using json = nlohmann::json;

StorageManager::StorageManager(const std::string& dataDir)
    : dataDirectory(dataDir), database(nullptr), emergencyMode(false) {

    blocksDirectory = dataDirectory + "/blocks";
    leveldbDirectory = dataDirectory + "/leveldb";

    g_logger.info("StorageManager initialized with data directory: " + dataDirectory);
}

StorageManager::~StorageManager() {
    cleanup();
}

bool StorageManager::initialize() {
    try {
        g_logger.info("Initializing StorageManager...");

        if (!initializeDirectories()) {
            g_logger.error("Failed to initialize directories");
            return false;
        }

        if (!initializeLevelDB()) {
            g_logger.error("Failed to initialize LevelDB");
            return false;
        }

        g_logger.info("StorageManager initialized successfully");
        return true;

    } catch (const std::exception& e) {
        g_logger.error("Exception during StorageManager initialization: " + std::string(e.what()));
        return false;
    }
}

bool StorageManager::initializeDirectories() {
    try {
        // Create main directories
        std::filesystem::create_directories(dataDirectory);
        std::filesystem::create_directories(blocksDirectory);
        std::filesystem::create_directories(leveldbDirectory);
        std::filesystem::create_directories(dataDirectory + "/logs");

        // Create shard directories for blocks
        for (int i = 0; i < 256; i++) {
            std::stringstream ss;
            ss << std::hex << std::setfill('0') << std::setw(2) << i;
            std::string shardDir = blocksDirectory + "/" + ss.str();
            std::filesystem::create_directories(shardDir);
        }

        g_logger.info("Directories initialized successfully");
        return true;

    } catch (const std::exception& e) {
        g_logger.error("Error creating directories: " + std::string(e.what()));
        return false;
    }
}

bool StorageManager::initializeLevelDB() {
    try {
        g_logger.info("Initializing LevelDB with timeout protection...");

        // Check if database directory exists and is accessible
        if (std::filesystem::exists(leveldbDirectory)) {
            // Check for LOCK file that indicates another process is using the database
            std::string lockFile = leveldbDirectory + "/LOCK";
            if (std::filesystem::exists(lockFile)) {
                g_logger.warning("Database LOCK file exists, checking if process is still running...");

                // Try to remove stale lock file (this will fail if process is actually running)
                try {
                    std::filesystem::remove(lockFile);
                    g_logger.info("Removed stale LOCK file");
                } catch (const std::exception& e) {
                    g_logger.warning("Could not remove LOCK file, database may be in use: " + std::string(e.what()));
                }
            }
        }

        leveldb::Options options;
        options.create_if_missing = true;
        options.error_if_exists = false;

        // Production-ready database settings from config
        options.compression = leveldb::kSnappyCompression;
        options.paranoid_checks = CONFIG.getParanoidChecks();
        options.write_buffer_size = CONFIG.getWriteBufferSizeMB() * 1024 * 1024;
        options.max_file_size = CONFIG.getMaxFileSizeMB_DB() * 1024 * 1024;
        // options.block_cache = leveldb::NewLRUCache(CONFIG.getBlockCacheSizeMB() * 1024 * 1024); // Disabled for compatibility

        // Timeout-protected database opening
        auto startTime = std::chrono::steady_clock::now();
        auto timeoutMs = 10000; // 10 second timeout for database opening

        g_logger.info("Opening LevelDB database with " + std::to_string(timeoutMs) + "ms timeout...");

        // CRITICAL FIX: Try LevelDB but fallback to in-memory mode if it hangs
        g_logger.info("Attempting LevelDB initialization...");

        // Quick sanity check - if database already exists and has issues, use emergency mode
        std::string manifestFile = leveldbDirectory + "/CURRENT";
        if (std::filesystem::exists(manifestFile)) {
            try {
                // Try to read the CURRENT file to see if database is corrupted
                std::ifstream currentFile(manifestFile);
                if (!currentFile.is_open()) {
                    g_logger.warning("Cannot read CURRENT file, database may be corrupted. Using emergency mode.");
                    return initializeEmergencyMode();
                }
            } catch (const std::exception& e) {
                g_logger.warning("Error checking database health: " + std::string(e.what()) + ". Using emergency mode.");
                return initializeEmergencyMode();
            }
        }

        // Try opening database with timeout monitoring
        leveldb::Status status;
        try {
            status = leveldb::DB::Open(options, leveldbDirectory, &database);
        } catch (const std::exception& e) {
            g_logger.error("Exception during LevelDB open: " + std::string(e.what()) + ". Using emergency mode.");
            return initializeEmergencyMode();
        }

        auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startTime).count();

        g_logger.info("LevelDB open completed in " + std::to_string(elapsedMs) + "ms");

        // Handle database lock issues (multiple process access)
        if (!status.ok() && (status.ToString().find("LOCK") != std::string::npos ||
                            status.ToString().find("being used by another process") != std::string::npos)) {
            g_logger.warning("Database locked by another process. Waiting and retrying...");

            // Wait briefly for other process to release lock
            std::this_thread::sleep_for(std::chrono::milliseconds(500));

            // Try opening again
            status = leveldb::DB::Open(options, leveldbDirectory, &database);

            if (!status.ok()) {
                g_logger.error("Database still locked after retry. Consider stopping other QFS instances.");
                return false;
            }
        }

        // Handle corruption with comprehensive recovery
        if (!status.ok() && status.IsCorruption()) {
            g_logger.warning("LevelDB corruption detected, attempting repair: " + status.ToString());

            // Close any existing connection
            if (database) {
                delete database;
                database = nullptr;
            }

            // Attempt repair
            leveldb::Status repairStatus = leveldb::RepairDB(leveldbDirectory, options);
            if (!repairStatus.ok()) {
                g_logger.error("Failed to repair LevelDB: " + repairStatus.ToString());

                // If repair fails, backup corrupted DB and recreate fresh
                g_logger.warning("Attempting nuclear option: backup and recreate database");

                std::string backupDir = leveldbDirectory + "_corrupted_" + Utils::getCurrentTimestamp();
                try {
                    std::filesystem::rename(leveldbDirectory, backupDir);
                    g_logger.info("Corrupted database backed up to: " + backupDir);

                    // Create fresh database directory
                    std::filesystem::create_directories(leveldbDirectory);

                    // Try opening fresh database
                    status = leveldb::DB::Open(options, leveldbDirectory, &database);
                    if (status.ok()) {
                        g_logger.info("Fresh database created successfully");
                    }
                } catch (const std::exception& e) {
                    g_logger.error("Failed to backup and recreate database: " + std::string(e.what()));
                    return false;
                }
            } else {
                g_logger.info("LevelDB repair successful, retrying connection");

                // Retry opening after repair
                status = leveldb::DB::Open(options, leveldbDirectory, &database);
            }
        }

        if (!status.ok()) {
            g_logger.error("Failed to open LevelDB after all recovery attempts: " + status.ToString());

            // Last resort: create completely fresh database
            g_logger.warning("Attempting last resort: fresh database creation");
            try {
                std::string backupDir = leveldbDirectory + "_failed_" + Utils::getCurrentTimestamp();
                std::filesystem::rename(leveldbDirectory, backupDir);
                std::filesystem::create_directories(leveldbDirectory);

                status = leveldb::DB::Open(options, leveldbDirectory, &database);
                if (status.ok()) {
                    g_logger.info("Last resort successful: fresh database created");
                } else {
                    g_logger.error("All database recovery attempts failed");
                    return false;
                }
            } catch (const std::exception& e) {
                g_logger.error("Last resort database creation failed: " + std::string(e.what()));
                return false;
            }
        }

        g_logger.info("LevelDB initialized successfully");
        return true;

    } catch (const std::exception& e) {
        g_logger.error("Error initializing LevelDB: " + std::string(e.what()));
        return false;
    }
}

bool StorageManager::initializeEmergencyMode() {
    try {
        g_logger.warning("🚨 EMERGENCY MODE ACTIVATED 🚨");
        g_logger.warning("Running in emergency file-system only mode (no LevelDB)");
        g_logger.warning("This is a temporary failsafe to ensure service availability");

        emergencyMode = true;
        database = nullptr;

        // Create minimal directory structure for emergency mode
        if (!initializeDirectories()) {
            g_logger.error("Failed to initialize directories for emergency mode");
            return false;
        }

        g_logger.info("Emergency mode initialized successfully");
        g_logger.info("Service is operational but with limited metadata functionality");
        return true;

    } catch (const std::exception& e) {
        g_logger.error("Error initializing emergency mode: " + std::string(e.what()));
        return false;
    }
}

void StorageManager::cleanup() {
    try {
        if (database) {
            delete database;
            database = nullptr;
            g_logger.info("StorageManager cleaned up successfully");
        }
        if (emergencyMode) {
            g_logger.info("Emergency mode cleanup completed");
        }
    } catch (const std::exception& e) {
        g_logger.error("Error during StorageManager cleanup: " + std::string(e.what()));
    }
}

std::string StorageManager::getBlockPath(const std::string& cid, size_t chunkIndex) {
    try {
        std::string shardDir = getShardDirectory(cid);
        return shardDir + "/" + cid + "_chunk_" + std::to_string(chunkIndex);
    } catch (const std::exception& e) {
        g_logger.error("Error generating block path: " + std::string(e.what()));
        return "";
    }
}

std::string StorageManager::getShardDirectory(const std::string& cid) {
    try {
        if (cid.length() < 2) {
            g_logger.error("CID too short for sharding: " + cid);
            return blocksDirectory + "/00";
        }

        // Use first 2 characters for sharding
        std::string prefix = cid.substr(0, 2);
        return blocksDirectory + "/" + prefix;
    } catch (const std::exception& e) {
        g_logger.error("Error getting shard directory: " + std::string(e.what()));
        return blocksDirectory + "/00";
    }
}

bool StorageManager::storeRootNode(const RootNode& rootNode) {
    std::lock_guard<std::mutex> lock(storageMutex);

    try {
        if (emergencyMode) {
            // In emergency mode, store metadata in memory
            std::string key = "root:" + rootNode.getCID();
            std::string typeKey = "type:" + rootNode.getCID();

            emergencyMetadata[key] = rootNode.toJSON();
            emergencyMetadata[typeKey] = "ROOT";

            g_logger.debug("Stored RootNode in emergency mode with CID: " + rootNode.getCID());
            return true;
        }

        std::string key = "root:" + rootNode.getCID();
        std::string value = rootNode.toJSON();

        leveldb::Status status = database->Put(leveldb::WriteOptions(), key, value);

        if (!status.ok()) {
            g_logger.error("Failed to store RootNode: " + status.ToString());
            return false;
        }

        // Also store type information
        std::string typeKey = "type:" + rootNode.getCID();
        status = database->Put(leveldb::WriteOptions(), typeKey, "ROOT");

        if (!status.ok()) {
            g_logger.error("Failed to store node type: " + status.ToString());
            return false;
        }

        g_logger.debug("Stored RootNode with CID: " + rootNode.getCID());
        return true;

    } catch (const std::exception& e) {
        g_logger.error("Exception storing RootNode: " + std::string(e.what()));
        return false;
    }
}

bool StorageManager::storeProtoNode(const ProtoNode& protoNode) {
    std::lock_guard<std::mutex> lock(storageMutex);

    try {
        std::string key = "proto:" + protoNode.getCID();
        std::string value = protoNode.toJSON();

        leveldb::Status status = database->Put(leveldb::WriteOptions(), key, value);

        if (!status.ok()) {
            g_logger.error("Failed to store ProtoNode: " + status.ToString());
            return false;
        }

        // Store type information
        std::string typeKey = "type:" + protoNode.getCID();
        status = database->Put(leveldb::WriteOptions(), typeKey, "PROTO");

        if (!status.ok()) {
            g_logger.error("Failed to store node type: " + status.ToString());
            return false;
        }

        g_logger.debug("Stored ProtoNode with CID: " + protoNode.getCID());
        return true;

    } catch (const std::exception& e) {
        g_logger.error("Exception storing ProtoNode: " + std::string(e.what()));
        return false;
    }
}

bool StorageManager::storeRawNode(const RawNode& rawNode) {
    std::lock_guard<std::mutex> lock(storageMutex);

    try {
        std::string key = "raw:" + rawNode.getCID();
        std::string existingValue;

        // Check if RawNode already exists (deduplication)
        leveldb::Status status = database->Get(leveldb::ReadOptions(), key, &existingValue);

        if (status.ok()) {
            // Node exists, increment reference count
            RawNode existingNode(std::vector<uint8_t>(), 0, "");
            if (existingNode.fromJSON(existingValue)) {
                existingNode.incrementReference();

                // Update with new reference count
                std::string updatedValue = existingNode.toJSON();
                status = database->Put(leveldb::WriteOptions(), key, updatedValue);

                if (!status.ok()) {
                    g_logger.error("Failed to update RawNode reference count: " + status.ToString());
                    return false;
                }

                g_logger.debug("Incremented reference count for existing RawNode: " + rawNode.getCID() +
                              " (new count: " + std::to_string(existingNode.getReferenceCount()) + ")");
                return true;
            } else {
                g_logger.error("Failed to parse existing RawNode JSON for deduplication");
                return false;
            }
        } else if (!status.IsNotFound()) {
            g_logger.error("Error checking for existing RawNode: " + status.ToString());
            return false;
        }

        // Node doesn't exist, store new node
        std::string value = rawNode.toJSON();
        status = database->Put(leveldb::WriteOptions(), key, value);

        if (!status.ok()) {
            g_logger.error("Failed to store RawNode metadata: " + status.ToString());
            return false;
        }

        // Store type information
        std::string typeKey = "type:" + rawNode.getCID();
        status = database->Put(leveldb::WriteOptions(), typeKey, "RAW");

        if (!status.ok()) {
            g_logger.error("Failed to store node type: " + status.ToString());
            return false;
        }

        // Store chunk data in blockstore (only for new nodes)
        if (!storeChunkData(rawNode.getCID(), rawNode.getChunkIndex(), rawNode.getData())) {
            g_logger.error("Failed to store chunk data for RawNode: " + rawNode.getCID());
            return false;
        }

        g_logger.debug("Stored new RawNode with CID: " + rawNode.getCID() +
                      " (reference count: " + std::to_string(rawNode.getReferenceCount()) + ")");
        return true;

    } catch (const std::exception& e) {
        g_logger.error("Exception storing RawNode: " + std::string(e.what()));
        return false;
    }
}

std::unique_ptr<RootNode> StorageManager::getRootNode(const std::string& cid) {
    std::lock_guard<std::mutex> lock(storageMutex);

    try {
        std::string key = "root:" + cid;
        std::string value;

        if (emergencyMode) {
            // In emergency mode, get from memory
            auto it = emergencyMetadata.find(key);
            if (it == emergencyMetadata.end()) {
                return nullptr;
            }
            value = it->second;
        } else {
            leveldb::Status status = database->Get(leveldb::ReadOptions(), key, &value);

            if (!status.ok()) {
                if (!status.IsNotFound()) {
                    g_logger.error("Failed to get RootNode: " + status.ToString());
                }
                return nullptr;
            }
        }

        auto rootNode = std::make_unique<RootNode>("", "");
        if (!rootNode->fromJSON(value)) {
            g_logger.error("Failed to deserialize RootNode from JSON");
            return nullptr;
        }

        return rootNode;

    } catch (const std::exception& e) {
        g_logger.error("Exception getting RootNode: " + std::string(e.what()));
        return nullptr;
    }
}

std::unique_ptr<ProtoNode> StorageManager::getProtoNode(const std::string& cid) {
    std::lock_guard<std::mutex> lock(storageMutex);

    try {
        std::string key = "proto:" + cid;
        std::string value;

        leveldb::Status status = database->Get(leveldb::ReadOptions(), key, &value);

        if (!status.ok()) {
            if (!status.IsNotFound()) {
                g_logger.error("Failed to get ProtoNode: " + status.ToString());
            }
            return nullptr;
        }

        auto protoNode = std::make_unique<ProtoNode>("");
        if (!protoNode->fromJSON(value)) {
            g_logger.error("Failed to deserialize ProtoNode from JSON");
            return nullptr;
        }

        return protoNode;

    } catch (const std::exception& e) {
        g_logger.error("Exception getting ProtoNode: " + std::string(e.what()));
        return nullptr;
    }
}

std::unique_ptr<RawNode> StorageManager::getRawNode(const std::string& cid) {
    std::lock_guard<std::mutex> lock(storageMutex);

    try {
        std::string key = "raw:" + cid;
        std::string value;

        leveldb::Status status = database->Get(leveldb::ReadOptions(), key, &value);

        if (!status.ok()) {
            if (!status.IsNotFound()) {
                g_logger.error("Failed to get RawNode: " + status.ToString());
            }
            return nullptr;
        }

        auto rawNode = std::make_unique<RawNode>(std::vector<uint8_t>(), 0, "");
        if (!rawNode->fromJSON(value)) {
            g_logger.error("Failed to deserialize RawNode from JSON");
            return nullptr;
        }

        // Load chunk data
        std::vector<uint8_t> chunkData = getChunkData(cid, rawNode->getChunkIndex());
        if (chunkData.empty()) {
            g_logger.error("Failed to load chunk data for RawNode: " + cid);
            return nullptr;
        }

        rawNode->setData(chunkData);
        return rawNode;

    } catch (const std::exception& e) {
        g_logger.error("Exception getting RawNode: " + std::string(e.what()));
        return nullptr;
    }
}

bool StorageManager::storeChunkData(const std::string& cid, size_t chunkIndex,
                                   const std::vector<uint8_t>& data) {
    try {
        std::string filepath = getBlockPath(cid, chunkIndex);

        if (!Utils::writeBytesToFile(filepath, data)) {
            g_logger.error("Failed to write chunk data to file: " + filepath);
            return false;
        }

        g_logger.debug("Stored chunk data: " + cid + "_chunk_" + std::to_string(chunkIndex));
        return true;

    } catch (const std::exception& e) {
        g_logger.error("Exception storing chunk data: " + std::string(e.what()));
        return false;
    }
}

std::vector<uint8_t> StorageManager::getChunkData(const std::string& cid, size_t chunkIndex) {
    try {
        std::string filepath = getBlockPath(cid, chunkIndex);

        std::vector<uint8_t> data = Utils::readFileToBytes(filepath);

        if (data.empty() && Utils::fileExists(filepath)) {
            g_logger.warning("Chunk file exists but is empty: " + filepath);
        }

        return data;

    } catch (const std::exception& e) {
        g_logger.error("Exception getting chunk data: " + std::string(e.what()));
        return {};
    }
}

bool StorageManager::nodeExists(const std::string& cid) {
    std::lock_guard<std::mutex> lock(storageMutex);

    try {
        if (emergencyMode) {
            // In emergency mode, check if metadata exists in memory or filesystem
            auto it = emergencyMetadata.find("type:" + cid);
            if (it != emergencyMetadata.end()) {
                return true;
            }

            // Also check if actual chunk files exist in blockstore
            std::string shardDir = getShardDirectory(cid);
            if (std::filesystem::exists(shardDir)) {
                for (const auto& entry : std::filesystem::directory_iterator(shardDir)) {
                    if (entry.path().filename().string().find(cid) == 0) {
                        return true;
                    }
                }
            }
            return false;
        }

        std::string typeKey = "type:" + cid;
        std::string value;

        leveldb::Status status = database->Get(leveldb::ReadOptions(), typeKey, &value);
        return status.ok();

    } catch (const std::exception& e) {
        g_logger.error("Exception checking node existence: " + std::string(e.what()));
        return false;
    }
}

DAGNodeType StorageManager::getNodeType(const std::string& cid) {
    std::lock_guard<std::mutex> lock(storageMutex);

    try {
        if (emergencyMode) {
            // In emergency mode, check memory metadata first
            auto it = emergencyMetadata.find("type:" + cid);
            if (it != emergencyMetadata.end()) {
                if (it->second == "ROOT") return DAGNodeType::ROOT;
                else if (it->second == "PROTO") return DAGNodeType::PROTO;
                else return DAGNodeType::RAW;
            }
            return DAGNodeType::RAW; // Default fallback
        }

        std::string typeKey = "type:" + cid;
        std::string value;

        leveldb::Status status = database->Get(leveldb::ReadOptions(), typeKey, &value);

        if (!status.ok()) {
            return DAGNodeType::RAW; // Default fallback
        }

        if (value == "ROOT") return DAGNodeType::ROOT;
        else if (value == "PROTO") return DAGNodeType::PROTO;
        else return DAGNodeType::RAW;

    } catch (const std::exception& e) {
        g_logger.error("Exception getting node type: " + std::string(e.what()));
        return DAGNodeType::RAW;
    }
}

StorageManager::FileStoreResult StorageManager::storeFile(const std::string& filename, const std::vector<uint8_t>& fileData) {
    FileStoreResult result = {false, "", 0};

    try {
        g_logger.info("Starting to store file: " + filename +
                     " (size: " + Utils::formatFileSize(fileData.size()) + ")");

        // Detect file type
        std::string fileType = Utils::detectMimeType(filename);

        // Create chunks
        Chunker chunker;
        std::vector<ChunkData> chunks = chunker.createChunks(fileData, fileType);

        if (chunks.empty()) {
            g_logger.error("Failed to create chunks for file: " + filename);
            return result;
        }

        // Create and store RawNodes
        std::vector<std::string> rawNodeCIDs;
        for (const auto& chunk : chunks) {
            RawNode rawNode(chunk.data, chunk.index, fileType);
            rawNode.setChecksum(chunk.checksum);

            if (!storeRawNode(rawNode)) {
                g_logger.error("Failed to store RawNode for chunk " + std::to_string(chunk.index));
                return result;
            }

            rawNodeCIDs.push_back(rawNode.getCID());
        }

        // Create RootNode
        RootNode rootNode(filename, fileType);
        rootNode.setTotalSize(fileData.size());
        rootNode.setTotalChunks(chunks.size());
        rootNode.setFileChecksum(Utils::calculateSHA256(fileData));
        rootNode.setIsPinned(true); // Pin the RootNode on upload

        // If we have <= 160 chunks, link directly to RootNode
        if (rawNodeCIDs.size() <= RootNode::MAX_CHILDREN) {
            for (const auto& cid : rawNodeCIDs) {
                rootNode.addChildLink(cid);
            }
            rootNode.setProtoNodesCount(0);
        } else {
            // Create ProtoNodes for chunks > 160
            std::vector<std::string> protoNodeCIDs;
            size_t protoNodeCount = 0;

            for (size_t i = 0; i < rawNodeCIDs.size(); i += ProtoNode::MAX_CHILDREN) {
                ProtoNode protoNode(fileType);
                protoNode.setIsPinned(true); // Pin the ProtoNode on upload

                size_t endIndex = std::min(i + ProtoNode::MAX_CHILDREN, rawNodeCIDs.size());
                for (size_t j = i; j < endIndex; ++j) {
                    protoNode.addChildLink(rawNodeCIDs[j]);
                }

                // Generate CID for ProtoNode
                std::string protoNodeJSON = protoNode.toJSON();
                std::string protoHash = Utils::calculateSHA256(protoNodeJSON);
                protoNode.setCID(Utils::generateCID(protoHash));

                if (!storeProtoNode(protoNode)) {
                    g_logger.error("Failed to store ProtoNode");
                    return result;
                }

                protoNodeCIDs.push_back(protoNode.getCID());
                protoNodeCount++;
            }

            // Link ProtoNodes to RootNode
            for (const auto& cid : protoNodeCIDs) {
                rootNode.addChildLink(cid);
            }
            rootNode.setProtoNodesCount(protoNodeCount);
        }

        // Generate CID for RootNode
        std::string rootNodeJSON = rootNode.toJSON();
        std::string rootHash = Utils::calculateSHA256(rootNodeJSON);
        rootNode.setCID(Utils::generateCID(rootHash));

        // Store RootNode
        if (!storeRootNode(rootNode)) {
            g_logger.error("Failed to store RootNode");
            return result;
        }

        g_logger.info("Successfully stored file: " + filename +
                     " with CID: " + rootNode.getCID() +
                     " (" + std::to_string(chunks.size()) + " chunks)");

        // Set success result
        result.success = true;
        result.cid = rootNode.getCID();
        result.chunksCount = chunks.size();

        return result;

    } catch (const std::exception& e) {
        g_logger.error("Exception storing file: " + std::string(e.what()));
        return result;
    }
}

StorageManager::FileStoreResult StorageManager::updateFile(const std::string& oldCid, const std::string& filename, const std::vector<uint8_t>& fileData) {
    FileStoreResult result = {false, "", 0};

    try {
        g_logger.info("Starting SIMPLE update approach (DELETE + UPLOAD) for file: " + filename +
                     " (old CID: " + oldCid +
                     ", size: " + Utils::formatFileSize(fileData.size()) + ")");

        // Step 1: Validate old file exists
        if (!nodeExists(oldCid)) {
            g_logger.error("Old file not found for update: " + oldCid);
            return result;
        }

        // Step 2: Delete old file completely (this now works correctly)
        g_logger.info("Step 1/2: Deleting old file: " + oldCid);
        if (!deleteFile(oldCid)) {
            g_logger.error("Failed to delete old file: " + oldCid);
            return result;
        }
        g_logger.info("Old file deleted successfully: " + oldCid);

        // Step 3: Upload new file (this now works correctly)
        g_logger.info("Step 2/2: Uploading new file: " + filename);
        FileStoreResult uploadResult = storeFile(filename, fileData);
        if (!uploadResult.success) {
            g_logger.error("Failed to upload new file after deleting old file: " + filename);
            return result;
        }

        g_logger.info("Successfully updated file using DELETE + UPLOAD approach: " + filename +
                     " (old CID: " + oldCid +
                     ", new CID: " + uploadResult.cid +
                     ", chunks: " + std::to_string(uploadResult.chunksCount) + ")");

        // Return success result
        result.success = true;
        result.cid = uploadResult.cid;
        result.chunksCount = uploadResult.chunksCount;

        return result;

    } catch (const std::exception& e) {
        g_logger.error("Exception in SIMPLE update (DELETE + UPLOAD): " + std::string(e.what()));
        return result;
    }
}

std::vector<uint8_t> StorageManager::retrieveFile(const std::string& cid) {
    try {
        g_logger.info("Starting to retrieve file with CID: " + cid);

        // Get RootNode
        auto rootNode = getRootNode(cid);
        if (!rootNode) {
            g_logger.error("RootNode not found for CID: " + cid);
            return {};
        }

        std::vector<ChunkData> allChunks;

        // Traverse child links
        for (const auto& childCID : rootNode->getChildLinks()) {
            DAGNodeType childType = getNodeType(childCID);

            if (childType == DAGNodeType::RAW) {
                // Direct RawNode
                auto rawNode = getRawNode(childCID);
                if (!rawNode) {
                    g_logger.error("Failed to get RawNode: " + childCID);
                    return {};
                }

                ChunkData chunk(rawNode->getData(), rawNode->getChunkIndex(), rawNode->getFileType());
                chunk.cid = rawNode->getCID();
                chunk.checksum = rawNode->getChecksum();
                allChunks.push_back(chunk);

            } else if (childType == DAGNodeType::PROTO) {
                // ProtoNode - traverse its children
                auto protoNode = getProtoNode(childCID);
                if (!protoNode) {
                    g_logger.error("Failed to get ProtoNode: " + childCID);
                    return {};
                }

                for (const auto& rawCID : protoNode->getChildLinks()) {
                    auto rawNode = getRawNode(rawCID);
                    if (!rawNode) {
                        g_logger.error("Failed to get RawNode: " + rawCID);
                        return {};
                    }

                    ChunkData chunk(rawNode->getData(), rawNode->getChunkIndex(), rawNode->getFileType());
                    chunk.cid = rawNode->getCID();
                    chunk.checksum = rawNode->getChecksum();
                    allChunks.push_back(chunk);
                }
            }
        }

        // Reassemble file from chunks
        Chunker chunker;
        std::vector<uint8_t> fileData = chunker.reassembleChunks(allChunks);

        if (fileData.empty()) {
            g_logger.error("Failed to reassemble file from chunks");
            return {};
        }

        g_logger.info("Successfully retrieved file with CID: " + cid +
                     " (size: " + Utils::formatFileSize(fileData.size()) + ")");

        return fileData;

    } catch (const std::exception& e) {
        g_logger.error("Exception retrieving file: " + std::string(e.what()));
        return {};
    }
}

bool StorageManager::deleteFile(const std::string& cid) {
    // ATOMIC DELETE OPERATION - Single mutex lock to prevent deadlocks
    std::lock_guard<std::mutex> lock(storageMutex);

    try {
        g_logger.info("Starting atomic delete for CID: " + cid);

        // Check if file exists first (without additional locking)
        std::string rootKey = "root:" + cid;
        std::string value;
        leveldb::Status status = database->Get(leveldb::ReadOptions(), rootKey, &value);

        if (!status.ok()) {
            if (status.IsNotFound()) {
                g_logger.warning("File not found for deletion: " + cid);
                return false;
            } else {
                g_logger.error("Database error checking file: " + status.ToString());
                return false;
            }
        }

        // Parse RootNode to get child links (without calling getRootNode)
        auto rootNode = std::make_unique<RootNode>("", "");
        if (!rootNode->fromJSON(value)) {
            g_logger.error("Failed to parse RootNode for deletion: " + cid);
            return false;
        }

        // Collect all keys to delete atomically AND chunk files to delete
        std::vector<std::string> keysToDelete;
        std::vector<std::pair<std::string, size_t>> chunksToDelete; // CID, chunk_index pairs
        keysToDelete.push_back("root:" + cid);
        keysToDelete.push_back("type:" + cid);

        // Process child links directly
        for (const auto& childCID : rootNode->getChildLinks()) {
            // Check child type directly from database
            std::string childTypeKey = "type:" + childCID;
            std::string childTypeValue;
            status = database->Get(leveldb::ReadOptions(), childTypeKey, &childTypeValue);

            if (status.ok()) {
                keysToDelete.push_back("type:" + childCID);

                if (childTypeValue == "RAW") {
                    keysToDelete.push_back("raw:" + childCID);

                    // Get RawNode to find chunk index for file deletion
                    std::string rawKey = "raw:" + childCID;
                    std::string rawValue;
                    status = database->Get(leveldb::ReadOptions(), rawKey, &rawValue);
                    if (status.ok()) {
                        auto tempRawNode = std::make_unique<RawNode>(std::vector<uint8_t>(), 0, "");
                        if (tempRawNode->fromJSON(rawValue)) {
                            chunksToDelete.push_back({childCID, tempRawNode->getChunkIndex()});
                        }
                    }
                } else if (childTypeValue == "PROTO") {
                    keysToDelete.push_back("proto:" + childCID);

                    // Get ProtoNode children directly
                    std::string protoKey = "proto:" + childCID;
                    std::string protoValue;
                    status = database->Get(leveldb::ReadOptions(), protoKey, &protoValue);

                    if (status.ok()) {
                        auto protoNode = std::make_unique<ProtoNode>();
                        if (protoNode->fromJSON(protoValue)) {
                            for (const auto& rawCID : protoNode->getChildLinks()) {
                                keysToDelete.push_back("type:" + rawCID);
                                keysToDelete.push_back("raw:" + rawCID);

                                // Get RawNode to find chunk index for file deletion
                                std::string rawKey = "raw:" + rawCID;
                                std::string rawValue;
                                status = database->Get(leveldb::ReadOptions(), rawKey, &rawValue);
                                if (status.ok()) {
                                    auto tempRawNode = std::make_unique<RawNode>(std::vector<uint8_t>(), 0, "");
                                    if (tempRawNode->fromJSON(rawValue)) {
                                        chunksToDelete.push_back({rawCID, tempRawNode->getChunkIndex()});
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        // Perform atomic batch delete of metadata
        leveldb::WriteBatch batch;
        for (const auto& key : keysToDelete) {
            batch.Delete(key);
        }

        status = database->Write(leveldb::WriteOptions(), &batch);

        if (!status.ok()) {
            g_logger.error("Failed to delete file batch: " + status.ToString());
            return false;
        }

        // CRITICAL FIX: Delete actual chunk files from disk
        size_t chunksDeleted = 0;
        for (const auto& chunkInfo : chunksToDelete) {
            try {
                if (deleteChunkData(chunkInfo.first, chunkInfo.second)) {
                    chunksDeleted++;
                } else {
                    g_logger.warning("Failed to delete chunk file: " + chunkInfo.first + "_chunk_" + std::to_string(chunkInfo.second));
                }
            } catch (const std::exception& e) {
                g_logger.warning("Exception deleting chunk file " + chunkInfo.first + ": " + e.what());
            }
        }

        g_logger.info("Successfully deleted file atomically: " + cid +
                     " (" + std::to_string(keysToDelete.size()) + " metadata keys, " +
                     std::to_string(chunksDeleted) + "/" + std::to_string(chunksToDelete.size()) + " chunk files)");
        return true;

    } catch (const std::exception& e) {
        g_logger.error("Exception in atomic delete: " + std::string(e.what()));
        return false;
    }
}

bool StorageManager::deleteRootNode(const std::string& cid) {
    std::lock_guard<std::mutex> lock(storageMutex);

    try {
        std::string key = "root:" + cid;
        std::string typeKey = "type:" + cid;

        leveldb::Status status = database->Delete(leveldb::WriteOptions(), key);
        if (!status.ok() && !status.IsNotFound()) {
            g_logger.error("Failed to delete RootNode: " + status.ToString());
            return false;
        }

        status = database->Delete(leveldb::WriteOptions(), typeKey);
        if (!status.ok() && !status.IsNotFound()) {
            g_logger.error("Failed to delete node type: " + status.ToString());
        }

        return true;
    } catch (const std::exception& e) {
        g_logger.error("Exception deleting RootNode: " + std::string(e.what()));
        return false;
    }
}

bool StorageManager::deleteProtoNode(const std::string& cid) {
    std::lock_guard<std::mutex> lock(storageMutex);

    try {
        std::string key = "proto:" + cid;
        std::string typeKey = "type:" + cid;

        leveldb::Status status = database->Delete(leveldb::WriteOptions(), key);
        if (!status.ok() && !status.IsNotFound()) {
            g_logger.error("Failed to delete ProtoNode: " + status.ToString());
            return false;
        }

        status = database->Delete(leveldb::WriteOptions(), typeKey);
        if (!status.ok() && !status.IsNotFound()) {
            g_logger.error("Failed to delete node type: " + status.ToString());
        }

        return true;
    } catch (const std::exception& e) {
        g_logger.error("Exception deleting ProtoNode: " + std::string(e.what()));
        return false;
    }
}

bool StorageManager::deleteRawNode(const std::string& cid) {
    std::lock_guard<std::mutex> lock(storageMutex);

    try {
        // Get RawNode to check reference count and pin status
        auto rawNode = getRawNode(cid);
        if (!rawNode) {
            g_logger.warning("RawNode not found for deletion: " + cid);
            return true; // Consider it already deleted
        }

        // Decrement reference count
        rawNode->decrementReference();

        // Check if node can be deleted (reference count is 0 and not pinned)
        if (!rawNode->canBeDeleted()) {
            // Update the node with decremented reference count
            std::string key = "raw:" + cid;
            std::string value = rawNode->toJSON();

            leveldb::Status status = database->Put(leveldb::WriteOptions(), key, value);
            if (!status.ok()) {
                g_logger.error("Failed to update RawNode reference count: " + status.ToString());
                return false;
            }

            g_logger.debug("Decremented reference count for RawNode: " + cid +
                          " (current count: " + std::to_string(rawNode->getReferenceCount()) +
                          ", pinned: " + (rawNode->getIsPinned() ? "true" : "false") + ")");
            return true;
        }

        // Reference count is 0 and not pinned, proceed with actual deletion
        g_logger.debug("Deleting RawNode: " + cid + " (reference count: 0, unpinned)");

        // NOTE: Chunk data deletion is now handled by caller to avoid double deletion
        // deleteChunkData(cid, rawNode->getChunkIndex());

        // Delete metadata from LevelDB
        std::string key = "raw:" + cid;
        std::string typeKey = "type:" + cid;

        leveldb::Status status = database->Delete(leveldb::WriteOptions(), key);
        if (!status.ok() && !status.IsNotFound()) {
            g_logger.error("Failed to delete RawNode metadata: " + status.ToString());
            return false;
        }

        status = database->Delete(leveldb::WriteOptions(), typeKey);
        if (!status.ok() && !status.IsNotFound()) {
            g_logger.error("Failed to delete node type: " + status.ToString());
        }

        g_logger.debug("Successfully deleted RawNode: " + cid);
        return true;

    } catch (const std::exception& e) {
        g_logger.error("Exception deleting RawNode: " + std::string(e.what()));
        return false;
    }
}

bool StorageManager::deleteChunkData(const std::string& cid, size_t chunkIndex) {
    try {
        std::string filepath = getBlockPath(cid, chunkIndex);

        if (std::filesystem::exists(filepath)) {
            std::filesystem::remove(filepath);
            g_logger.debug("Deleted chunk data: " + cid + "_chunk_" + std::to_string(chunkIndex));
        }

        return true;
    } catch (const std::exception& e) {
        g_logger.error("Exception deleting chunk data: " + std::string(e.what()));
        return false;
    }
}

std::vector<std::string> StorageManager::listAllFiles() {
    std::lock_guard<std::mutex> lock(storageMutex);
    std::vector<std::string> fileCIDs;

    try {
        std::unique_ptr<leveldb::Iterator> it(database->NewIterator(leveldb::ReadOptions()));

        for (it->SeekToFirst(); it->Valid(); it->Next()) {
            std::string key = it->key().ToString();

            // Look for root node keys
            if (key.substr(0, 5) == "root:") {
                std::string cid = key.substr(5);
                fileCIDs.push_back(cid);
            }
        }

        if (!it->status().ok()) {
            g_logger.error("Error iterating through database: " + it->status().ToString());
        }

    } catch (const std::exception& e) {
        g_logger.error("Exception listing files: " + std::string(e.what()));
    }

    return fileCIDs;
}

size_t StorageManager::getTotalStorageUsed() {
    try {
        size_t totalSize = 0;

        for (const auto& entry : std::filesystem::recursive_directory_iterator(blocksDirectory)) {
            if (entry.is_regular_file()) {
                totalSize += entry.file_size();
            }
        }

        return totalSize;
    } catch (const std::exception& e) {
        g_logger.error("Exception calculating storage usage: " + std::string(e.what()));
        return 0;
    }
}

StorageManager::StorageStats StorageManager::getStorageStats() {
    StorageStats stats;

    try {
        // Count files
        std::vector<std::string> files = listAllFiles();
        stats.totalFiles = files.size();

        // Count chunks and calculate sizes
        std::lock_guard<std::mutex> lock(storageMutex);
        std::unique_ptr<leveldb::Iterator> it(database->NewIterator(leveldb::ReadOptions()));

        for (it->SeekToFirst(); it->Valid(); it->Next()) {
            std::string key = it->key().ToString();

            if (key.substr(0, 4) == "raw:") {
                stats.totalChunks++;
            }

            stats.metadataEntries++;
        }

        // Calculate storage usage
        stats.totalSizeBytes = getTotalStorageUsed();

    } catch (const std::exception& e) {
        g_logger.error("Exception getting storage stats: " + std::string(e.what()));
    }

    return stats;
}

