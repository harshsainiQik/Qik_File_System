/**
 * QFS Bootstrap Node
 *
 * Provider discovery service for QFS peer-to-peer file sharing network.
 * This service maintains a registry of which nodes have which files,
 * enabling efficient peer discovery without complex DHT protocols.
 *
 * @file main.cpp
 * @author QFS Development Team
 * @date October 15, 2025
 * @version 1.0.0
 */

#include "bootstrap_server.h"
#include <iostream>
#include <string>
#include <cstdlib>
#include <csignal>
#include <memory>

// Global pointer for signal handler
std::unique_ptr<qfs::BootstrapServer> g_server;

// Version information
const std::string VERSION = "1.0.0";
const std::string BUILD_DATE = __DATE__;

/**
 * Print usage information
 */
void printUsage(const char* programName) {
    std::cout << "QFS Bootstrap Node v" << VERSION << "\n";
    std::cout << "Provider discovery service for QFS P2P network\n\n";
    std::cout << "Usage: " << programName << " [options]\n\n";
    std::cout << "Options:\n";
    std::cout << "  -h, --help              Show this help message\n";
    std::cout << "  -v, --version           Show version information\n";
    std::cout << "  -p, --port <port>       Port to listen on (default: 8090)\n";
    std::cout << "  --host <host>           Host to bind to (default: 0.0.0.0)\n";
    std::cout << "  -c, --config <file>     Configuration file path\n";
    std::cout << "  --verbose               Enable verbose logging\n";
    std::cout << "\n";
    std::cout << "Examples:\n";
    std::cout << "  " << programName << "                          # Start with defaults\n";
    std::cout << "  " << programName << " --port 9000              # Use custom port\n";
    std::cout << "  " << programName << " --config bootstrap.json  # Use config file\n";
    std::cout << "\n";
}

/**
 * Print version information
 */
void printVersion() {
    std::cout << "QFS Bootstrap Node\n";
    std::cout << "Version: " << VERSION << "\n";
    std::cout << "Build Date: " << BUILD_DATE << "\n";
    std::cout << "C++ Standard: " << __cplusplus << "\n";
}

/**
 * Parse command line arguments
 */
struct Config {
    int port = 8090;
    std::string host = "0.0.0.0";
    std::string configFile = "";
    bool verbose = false;
    bool showHelp = false;
    bool showVersion = false;
};

Config parseArguments(int argc, char* argv[]) {
    Config config;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            config.showHelp = true;
        }
        else if (arg == "-v" || arg == "--version") {
            config.showVersion = true;
        }
        else if (arg == "-p" || arg == "--port") {
            if (i + 1 < argc) {
                config.port = std::atoi(argv[++i]);
            }
        }
        else if (arg == "--host") {
            if (i + 1 < argc) {
                config.host = argv[++i];
            }
        }
        else if (arg == "-c" || arg == "--config") {
            if (i + 1 < argc) {
                config.configFile = argv[++i];
            }
        }
        else if (arg == "--verbose") {
            config.verbose = true;
        }
    }

    return config;
}

/**
 * Signal handler for graceful shutdown
 */
void signalHandler(int signal) {
    std::cout << "\n[INFO] Received signal " << signal << std::endl;
    if (g_server) {
        g_server->stop();
    }
}

/**
 * Main entry point
 */
int main(int argc, char* argv[]) {
    // Parse command line arguments
    Config config = parseArguments(argc, argv);

    // Show help if requested
    if (config.showHelp) {
        printUsage(argv[0]);
        return 0;
    }

    // Show version if requested
    if (config.showVersion) {
        printVersion();
        return 0;
    }

    // Print banner
    std::cout << "========================================\n";
    std::cout << "   QFS Bootstrap Node v" << VERSION << "\n";
    std::cout << "   Provider Discovery Service\n";
    std::cout << "========================================\n";
    std::cout << "\n";

    // Print configuration
    std::cout << "[INFO] Configuration:\n";
    std::cout << "[INFO]   Host: " << config.host << "\n";
    std::cout << "[INFO]   Port: " << config.port << "\n";

    if (!config.configFile.empty()) {
        std::cout << "[INFO]   Config File: " << config.configFile << "\n";
    }

    if (config.verbose) {
        std::cout << "[INFO]   Verbose Logging: Enabled\n";
    }

    std::cout << "\n";

    try {
        // Create Bootstrap Server
        g_server = std::make_unique<qfs::BootstrapServer>(config.host, config.port);

        // Setup signal handlers for graceful shutdown
        std::signal(SIGINT, signalHandler);   // Ctrl+C
        std::signal(SIGTERM, signalHandler);  // Termination signal

        std::cout << "[INFO] Signal handlers registered (Ctrl+C for graceful shutdown)\n";
        std::cout << "\n";

        // Start the HTTP server (blocking call)
        bool success = g_server->start();

        if (!success) {
            std::cerr << "[ERROR] Failed to start Bootstrap Node server\n";
            return 1;
        }

    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "[ERROR] Unknown exception occurred\n";
        return 1;
    }

    std::cout << "[INFO] Bootstrap Node shutdown complete\n";
    std::cout << "[INFO] Goodbye!\n";

    return 0;
}
