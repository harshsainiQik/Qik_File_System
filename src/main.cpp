#include "../include/logger.h"
#include "../include/http_server.h"
#include "../include/utils.h"
#include "../include/instance_lock.h"
#include "../include/config_manager.h"
#include <iostream>
#include <signal.h>
#include <thread>
#include <chrono>

// Global server instance for signal handling
HttpServer* g_server = nullptr;
InstanceLock* g_instanceLock = nullptr;

void signalHandler(int signal) {
    std::cout << "\nReceived signal " << signal << ", initiating graceful shutdown...\n";

    try {
        if (g_server) {
            std::cout << "Stopping HTTP server...\n";
            g_server->stop();
            std::cout << "HTTP server stopped successfully.\n";
        }

        std::cout << "Cleaning up resources...\n";

        // Release instance lock
        if (g_instanceLock) {
            std::cout << "Releasing instance lock...\n";
            g_instanceLock->releaseLock();
            delete g_instanceLock;
            g_instanceLock = nullptr;
        }

        // Allow some time for cleanup operations
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        std::cout << "QFS shutdown complete.\n";

    } catch (const std::exception& e) {
        std::cerr << "Error during shutdown: " << e.what() << "\n";
    }

    exit(0);
}

void printBanner() {
    std::cout << "╔═══════════════════════════════════════════════════════════════╗\n";
    std::cout << "║                    QFS - Qik File System                     ║\n";
    std::cout << "║                   Simplified IPFS-like Service               ║\n";
    std::cout << "║                         Version 1.0.0                        ║\n";
    std::cout << "╚═══════════════════════════════════════════════════════════════╝\n";
    std::cout << "\n";
}

void printUsage() {
    std::cout << "Usage: qfs [OPTIONS]\n\n";
    std::cout << "Options:\n";
    std::cout << "  -p, --port PORT       Server port (default: 8080)\n";
    std::cout << "  -h, --host HOST       Server host (default: localhost)\n";
    std::cout << "  -d, --data-dir DIR    Data directory (default: data)\n";
    std::cout << "  -v, --verbose         Enable verbose logging\n";
    std::cout << "  --help                Show this help message\n\n";
    std::cout << "Examples:\n";
    std::cout << "  qfs                               # Start with default settings\n";
    std::cout << "  qfs -p 9000 -h 0.0.0.0          # Start on port 9000, all interfaces\n";
    std::cout << "  qfs -d /custom/data -v           # Custom data directory with verbose logging\n\n";
}

bool parseArguments(int argc, char* argv[], int& port, std::string& host,
                   std::string& dataDir, bool& verbose) {
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "--help") {
            printUsage();
            return false;
        } else if (arg == "-p" || arg == "--port") {
            if (i + 1 < argc) {
                try {
                    port = std::stoi(argv[++i]);
                    if (port < 1 || port > 65535) {
                        std::cerr << "Error: Port must be between 1 and 65535\n";
                        return false;
                    }
                } catch (const std::exception& e) {
                    std::cerr << "Error: Invalid port number\n";
                    return false;
                }
            } else {
                std::cerr << "Error: --port requires a value\n";
                return false;
            }
        } else if (arg == "-h" || arg == "--host") {
            if (i + 1 < argc) {
                host = argv[++i];
            } else {
                std::cerr << "Error: --host requires a value\n";
                return false;
            }
        } else if (arg == "-d" || arg == "--data-dir") {
            if (i + 1 < argc) {
                dataDir = argv[++i];
            } else {
                std::cerr << "Error: --data-dir requires a value\n";
                return false;
            }
        } else if (arg == "-v" || arg == "--verbose") {
            verbose = true;
        } else {
            std::cerr << "Error: Unknown option: " << arg << "\n";
            std::cerr << "Use --help for usage information\n";
            return false;
        }
    }

    return true;
}

