#ifndef DAG_NODE_H
#define DAG_NODE_H

#include <string>
#include <vector>
#include <memory>
#include <cstdint>
#include "utils.h"

enum class DAGNodeType {
    ROOT,
    PROTO,
    RAW
};

// Base class for all DAG nodes
class DAGNode {
protected:
    std::string cid;
    DAGNodeType nodeType;
    std::string fileType;
    size_t totalSize;
    bool isPinned;              // Pin flag for garbage collection
    std::string createTimestamp; // Creation timestamp

public:
    DAGNode(DAGNodeType type, const std::string& filetype = "")
        : nodeType(type), fileType(filetype), totalSize(0), isPinned(false) {
        createTimestamp = Utils::getCurrentTimestamp();
    }

    virtual ~DAGNode() = default;

    // Getters
    std::string getCID() const { return cid; }
    DAGNodeType getType() const { return nodeType; }
    std::string getFileType() const { return fileType; }
    size_t getTotalSize() const { return totalSize; }
    bool getIsPinned() const { return isPinned; }
    std::string getCreateTimestamp() const { return createTimestamp; }

    // Setters
    void setCID(const std::string& newCID) { cid = newCID; }
    void setTotalSize(size_t size) { totalSize = size; }
    void setIsPinned(bool pinned) { isPinned = pinned; }
    void setCreateTimestamp(const std::string& timestamp) { createTimestamp = timestamp; }

    // Virtual methods
    virtual std::string toJSON() const = 0;
    virtual bool fromJSON(const std::string& json) = 0;
};

// RawNode - contains actual chunk data
class RawNode : public DAGNode {
private:
    std::vector<uint8_t> data;
    size_t chunkIndex;
    std::string checksum;
    bool isStored;
    size_t referenceCount;      // Reference count for deduplication

public:
    RawNode(const std::vector<uint8_t>& chunkData, size_t index,
            const std::string& filetype = "");

    // Data access
    std::vector<uint8_t> getData() const { return data; }
    void setData(const std::vector<uint8_t>& newData) { data = newData; }

    size_t getChunkIndex() const { return chunkIndex; }
    std::string getChecksum() const { return checksum; }
    bool getIsStored() const { return isStored; }
    size_t getReferenceCount() const { return referenceCount; }

    void setIsStored(bool stored) { isStored = stored; }
    void setChecksum(const std::string& hash) { checksum = hash; }
    void setReferenceCount(size_t count) { referenceCount = count; }

    // Reference counting methods
    void incrementReference() { referenceCount++; }
    void decrementReference() { if (referenceCount > 0) referenceCount--; }
    bool canBeDeleted() const { return referenceCount == 0 && !isPinned; }

    // Serialization
    std::string toJSON() const override;
    bool fromJSON(const std::string& json) override;

    // Validation
    bool validateChecksum() const;
};

// ProtoNode - contains links to child nodes
class ProtoNode : public DAGNode {
private:
    std::vector<std::string> childLinks;
    DAGNodeType childType;
    size_t childCount;

public:
    ProtoNode(const std::string& filetype = "");

    // Child management
    void addChildLink(const std::string& childCID);
    std::vector<std::string> getChildLinks() const { return childLinks; }
    size_t getChildCount() const { return childCount; }
    DAGNodeType getChildType() const { return childType; }

    void setChildType(DAGNodeType type) { childType = type; }

    // Serialization
    std::string toJSON() const override;
    bool fromJSON(const std::string& json) override;

    // Validation
    bool canAddMoreChildren() const;
    static const size_t MAX_CHILDREN = 160;
};

// RootNode - represents the complete file
class RootNode : public DAGNode {
private:
    std::vector<std::string> childLinks;
    std::string originalFilename;
    std::string fileExtension;
    size_t totalChunks;
    size_t protoNodesCount;
    std::string uploadTimestamp;
    std::string fileChecksum;

public:
    RootNode(const std::string& filename, const std::string& filetype = "");

    // Child management
    void addChildLink(const std::string& childCID);
    std::vector<std::string> getChildLinks() const { return childLinks; }

    // File metadata
    std::string getOriginalFilename() const { return originalFilename; }
    std::string getFileExtension() const { return fileExtension; }
    size_t getTotalChunks() const { return totalChunks; }
    size_t getProtoNodesCount() const { return protoNodesCount; }
    std::string getUploadTimestamp() const { return uploadTimestamp; }
    std::string getFileChecksum() const { return fileChecksum; }

    // Setters
    void setTotalChunks(size_t chunks) { totalChunks = chunks; }
    void setProtoNodesCount(size_t count) { protoNodesCount = count; }
    void setUploadTimestamp(const std::string& timestamp) { uploadTimestamp = timestamp; }
    void setFileChecksum(const std::string& checksum) { fileChecksum = checksum; }

    // Serialization
    std::string toJSON() const override;
    bool fromJSON(const std::string& json) override;

    // Validation
    bool canAddMoreChildren() const;
    static const size_t MAX_CHILDREN = 160;
};

#endif // DAG_NODE_H