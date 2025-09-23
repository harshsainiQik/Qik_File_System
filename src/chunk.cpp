#include "../include/chunk.h"
#include "../include/logger.h"
#include "../include/utils.h"
#include <algorithm>

Chunker::Chunker() {
    chunkSize = CONFIG.getChunkSizeBytes();
    g_logger.info("Chunker initialized with chunk size: " + std::to_string(chunkSize) + " bytes (from config)");
}

std::vector<ChunkData> Chunker::createChunks(const std::vector<uint8_t>& fileData,
                                            const std::string& fileType) {
    std::vector<ChunkData> chunks;

    try {
        g_logger.info("Starting to create chunks for file of size: " +
                     std::to_string(fileData.size()) + " bytes");

        if (fileData.empty()) {
            g_logger.warning("Empty file data provided to chunker");
            return chunks;
        }

        size_t totalSize = fileData.size();
        size_t chunkIndex = 0;

        for (size_t offset = 0; offset < totalSize; offset += chunkSize) {
            size_t currentChunkSize = std::min(chunkSize, totalSize - offset);

            // Create chunk data
            std::vector<uint8_t> chunkData(fileData.begin() + offset,
                                          fileData.begin() + offset + currentChunkSize);

            ChunkData chunk(chunkData, chunkIndex, fileType);

            // Generate checksum for the chunk
            chunk.checksum = Utils::calculateSHA256(chunkData);

            // Generate CID (Content Identifier)
            chunk.cid = Utils::generateCID(chunk.checksum);

            chunks.push_back(chunk);

            g_logger.debug("Created chunk " + std::to_string(chunkIndex) +
                          " with size " + std::to_string(currentChunkSize) +
                          " bytes, CID: " + chunk.cid);

            chunkIndex++;
        }

        g_logger.info("Successfully created " + std::to_string(chunks.size()) + " chunks");

    } catch (const std::exception& e) {
        g_logger.error("Error creating chunks: " + std::string(e.what()));
        chunks.clear();
    }

    return chunks;
}

std::vector<uint8_t> Chunker::reassembleChunks(const std::vector<ChunkData>& chunks) {
    std::vector<uint8_t> reassembledData;

    try {
        g_logger.info("Starting to reassemble " + std::to_string(chunks.size()) + " chunks");

        if (chunks.empty()) {
            g_logger.warning("No chunks provided for reassembly");
            return reassembledData;
        }

        // Sort chunks by index to ensure correct order
        std::vector<ChunkData> sortedChunks = chunks;
        std::sort(sortedChunks.begin(), sortedChunks.end(),
                 [](const ChunkData& a, const ChunkData& b) {
                     return a.index < b.index;
                 });

        // Calculate total size
        size_t totalSize = 0;
        for (const auto& chunk : sortedChunks) {
            totalSize += chunk.data.size();
        }

        reassembledData.reserve(totalSize);

        // Reassemble chunks in order
        for (const auto& chunk : sortedChunks) {
            // Validate chunk integrity
            if (!validateChunk(chunk)) {
                g_logger.error("Chunk validation failed for chunk " +
                              std::to_string(chunk.index) + " with CID: " + chunk.cid);
                reassembledData.clear();
                return reassembledData;
            }

            reassembledData.insert(reassembledData.end(),
                                 chunk.data.begin(), chunk.data.end());

            g_logger.debug("Reassembled chunk " + std::to_string(chunk.index) +
                          " with " + std::to_string(chunk.data.size()) + " bytes");
        }

        g_logger.info("Successfully reassembled file with total size: " +
                     std::to_string(reassembledData.size()) + " bytes");

    } catch (const std::exception& e) {
        g_logger.error("Error reassembling chunks: " + std::string(e.what()));
        reassembledData.clear();
    }

    return reassembledData;
}

bool Chunker::validateChunk(const ChunkData& chunk) {
    try {
        // Calculate checksum of chunk data
        std::string calculatedChecksum = Utils::calculateSHA256(chunk.data);

        bool isValid = (calculatedChecksum == chunk.checksum);

        if (!isValid) {
            g_logger.error("Checksum mismatch for chunk " + std::to_string(chunk.index) +
                          ". Expected: " + chunk.checksum +
                          ", Calculated: " + calculatedChecksum);
        }

        return isValid;

    } catch (const std::exception& e) {
        g_logger.error("Error validating chunk: " + std::string(e.what()));
        return false;
    }
}