/**
 * Provider Registry - In-memory storage for QFS provider information
 *
 * This class maintains a registry of which nodes have which files,
 * enabling fast provider discovery for the P2P network.
 *
 * @file provider_registry.h
 * @author QFS Development Team
 * @date October 15, 2025
 * @version 1.0.0
 */

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <chrono>
#include <sstream>
#include <iomanip>

namespace qfs {

/**
 * ProviderInfo - Information about a node that has a file
 *
 * Contains all necessary information to connect to a provider
 * and download a file.
 */
struct ProviderInfo {
    std::string nodeId;        // Unique node identifier
    std::string ip;            // IP address
    int port;                  // Port number
    uint64_t bandwidth;        // Available bandwidth (bytes/sec)
    std::chrono::system_clock::time_point lastSeen;  // Last announcement time

    /**
     * Default constructor
     */
    ProviderInfo()
        : port(0)
        , bandwidth(0)
        , lastSeen(std::chrono::system_clock::now())
    {}

    /**
     * Parameterized constructor
     */
    ProviderInfo(const std::string& id, const std::string& address,
                 int p, uint64_t bw)
        : nodeId(id)
        , ip(address)
        , port(p)
        , bandwidth(bw)
        , lastSeen(std::chrono::system_clock::now())
    {}

    /**
     * Convert to JSON string for API responses
     *
     * @return JSON representation of provider info
     */
    std::string toJson() const {
        std::ostringstream json;

        // Format timestamp
        auto time = std::chrono::system_clock::to_time_t(lastSeen);
        std::tm tm;
        #ifdef _WIN32
            localtime_s(&tm, &time);
        #else
            localtime_r(&time, &tm);
        #endif

        json << "{\n";
        json << "      \"nodeId\": \"" << nodeId << "\",\n";
        json << "      \"ip\": \"" << ip << "\",\n";
        json << "      \"port\": " << port << ",\n";
        json << "      \"bandwidth\": " << bandwidth << ",\n";
        json << "      \"lastSeen\": \""
             << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S") << "Z\"\n";
        json << "    }";

        return json.str();
    }
};

/**
 * ProviderRegistry - Thread-safe in-memory provider registry
 *
 * Maintains a mapping of CIDs to provider lists, allowing fast
 * lookups and updates in a multi-threaded environment.
 */
class ProviderRegistry {
public:
    /**
     * Constructor
     */
    ProviderRegistry();

    /**
     * Destructor
     */
    ~ProviderRegistry();

    /**
     * Add or update a provider for a CID
     *
     * If the provider already exists, updates its information.
     * Otherwise, adds it as a new provider.
     *
     * @param cid Content Identifier
     * @param info Provider information
     */
    void addProvider(const std::string& cid, const ProviderInfo& info);

    /**
     * Get all providers for a CID
     *
     * Returns a copy of the provider list to avoid threading issues.
     *
     * @param cid Content Identifier
     * @return Vector of provider information (empty if none found)
     */
    std::vector<ProviderInfo> getProviders(const std::string& cid) const;

    /**
     * Check if any providers exist for a CID
     *
     * Faster than getProviders() when only checking existence.
     *
     * @param cid Content Identifier
     * @return true if providers exist, false otherwise
     */
    bool hasProviders(const std::string& cid) const;

    /**
     * Remove stale providers
     *
     * Removes providers that haven't announced within the timeout period.
     * This helps keep the registry clean and current.
     *
     * @param timeoutSeconds Maximum age of providers (in seconds)
     * @return Number of providers removed
     */
    size_t removeStaleProviders(int timeoutSeconds);

    /**
     * Get total number of CIDs registered
     *
     * @return Number of unique CIDs
     */
    size_t getTotalCIDs() const;

    /**
     * Get total number of providers across all CIDs
     *
     * @return Total provider count
     */
    size_t getTotalProviders() const;

    /**
     * Clear all providers (for testing or reset)
     */
    void clear();

private:
    // Main registry: CID -> List of Providers
    std::unordered_map<std::string, std::vector<ProviderInfo>> registry_;

    // Mutex for thread-safe access
    mutable std::mutex mutex_;
};

} // namespace qfs
