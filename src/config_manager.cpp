#include "../include/config_manager.h"
#include "../include/logger.h"
#include <fstream>
#include <iostream>
#include <filesystem>

// Static member initialization
std::unique_ptr<ConfigManager> ConfigManager::instance = nullptr;
std::mutex ConfigManager::instanceMutex;

ConfigManager& ConfigManager::getInstance() {
    std::lock_guard<std::mutex> lock(instanceMutex);
    if (instance == nullptr) {
        instance = std::unique_ptr<ConfigManager>(new ConfigManager());
    }
    return *instance;
}

void ConfigManager::initialize(const std::string& configPath) {
    getInstance().loadConfig(configPath);
}

bool ConfigManager::loadConfig(const std::string& path) {
    configPath = path.empty() ? "config/config.json" : path;

    if (!loadConfigFile(configPath)) {
        std::cerr << "Failed to load config from: " << configPath << std::endl;
        return false;
    }

    if (!validateConfig()) {
        std::cerr << "Configuration validation failed" << std::endl;
        return false;
    }

    isLoaded = true;
    std::cout << "Configuration loaded successfully from: " << configPath << std::endl;
    return true;
}

bool ConfigManager::loadConfigFile(const std::string& path) {
    try {
        // Check if file exists
        if (!std::filesystem::exists(path)) {
            std::cerr << "Config file does not exist: " << path << std::endl;
            return false;
        }

        std::ifstream configFile(path);
        if (!configFile.is_open()) {
            std::cerr << "Cannot open config file: " << path << std::endl;
            return false;
        }

        configFile >> config;
        return true;

    } catch (const json::parse_error& e) {
        std::cerr << "JSON parse error in config file: " << e.what() << std::endl;
        return false;
    } catch (const std::exception& e) {
        std::cerr << "Error loading config file: " << e.what() << std::endl;
        return false;
    }
}

json ConfigManager::getNestedValue(const std::string& path, const json& defaultValue) const {
    if (!isLoaded) {
        return defaultValue;
    }

    try {
        json current = config;
        std::string delimiter = ".";
        size_t pos = 0;
        std::string token;
        std::string pathCopy = path;

        while ((pos = pathCopy.find(delimiter)) != std::string::npos) {
            token = pathCopy.substr(0, pos);
            if (current.contains(token)) {
                current = current[token];
            } else {
                return defaultValue;
            }
            pathCopy.erase(0, pos + delimiter.length());
        }

        if (current.contains(pathCopy)) {
            return current[pathCopy];
        } else {
            return defaultValue;
        }

    } catch (const std::exception& e) {
        return defaultValue;
    }
}

void ConfigManager::reloadConfig() {
    loadConfig(configPath);
}

bool ConfigManager::validateConfig() const {
    try {
        // Validate required sections exist
        if (!config.contains("server") || !config.contains("storage") ||
            !config.contains("database") || !config.contains("logging")) {
            return false;
        }

        // Validate server configuration
        if (getServerPort() <= 0 || getServerPort() > 65535) {
            std::cerr << "Invalid server port: " << getServerPort() << std::endl;
            return false;
        }

        // Validate storage configuration
        if (getChunkSizeBytes() <= 0 || getChunkSizeBytes() > 10485760) { // Max 10MB chunks
            std::cerr << "Invalid chunk size: " << getChunkSizeBytes() << std::endl;
            return false;
        }

        // Validate data directory
        std::string dataDir = getDataDirectory();
        if (dataDir.empty()) {
            std::cerr << "Data directory cannot be empty" << std::endl;
            return false;
        }

        return true;

    } catch (const std::exception& e) {
        std::cerr << "Config validation error: " << e.what() << std::endl;
        return false;
    }
}

// Server configuration methods
std::string ConfigManager::getServerHost() const {
    return getValue<std::string>("server.host", "localhost");
}

int ConfigManager::getServerPort() const {
    return getValue<int>("server.port", 8001);
}

int ConfigManager::getMaxConnections() const {
    return getValue<int>("server.max_connections", 100);
}

int ConfigManager::getRequestTimeout() const {
    return getValue<int>("server.request_timeout_ms", 30000);
}

int ConfigManager::getResponseTimeout() const {
    return getValue<int>("server.response_timeout_ms", 30000);
}

bool ConfigManager::isCorsEnabled() const {
    return getValue<bool>("server.enable_cors", true);
}

int ConfigManager::getMaxRequestSizeMB() const {
    return getValue<int>("server.max_request_size_mb", 100);
}

