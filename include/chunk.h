#ifndef CHUNK_H
#define CHUNK_H

#include <vector>
#include <string>
#include <cstdint>
#include "config_manager.h"

struct ChunkData {
    std::vector<uint8_t> data;
    size_t index;
    size_t originalSize;
    std::string cid;
    std::string fileType;
    std::string checksum;

    ChunkData() : index(0), originalSize(0) {}

    ChunkData(const std::vector<uint8_t>& chunkData, size_t idx, const std::string& type)
        : data(chunkData), index(idx), originalSize(chunkData.size()), fileType(type) {}
};

class Chunker {
private:
    size_t chunkSize;

public:
    Chunker();

    // Split file data into chunks
    std::vector<ChunkData> createChunks(const std::vector<uint8_t>& fileData,
                                       const std::string& fileType);

    // Reassemble chunks back into original file
    std::vector<uint8_t> reassembleChunks(const std::vector<ChunkData>& chunks);

    // Get chunk size
    size_t getChunkSize() const { return chunkSize; }

    // Validate chunk integrity
    bool validateChunk(const ChunkData& chunk);
};

#endif // CHUNK_H