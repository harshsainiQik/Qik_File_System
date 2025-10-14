#include "../include/dag_node.h"
#include "../include/logger.h"
#include "../include/utils.h"
#include <nlohmann-json/json.hpp>
#include <sstream>

using json = nlohmann::json;

// RawNode Implementation
RawNode::RawNode(const std::vector<uint8_t>& chunkData, size_t index, const std::string& filetype)
    : DAGNode(DAGNodeType::RAW, filetype), data(chunkData), chunkIndex(index),
      isStored(false), referenceCount(1) {

    try {
        totalSize = chunkData.size();
        checksum = Utils::calculateSHA256(chunkData);
        cid = Utils::generateCID(checksum);
        isPinned = true;  // Default to pinned when created

        // Debug logging removed for performance
    } catch (const std::exception& e) {
        g_logger.error("Error creating RawNode: " + std::string(e.what()));
    }
}

std::string RawNode::toJSON() const {
    try {
        json j;
        j["cid"] = cid;
        j["type"] = "RAW";
        j["file_type"] = fileType;
        j["total_size"] = totalSize;
        j["chunk_index"] = chunkIndex;
        j["checksum"] = checksum;
        j["is_stored"] = isStored;
        j["reference_count"] = referenceCount;
        j["is_pinned"] = isPinned;
        j["create_timestamp"] = createTimestamp;

        return j.dump();
    } catch (const std::exception& e) {
        g_logger.error("Error serializing RawNode to JSON: " + std::string(e.what()));
        return "{}";
    }
}

bool RawNode::fromJSON(const std::string& jsonStr) {
    try {
        json j = json::parse(jsonStr);

        cid = j.value("cid", "");
        fileType = j.value("file_type", "");
        totalSize = j.value("total_size", 0);
        chunkIndex = j.value("chunk_index", 0);
        checksum = j.value("checksum", "");
        isStored = j.value("is_stored", false);
        referenceCount = j.value("reference_count", 1);
        isPinned = j.value("is_pinned", true);
        createTimestamp = j.value("create_timestamp", "");

        return true;
    } catch (const std::exception& e) {
        g_logger.error("Error deserializing RawNode from JSON: " + std::string(e.what()));
        return false;
    }
}

bool RawNode::validateChecksum() const {
    try {
        std::string calculatedChecksum = Utils::calculateSHA256(data);
        return calculatedChecksum == checksum;
    } catch (const std::exception& e) {
        g_logger.error("Error validating RawNode checksum: " + std::string(e.what()));
        return false;
    }
}

// ProtoNode Implementation
ProtoNode::ProtoNode(const std::string& filetype)
    : DAGNode(DAGNodeType::PROTO, filetype), childType(DAGNodeType::RAW), childCount(0) {

    childLinks.reserve(MAX_CHILDREN);
    g_logger.debug("Created ProtoNode with file type: " + filetype);
}

void ProtoNode::addChildLink(const std::string& childCID) {
    try {
        if (childLinks.size() >= MAX_CHILDREN) {
            g_logger.warning("ProtoNode already has maximum number of children (" +
                           std::to_string(MAX_CHILDREN) + ")");
            return;
        }

        childLinks.push_back(childCID);
        childCount = childLinks.size();

        g_logger.debug("Added child link to ProtoNode: " + childCID +
                      " (total children: " + std::to_string(childCount) + ")");
    } catch (const std::exception& e) {
        g_logger.error("Error adding child link to ProtoNode: " + std::string(e.what()));
    }
}

std::string ProtoNode::toJSON() const {
    try {
        json j;
        j["cid"] = cid;
        j["type"] = "PROTO";
        j["file_type"] = fileType;
        j["total_size"] = totalSize;
        j["child_count"] = childCount;
        j["child_type"] = (childType == DAGNodeType::RAW) ? "RAW" : "PROTO";
        j["child_links"] = childLinks;
        j["is_pinned"] = isPinned;
        j["create_timestamp"] = createTimestamp;

        return j.dump();
    } catch (const std::exception& e) {
        g_logger.error("Error serializing ProtoNode to JSON: " + std::string(e.what()));
        return "{}";
    }
}