int main(int argc, char* argv[]) {
    try {
        // Initialize configuration manager first
        ConfigManager::initialize("config/config.json");
        if (!CONFIG.isConfigLoaded()) {
            std::cerr << "Failed to load configuration. Using defaults.\n";
        }

        // Get default values from config
        int port = CONFIG.getServerPort();
        std::string host = CONFIG.getServerHost();
        std::string dataDir = CONFIG.getDataDirectory();
        bool verbose = CONFIG.isVerboseLogging();

        // Parse command line arguments (can override config)
        if (!parseArguments(argc, argv, port, host, dataDir, verbose)) {
            return 1;
        }

        // Print banner
        printBanner();

        // Print configuration summary
        if (CONFIG.isConfigLoaded()) {
            std::cout << CONFIG.getConfigSummary() << std::endl;
        }

        // Initialize logger
        Logger logger(dataDir + "/logs/qfs.log", verbose ? LogLevel::DEBUG : LogLevel::INFO);

        if (!logger.isOpen()) {
            std::cerr << "Failed to initialize logger. Check data directory permissions.\n";
            return 1;
        }

        logger.info("========================================");
        logger.info("QFS (Qik File System) Starting...");
        logger.info("Version: 1.0.0");
        logger.info("Host: " + host);
        logger.info("Port: " + std::to_string(port));
        logger.info("Data Directory: " + dataDir);
        std::string logLevelStr = verbose ? "DEBUG" : "INFO";
        logger.info("Log Level: " + logLevelStr);
        logger.info("========================================");

        // Create data directories if they don't exist
        if (!Utils::createDirectories(dataDir)) {
            std::cerr << "Failed to create data directories. Check permissions.\n";
            logger.error("Failed to create data directories: " + dataDir);
            return 1;
        }

        // Acquire instance lock to prevent multiple instances
        std::cout << "Checking for existing QFS instances...\n";
        g_instanceLock = new InstanceLock(dataDir);
        if (!g_instanceLock->acquireLock()) {
            std::cerr << "ERROR: Another QFS instance is already running in this data directory!\n";
            std::cerr << "If you're sure no other instance is running, delete the files:\n";
            std::cerr << "  - " << dataDir << "/qfs.lock\n";
            std::cerr << "  - " << dataDir << "/qfs.pid\n";
            logger.error("Failed to acquire instance lock - another QFS instance is running");

            delete g_instanceLock;
            g_instanceLock = nullptr;
            return 1;
        }
        std::cout << "Instance lock acquired successfully.\n";

        // Create HTTP server
        HttpServer server(port, host);
        g_server = &server;

        // Setup signal handlers for graceful shutdown
        signal(SIGINT, signalHandler);   // Ctrl+C
        signal(SIGTERM, signalHandler);  // Termination signal
        #ifndef _WIN32
        signal(SIGQUIT, signalHandler);  // Quit signal (Unix only)
        #endif

        // Initialize server
        std::cout << "Initializing QFS server...\n";
        std::cout << "About to call server.initialize()...\n";
        bool initResult = server.initialize();
        std::cout << "server.initialize() call completed with result: " << (initResult ? "true" : "false") << "\n";
        if (!initResult) {
            std::cerr << "Failed to initialize server. Check logs for details.\n";
            logger.error("Server initialization failed");
            return 1;
        }

        std::cout << "Server initialization returned successfully!\n";
        std::cout << "About to start HTTP server...\n";

        // Start server
        std::cout << "Starting HTTP server on http://" << host << ":" << port << "\n";
        std::cout << "\nAvailable endpoints:\n";
        std::cout << "  POST   /upload           - Upload a file\n";
        std::cout << "  PUT    /update/{cid}     - Update an existing file\n";
        std::cout << "  GET    /download/{cid}   - Download a file by CID\n";
        std::cout << "  GET    /chunk/{cid}      - Download individual chunk by CID\n";
        std::cout << "  DELETE /delete/{cid}     - Delete a file by CID\n";
        std::cout << "  GET    /status          - Get server status\n";
        std::cout << "  GET    /list            - List all files\n\n";

        if (!server.start()) {
            std::cerr << "Failed to start HTTP server. Check if port is available.\n";
            logger.error("Failed to start HTTP server on port " + std::to_string(port));
            return 1;
        }

        std::cout << "QFS server is running! Press Ctrl+C to stop.\n\n";

        // Keep the main thread alive
        while (server.isServerRunning()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        std::cout << "Server has stopped.\n";
        logger.info("QFS server shutdown complete");

        // Release instance lock on normal shutdown
        if (g_instanceLock) {
            g_instanceLock->releaseLock();
            delete g_instanceLock;
            g_instanceLock = nullptr;
        }

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown fatal error occurred" << std::endl;
        return 1;
    }
}