// Storage configuration methods
std::string ConfigManager::getDataDirectory() const {
    return getValue<std::string>("storage.data_directory", "data");
}

size_t ConfigManager::getChunkSizeBytes() const {
    return getValue<size_t>("storage.chunk_size_bytes", 262144);
}

int ConfigManager::getMaxFileSizeMB() const {
    return getValue<int>("storage.max_file_size_mb", 1024);
}

bool ConfigManager::isCompressionEnabled() const {
    return getValue<bool>("storage.compression_enabled", true);
}

int ConfigManager::getCompressionLevel() const {
    return getValue<int>("storage.compression_level", 6);
}

int ConfigManager::getShardCount() const {
    return getValue<int>("storage.shard_count", 256);
}

bool ConfigManager::isDeduplicationEnabled() const {
    return getValue<bool>("storage.enable_deduplication", true);
}

int ConfigManager::getCleanupIntervalHours() const {
    return getValue<int>("storage.cleanup_interval_hours", 24);
}

// Database configuration methods
int ConfigManager::getWriteBufferSizeMB() const {
    return getValue<int>("database.leveldb.write_buffer_size_mb", 4);
}

int ConfigManager::getMaxFileSizeMB_DB() const {
    return getValue<int>("database.leveldb.max_file_size_mb", 2);
}

std::string ConfigManager::getCompressionType() const {
    return getValue<std::string>("database.leveldb.compression", "snappy");
}

bool ConfigManager::getParanoidChecks() const {
    return getValue<bool>("database.leveldb.paranoid_checks", false);
}

int ConfigManager::getBlockCacheSizeMB() const {
    return getValue<int>("database.leveldb.block_cache_size_mb", 8);
}

int ConfigManager::getBloomFilterBits() const {
    return getValue<int>("database.leveldb.bloom_filter_bits", 10);
}

int ConfigManager::getMaxOpenFiles() const {
    return getValue<int>("database.leveldb.max_open_files", 1000);
}

bool ConfigManager::getSyncWrites() const {
    return getValue<bool>("database.leveldb.sync_writes", false);
}

// Connection pool configuration methods
int ConfigManager::getMaxDBConnections() const {
    return getValue<int>("database.connection_pool.max_connections", 10);
}

int ConfigManager::getMinDBConnections() const {
    return getValue<int>("database.connection_pool.min_connections", 2);
}

int ConfigManager::getConnectionTimeout() const {
    return getValue<int>("database.connection_pool.connection_timeout_ms", 5000);
}

int ConfigManager::getIdleTimeout() const {
    return getValue<int>("database.connection_pool.idle_timeout_ms", 300000);
}

int ConfigManager::getMaxRetries() const {
    return getValue<int>("database.connection_pool.max_retries", 3);
}

int ConfigManager::getRetryDelay() const {
    return getValue<int>("database.connection_pool.retry_delay_ms", 1000);
}

// Logging configuration methods
std::string ConfigManager::getLogLevel() const {
    return getValue<std::string>("logging.level", "INFO");
}

std::string ConfigManager::getLogFilePath() const {
    return getValue<std::string>("logging.file_path", "logs/qfs.log");
}

int ConfigManager::getMaxLogFileSizeMB() const {
    return getValue<int>("logging.max_file_size_mb", 10);
}

int ConfigManager::getMaxLogFiles() const {
    return getValue<int>("logging.max_files", 5);
}

bool ConfigManager::isConsoleEnabled() const {
    return getValue<bool>("logging.enable_console", true);
}

bool ConfigManager::isJsonLoggingEnabled() const {
    return getValue<bool>("logging.enable_json", false);
}

// Security configuration methods
bool ConfigManager::isAuthenticationEnabled() const {
    return getValue<bool>("security.enable_authentication", false);
}

bool ConfigManager::isApiKeyRequired() const {
    return getValue<bool>("security.api_key_required", false);
}

bool ConfigManager::isRateLimitingEnabled() const {
    return getValue<bool>("security.rate_limiting.enabled", true);
}

int ConfigManager::getRequestsPerMinute() const {
    return getValue<int>("security.rate_limiting.requests_per_minute", 60);
}

int ConfigManager::getBurstSize() const {
    return getValue<int>("security.rate_limiting.burst_size", 10);
}

std::vector<std::string> ConfigManager::getAllowedExtensions() const {
    std::vector<std::string> defaultExtensions = {".txt", ".json", ".csv", ".log", ".md"};
    try {
        auto extensions = getNestedValue("security.upload_restrictions.allowed_extensions");
        if (extensions.is_array()) {
            return extensions.get<std::vector<std::string>>();
        }
    } catch (const std::exception& e) {
        // Return default
    }
    return defaultExtensions;
}