bool ProtoNode::fromJSON(const std::string& jsonStr) {
    try {
        json j = json::parse(jsonStr);

        cid = j.value("cid", "");
        fileType = j.value("file_type", "");
        totalSize = j.value("total_size", 0);
        childCount = j.value("child_count", 0);

        std::string childTypeStr = j.value("child_type", "RAW");
        childType = (childTypeStr == "PROTO") ? DAGNodeType::PROTO : DAGNodeType::RAW;

        childLinks.clear();
        if (j.contains("child_links") && j["child_links"].is_array()) {
            childLinks = j["child_links"].get<std::vector<std::string>>();
        }

        isPinned = j.value("is_pinned", true);
        createTimestamp = j.value("create_timestamp", "");

        return true;
    } catch (const std::exception& e) {
        g_logger.error("Error deserializing ProtoNode from JSON: " + std::string(e.what()));
        return false;
    }
}

bool ProtoNode::canAddMoreChildren() const {
    return childLinks.size() < MAX_CHILDREN;
}

// RootNode Implementation
RootNode::RootNode(const std::string& filename, const std::string& filetype)
    : DAGNode(DAGNodeType::ROOT, filetype), originalFilename(filename),
      totalChunks(0), protoNodesCount(0) {

    try {
        // Extract file extension
        size_t dotPos = filename.find_last_of('.');
        if (dotPos != std::string::npos) {
            fileExtension = filename.substr(dotPos);
        }

        uploadTimestamp = Utils::getCurrentTimestamp();
        childLinks.reserve(MAX_CHILDREN);

        g_logger.debug("Created RootNode for file: " + filename +
                      ", type: " + filetype + ", extension: " + fileExtension);
    } catch (const std::exception& e) {
        g_logger.error("Error creating RootNode: " + std::string(e.what()));
    }
}

void RootNode::addChildLink(const std::string& childCID) {
    try {
        if (childLinks.size() >= MAX_CHILDREN) {
            g_logger.warning("RootNode already has maximum number of children (" +
                           std::to_string(MAX_CHILDREN) + ")");
            return;
        }

        childLinks.push_back(childCID);

        g_logger.debug("Added child link to RootNode: " + childCID +
                      " (total children: " + std::to_string(childLinks.size()) + ")");
    } catch (const std::exception& e) {
        g_logger.error("Error adding child link to RootNode: " + std::string(e.what()));
    }
}

std::string RootNode::toJSON() const {
    try {
        json j;
        j["cid"] = cid;
        j["type"] = "ROOT";
        j["original_filename"] = originalFilename;
        j["file_type"] = fileType;
        j["file_extension"] = fileExtension;
        j["total_file_size"] = totalSize;
        j["total_chunks"] = totalChunks;
        j["proto_nodes_count"] = protoNodesCount;
        j["upload_timestamp"] = uploadTimestamp;
        j["file_checksum"] = fileChecksum;
        j["child_links"] = childLinks;
        j["is_pinned"] = isPinned;
        j["create_timestamp"] = createTimestamp;

        return j.dump();
    } catch (const std::exception& e) {
        g_logger.error("Error serializing RootNode to JSON: " + std::string(e.what()));
        return "{}";
    }
}

bool RootNode::fromJSON(const std::string& jsonStr) {
    try {
        json j = json::parse(jsonStr);

        cid = j.value("cid", "");
        originalFilename = j.value("original_filename", "");
        fileType = j.value("file_type", "");
        fileExtension = j.value("file_extension", "");
        totalSize = j.value("total_file_size", 0);
        totalChunks = j.value("total_chunks", 0);
        protoNodesCount = j.value("proto_nodes_count", 0);
        uploadTimestamp = j.value("upload_timestamp", "");
        fileChecksum = j.value("file_checksum", "");

        childLinks.clear();
        if (j.contains("child_links") && j["child_links"].is_array()) {
            childLinks = j["child_links"].get<std::vector<std::string>>();
        }

        isPinned = j.value("is_pinned", true);
        createTimestamp = j.value("create_timestamp", "");

        return true;
    } catch (const std::exception& e) {
        g_logger.error("Error deserializing RootNode from JSON: " + std::string(e.what()));
        return false;
    }
}

bool RootNode::canAddMoreChildren() const {
    return childLinks.size() < MAX_CHILDREN;
}