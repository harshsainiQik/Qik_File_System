#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <string>
#include <nlohmann-json/json.hpp>
#include <memory>
#include <mutex>

using json = nlohmann::json;

class ConfigManager {
private:
    static std::unique_ptr<ConfigManager> instance;
    static std::mutex instanceMutex;

    json config;
    std::string configPath;
    bool isLoaded;

    // Private constructor for singleton
    ConfigManager() : isLoaded(false) {}

    // Helper methods
    bool loadConfigFile(const std::string& path);
    json getNestedValue(const std::string& path, const json& defaultValue = json()) const;

public:
    // Singleton access
    static ConfigManager& getInstance();
    static void initialize(const std::string& configPath = "config/config.json");

    // Destructor
    ~ConfigManager() = default;

    // Delete copy constructor and assignment operator
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;

    // Configuration loading
    bool loadConfig(const std::string& path = "");
    bool isConfigLoaded() const { return isLoaded; }
    void reloadConfig();

    // Server configuration
    std::string getServerHost() const;
    int getServerPort() const;
    int getMaxConnections() const;
    int getRequestTimeout() const;
    int getResponseTimeout() const;
    bool isCorsEnabled() const;
    int getMaxRequestSizeMB() const;

    // Storage configuration
    std::string getDataDirectory() const;
    size_t getChunkSizeBytes() const;
    int getMaxFileSizeMB() const;
    bool isCompressionEnabled() const;
    int getCompressionLevel() const;
    int getShardCount() const;
    bool isDeduplicationEnabled() const;
    int getCleanupIntervalHours() const;

    // Database configuration
    int getWriteBufferSizeMB() const;
    int getMaxFileSizeMB_DB() const;
    std::string getCompressionType() const;
    bool getParanoidChecks() const;
    int getBlockCacheSizeMB() const;
    int getBloomFilterBits() const;
    int getMaxOpenFiles() const;
    bool getSyncWrites() const;

    // Connection pool configuration
    int getMaxDBConnections() const;
    int getMinDBConnections() const;
    int getConnectionTimeout() const;
    int getIdleTimeout() const;
    int getMaxRetries() const;
    int getRetryDelay() const;

    // Logging configuration
    std::string getLogLevel() const;
    std::string getLogFilePath() const;
    int getMaxLogFileSizeMB() const;
    int getMaxLogFiles() const;
    bool isConsoleEnabled() const;
    bool isJsonLoggingEnabled() const;

    // Security configuration
    bool isAuthenticationEnabled() const;
    bool isApiKeyRequired() const;
    bool isRateLimitingEnabled() const;
    int getRequestsPerMinute() const;
    int getBurstSize() const;
    std::vector<std::string> getAllowedExtensions() const;
    std::vector<std::string> getBlockedExtensions() const;

    // Performance configuration
    int getWorkerThreads() const;
    int getIOThreads() const;
    bool isThreadPoolEnabled() const;
    bool isMemoryCacheEnabled() const;
    int getCacheSizeMB() const;
    int getCacheTTLMinutes() const;

    // Timeout configuration
    int getUploadTimeout() const;
    int getDownloadTimeout() const;
    int getUpdateTimeout() const;
    int getDeleteTimeout() const;
    int getChunkTimeout() const;
    int getListTimeout() const;

    // Monitoring configuration
    bool isMetricsEnabled() const;
    std::string getMetricsEndpoint() const;
    int getHealthCheckInterval() const;
    int getStorageUsageAlert() const;
    int getMemoryUsageAlert() const;

    // Development configuration
    bool isDebugMode() const;
    bool isVerboseLogging() const;
    std::string getTestDataPath() const;

    // Version and environment
    std::string getVersion() const;
    std::string getEnvironment() const;

    // Generic configuration access
    template<typename T>
    T getValue(const std::string& path, const T& defaultValue) const {
        try {
            json value = getNestedValue(path);
            if (value.is_null()) {
                return defaultValue;
            }
            return value.get<T>();
        } catch (const std::exception& e) {
            return defaultValue;
        }
    }

    // Configuration validation
    bool validateConfig() const;
    std::string getConfigSummary() const;
};

// Global access macro for convenience
#define CONFIG ConfigManager::getInstance()

#endif // CONFIG_MANAGER_H