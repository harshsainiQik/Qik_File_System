#include "../include/utils.h"
#include "../include/logger.h"
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <algorithm>
#include <cctype>

std::string Utils::calculateSHA256(const std::vector<uint8_t>& data) {
    try {
        unsigned char hash[SHA256_DIGEST_LENGTH];
        SHA256_CTX sha256;
        SHA256_Init(&sha256);
        SHA256_Update(&sha256, data.data(), data.size());
        SHA256_Final(hash, &sha256);

        std::stringstream ss;
        for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
            ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
        }
        return ss.str();
    } catch (const std::exception& e) {
        g_logger.error("Error calculating SHA256: " + std::string(e.what()));
        return "";
    }
}

std::string Utils::calculateSHA256(const std::string& data) {
    std::vector<uint8_t> bytes(data.begin(), data.end());
    return calculateSHA256(bytes);
}

std::string Utils::generateCID(const std::string& hash) {
    try {
        // Simple CID generation: "Qm" prefix + first 44 characters of hash
        std::string cid = "Qm" + hash.substr(0, 44);
        return cid;
    } catch (const std::exception& e) {
        g_logger.error("Error generating CID: " + std::string(e.what()));
        return "";
    }
}

std::string Utils::encodeBase58(const std::vector<uint8_t>& data) {
    // Simplified Base58 encoding (for demonstration)
    // In production, use a proper Base58 library
    static const char alphabet[] = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

    std::string result;
    try {
        // Convert to string for simple encoding
        for (uint8_t byte : data) {
            result += alphabet[byte % 58];
        }
    } catch (const std::exception& e) {
        g_logger.error("Error in Base58 encoding: " + std::string(e.what()));
        return "";
    }
    return result;
}

std::vector<uint8_t> Utils::decodeBase58(const std::string& encoded) {
    // Simplified Base58 decoding (for demonstration)
    std::vector<uint8_t> result;
    try {
        for (char c : encoded) {
            result.push_back(static_cast<uint8_t>(c));
        }
    } catch (const std::exception& e) {
        g_logger.error("Error in Base58 decoding: " + std::string(e.what()));
        return {};
    }
    return result;
}

std::vector<uint8_t> Utils::readFileToBytes(const std::string& filepath) {
    std::vector<uint8_t> data;

    try {
        std::ifstream file(filepath, std::ios::binary);
        if (!file.is_open()) {
            g_logger.error("Cannot open file for reading: " + filepath);
            return data;
        }

        file.seekg(0, std::ios::end);
        size_t fileSize = file.tellg();
        file.seekg(0, std::ios::beg);

        data.resize(fileSize);
        file.read(reinterpret_cast<char*>(data.data()), fileSize);

        if (file.fail()) {
            g_logger.error("Error reading file: " + filepath);
            data.clear();
        } else {
            g_logger.debug("Successfully read " + std::to_string(fileSize) +
                          " bytes from " + filepath);
        }

        file.close();
    } catch (const std::exception& e) {
        g_logger.error("Exception reading file " + filepath + ": " + e.what());
        data.clear();
    }

    return data;
}

bool Utils::writeBytesToFile(const std::string& filepath, const std::vector<uint8_t>& data) {
    try {
        // Create directories if they don't exist
        std::filesystem::path filePath(filepath);
        if (filePath.has_parent_path()) {
            std::filesystem::create_directories(filePath.parent_path());
        }

        std::ofstream file(filepath, std::ios::binary);
        if (!file.is_open()) {
            g_logger.error("Cannot open file for writing: " + filepath);
            return false;
        }

        file.write(reinterpret_cast<const char*>(data.data()), data.size());

        if (file.fail()) {
            g_logger.error("Error writing to file: " + filepath);
            return false;
        }

        file.close();
        g_logger.debug("Successfully wrote " + std::to_string(data.size()) +
                      " bytes to " + filepath);
        return true;

    } catch (const std::exception& e) {
        g_logger.error("Exception writing file " + filepath + ": " + e.what());
        return false;
    }
}

bool Utils::fileExists(const std::string& filepath) {
    try {
        return std::filesystem::exists(filepath);
    } catch (const std::exception& e) {
        g_logger.error("Error checking file existence: " + std::string(e.what()));
        return false;
    }
}

bool Utils::createDirectories(const std::string& path) {
    try {
        std::filesystem::create_directories(path);
        return true;
    } catch (const std::exception& e) {
        g_logger.error("Error creating directories: " + std::string(e.what()));
        return false;
    }
}

std::string Utils::trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) return "";

    size_t end = str.find_last_not_of(" \t\n\r");
    return str.substr(start, end - start + 1);
}

std::vector<std::string> Utils::split(const std::string& str, char delimiter) {
    std::vector<std::string> result;
    std::stringstream ss(str);
    std::string item;

    while (std::getline(ss, item, delimiter)) {
        result.push_back(item);
    }

    return result;
}

std::string Utils::join(const std::vector<std::string>& parts, const std::string& delimiter) {
    if (parts.empty()) return "";

    std::string result = parts[0];
    for (size_t i = 1; i < parts.size(); ++i) {
        result += delimiter + parts[i];
    }
    return result;
}

