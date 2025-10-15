/**
 * Bootstrap Server - HTTP server for QFS provider discovery
 *
 * This class implements an HTTP server that handles provider registration
 * and discovery requests from QFS nodes. It provides a simple REST API
 * for the peer-to-peer network coordination.
 *
 * @file bootstrap_server.h
 * @author QFS Development Team
 * @date October 15, 2025
 * @version 1.0.0
 */

#pragma once

#include <string>
#include <memory>
#include <atomic>
#include <chrono>
#include "../third_party/cpp-httplib/httplib.h"

namespace qfs {

/**
 * BootstrapServer class
 *
 * Manages the HTTP server for Bootstrap Node, handling provider
 * registration and discovery requests.
 */
class BootstrapServer {
public:
    /**
     * Constructor
     *
     * @param host Host address to bind to (e.g., "0.0.0.0", "localhost")
     * @param port Port number to listen on (e.g., 8090)
     */
    BootstrapServer(const std::string& host, int port);

    /**
     * Destructor
     *
     * Ensures proper cleanup of server resources
     */
    ~BootstrapServer();

    /**
     * Start the HTTP server
     *
     * This is a blocking call that runs until stop() is called or
     * the server encounters an error.
     *
     * @return true if server started successfully, false otherwise
     */
    bool start();

    /**
     * Stop the HTTP server
     *
     * Gracefully shuts down the server. Can be called from signal
     * handlers for clean shutdown.
     */
    void stop();

    /**
     * Check if server is running
     *
     * @return true if server is currently running, false otherwise
     */
    bool isRunning() const;

    /**
     * Get server uptime in seconds
     *
     * @return Number of seconds since server started
     */
    uint64_t getUptime() const;

private:
    /**
     * Setup all HTTP routes/endpoints
     *
     * Registers handlers for:
     * - GET /health - Health check endpoint
     * - POST /announce - Provider registration (future)
     * - GET /discover/:cid - Provider discovery (future)
     */
    void setupRoutes();

    /**
     * Handle GET /health requests
     *
     * Returns server status, version, and uptime information.
     * Used for health checks and monitoring.
     *
     * @param req HTTP request object
     * @param res HTTP response object
     */
    void handleHealth(const httplib::Request& req, httplib::Response& res);

    /**
     * Handle 404 Not Found errors
     *
     * Returns a friendly error message for unknown endpoints.
     *
     * @param req HTTP request object
     * @param res HTTP response object
     */
    void handleNotFound(const httplib::Request& req, httplib::Response& res);

    /**
     * Handle 500 Internal Server Error
     *
     * Returns error information when server encounters exceptions.
     *
     * @param req HTTP request object
     * @param res HTTP response object
     */
    void handleError(const httplib::Request& req, httplib::Response& res);

    /**
     * Log incoming HTTP requests
     *
     * @param req HTTP request object
     * @param res HTTP response object
     */
    void logRequest(const httplib::Request& req, const httplib::Response& res);

private:
    std::string host_;                              // Host address
    int port_;                                      // Port number
    std::unique_ptr<httplib::Server> server_;       // HTTP server instance
    std::atomic<bool> running_;                     // Server running state
    std::chrono::steady_clock::time_point startTime_; // Server start time
};

} // namespace qfs
