#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <vector>
#include <cstdint>

class Utils {
public:
    // Hash and CID generation
    static std::string calculateSHA256(const std::vector<uint8_t>& data);
    static std::string calculateSHA256(const std::string& data);
    static std::string generateCID(const std::string& hash);

    // Base58 encoding (simplified version)
    static std::string encodeBase58(const std::vector<uint8_t>& data);
    static std::vector<uint8_t> decodeBase58(const std::string& encoded);

    // File operations
    static std::vector<uint8_t> readFileToBytes(const std::string& filepath);
    static bool writeBytesToFile(const std::string& filepath, const std::vector<uint8_t>& data);
    static bool fileExists(const std::string& filepath);
    static bool createDirectories(const std::string& path);

    // String utilities
    static std::string trim(const std::string& str);
    static std::vector<std::string> split(const std::string& str, char delimiter);
    static std::string join(const std::vector<std::string>& parts, const std::string& delimiter);

    // MIME type detection (simplified)
    static std::string detectMimeType(const std::string& filename);
    static std::string detectMimeType(const std::vector<uint8_t>& data, const std::string& filename);

    // File extension utilities
    static std::string getFileExtension(const std::string& filename);

    // Time utilities
    static std::string getCurrentTimestamp();
    static std::string formatFileSize(size_t bytes);

    // Validation
    static bool isValidCID(const std::string& cid);
    static bool isValidFilename(const std::string& filename);
};

#endif // UTILS_H