std::string Utils::detectMimeType(const std::string& filename) {
    try {
        std::string extension = filename.substr(filename.find_last_of('.'));
        std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

        // Text files
        if (extension == ".txt") return "text/plain";
        else if (extension == ".html" || extension == ".htm") return "text/html";
        else if (extension == ".css") return "text/css";
        else if (extension == ".csv") return "text/csv";
        else if (extension == ".xml") return "text/xml";

        // Documents
        else if (extension == ".pdf") return "application/pdf";
        else if (extension == ".doc") return "application/msword";
        else if (extension == ".docx") return "application/vnd.openxmlformats-officedocument.wordprocessingml.document";
        else if (extension == ".xls") return "application/vnd.ms-excel";
        else if (extension == ".xlsx") return "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet";
        else if (extension == ".ppt") return "application/vnd.ms-powerpoint";
        else if (extension == ".pptx") return "application/vnd.openxmlformats-officedocument.presentationml.presentation";
        else if (extension == ".rtf") return "application/rtf";

        // Images
        else if (extension == ".jpg" || extension == ".jpeg") return "image/jpeg";
        else if (extension == ".png") return "image/png";
        else if (extension == ".gif") return "image/gif";
        else if (extension == ".bmp") return "image/bmp";
        else if (extension == ".webp") return "image/webp";
        else if (extension == ".svg") return "image/svg+xml";
        else if (extension == ".ico") return "image/x-icon";
        else if (extension == ".tiff" || extension == ".tif") return "image/tiff";

        // Audio
        else if (extension == ".mp3") return "audio/mpeg";
        else if (extension == ".wav") return "audio/wav";
        else if (extension == ".flac") return "audio/flac";
        else if (extension == ".aac") return "audio/aac";
        else if (extension == ".ogg") return "audio/ogg";
        else if (extension == ".m4a") return "audio/mp4";

        // Video
        else if (extension == ".mp4") return "video/mp4";
        else if (extension == ".avi") return "video/x-msvideo";
        else if (extension == ".mov") return "video/quicktime";
        else if (extension == ".wmv") return "video/x-ms-wmv";
        else if (extension == ".flv") return "video/x-flv";
        else if (extension == ".mkv") return "video/x-matroska";
        else if (extension == ".webm") return "video/webm";

        // Archives
        else if (extension == ".zip") return "application/zip";
        else if (extension == ".rar") return "application/vnd.rar";
        else if (extension == ".7z") return "application/x-7z-compressed";
        else if (extension == ".tar") return "application/x-tar";
        else if (extension == ".gz") return "application/gzip";
        else if (extension == ".bz2") return "application/x-bzip2";

        // Programming/Data
        else if (extension == ".json") return "application/json";
        else if (extension == ".js") return "application/javascript";
        else if (extension == ".py") return "text/x-python";
        else if (extension == ".cpp" || extension == ".cc" || extension == ".cxx") return "text/x-c++src";
        else if (extension == ".c") return "text/x-csrc";
        else if (extension == ".h" || extension == ".hpp") return "text/x-chdr";
        else if (extension == ".java") return "text/x-java-source";
        else if (extension == ".php") return "text/x-php";
        else if (extension == ".rb") return "text/x-ruby";
        else if (extension == ".go") return "text/x-go";
        else if (extension == ".rs") return "text/x-rust";
        else if (extension == ".sql") return "application/sql";

        // Other common types
        else if (extension == ".exe") return "application/x-msdownload";
        else if (extension == ".dmg") return "application/x-apple-diskimage";
        else if (extension == ".iso") return "application/x-iso9660-image";
        else if (extension == ".deb") return "application/vnd.debian.binary-package";
        else if (extension == ".rpm") return "application/x-rpm";

        else return "application/octet-stream";

    } catch (const std::exception& e) {
        g_logger.error("Error detecting MIME type: " + std::string(e.what()));
        return "application/octet-stream";
    }
}

std::string Utils::detectMimeType(const std::vector<uint8_t>& data, const std::string& filename) {
    // For now, just use filename extension
    // In production, you could implement magic number detection
    return detectMimeType(filename);
}

std::string Utils::getFileExtension(const std::string& filename) {
    try {
        size_t dotPos = filename.find_last_of('.');
        if (dotPos == std::string::npos || dotPos == filename.length() - 1) {
            return ""; // No extension found
        }

        std::string extension = filename.substr(dotPos + 1);
        std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
        return extension;

    } catch (const std::exception& e) {
        g_logger.error("Error extracting file extension: " + std::string(e.what()));
        return "";
    }
}

std::string Utils::getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);

    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

std::string Utils::formatFileSize(size_t bytes) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    double size = static_cast<double>(bytes);
    int unit = 0;

    while (size >= 1024.0 && unit < 4) {
        size /= 1024.0;
        unit++;
    }

    std::stringstream ss;
    ss << std::fixed << std::setprecision(2) << size << " " << units[unit];
    return ss.str();
}

bool Utils::isValidCID(const std::string& cid) {
    try {
        // Simple CID validation: should start with "Qm" and have reasonable length
        return (cid.length() >= 46 && cid.substr(0, 2) == "Qm");
    } catch (const std::exception& e) {
        g_logger.error("Error validating CID: " + std::string(e.what()));
        return false;
    }
}

bool Utils::isValidFilename(const std::string& filename) {
    try {
        // Basic filename validation
        return !filename.empty() &&
               filename.find_first_of("\\/:*?\"<>|") == std::string::npos;
    } catch (const std::exception& e) {
        g_logger.error("Error validating filename: " + std::string(e.what()));
        return false;
    }
}