std::vector<std::string> ConfigManager::getBlockedExtensions() const {
    std::vector<std::string> defaultBlocked = {".exe", ".bat", ".sh", ".ps1", ".cmd"};
    try {
        auto extensions = getNestedValue("security.upload_restrictions.blocked_extensions");
        if (extensions.is_array()) {
            return extensions.get<std::vector<std::string>>();
        }
    } catch (const std::exception& e) {
        // Return default
    }
    return defaultBlocked;
}

// Performance configuration methods
int ConfigManager::getWorkerThreads() const {
    return getValue<int>("performance.threading.worker_threads", 4);
}

int ConfigManager::getIOThreads() const {
    return getValue<int>("performance.threading.io_threads", 2);
}

bool ConfigManager::isThreadPoolEnabled() const {
    return getValue<bool>("performance.threading.enable_thread_pool", true);
}

bool ConfigManager::isMemoryCacheEnabled() const {
    return getValue<bool>("performance.caching.enable_memory_cache", true);
}

int ConfigManager::getCacheSizeMB() const {
    return getValue<int>("performance.caching.cache_size_mb", 64);
}

int ConfigManager::getCacheTTLMinutes() const {
    return getValue<int>("performance.caching.cache_ttl_minutes", 30);
}

// Timeout configuration methods
int ConfigManager::getUploadTimeout() const {
    return getValue<int>("performance.timeouts.upload_timeout_ms", 60000);
}

int ConfigManager::getDownloadTimeout() const {
    return getValue<int>("performance.timeouts.download_timeout_ms", 30000);
}

int ConfigManager::getUpdateTimeout() const {
    return getValue<int>("performance.timeouts.update_timeout_ms", 600000);
}

int ConfigManager::getDeleteTimeout() const {
    return getValue<int>("performance.timeouts.delete_timeout_ms", 300000);
}

int ConfigManager::getChunkTimeout() const {
    return getValue<int>("performance.timeouts.chunk_timeout_ms", 10000);
}

int ConfigManager::getListTimeout() const {
    return getValue<int>("performance.timeouts.list_timeout_ms", 5000);
}

// Monitoring configuration methods
bool ConfigManager::isMetricsEnabled() const {
    return getValue<bool>("monitoring.enable_metrics", true);
}

std::string ConfigManager::getMetricsEndpoint() const {
    return getValue<std::string>("monitoring.metrics_endpoint", "/metrics");
}

int ConfigManager::getHealthCheckInterval() const {
    return getValue<int>("monitoring.health_check_interval_seconds", 30);
}

int ConfigManager::getStorageUsageAlert() const {
    return getValue<int>("monitoring.storage_usage_alert_percent", 90);
}

int ConfigManager::getMemoryUsageAlert() const {
    return getValue<int>("monitoring.memory_usage_alert_percent", 85);
}

// Development configuration methods
bool ConfigManager::isDebugMode() const {
    return getValue<bool>("development.debug_mode", false);
}

bool ConfigManager::isVerboseLogging() const {
    return getValue<bool>("development.verbose_logging", false);
}

std::string ConfigManager::getTestDataPath() const {
    return getValue<std::string>("development.test_data_path", "test_data");
}

// Version and environment methods
std::string ConfigManager::getVersion() const {
    return getValue<std::string>("version", "1.0.0");
}

std::string ConfigManager::getEnvironment() const {
    return getValue<std::string>("environment", "production");
}

std::string ConfigManager::getConfigSummary() const {
    if (!isLoaded) {
        return "Configuration not loaded";
    }

    std::ostringstream summary;
    summary << "QFS Configuration Summary:\n";
    summary << "========================\n";
    summary << "Environment: " << getEnvironment() << "\n";
    summary << "Version: " << getVersion() << "\n";
    summary << "Server: " << getServerHost() << ":" << getServerPort() << "\n";
    summary << "Data Directory: " << getDataDirectory() << "\n";
    summary << "Chunk Size: " << getChunkSizeBytes() << " bytes\n";
    summary << "Max File Size: " << getMaxFileSizeMB() << " MB\n";
    summary << "Deduplication: " << (isDeduplicationEnabled() ? "Enabled" : "Disabled") << "\n";
    summary << "Log Level: " << getLogLevel() << "\n";
    summary << "Debug Mode: " << (isDebugMode() ? "Enabled" : "Disabled") << "\n";

    return summary.str();
}