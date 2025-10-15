/**
 * Bootstrap Server Implementation
 *
 * @file bootstrap_server.cpp
 * @author QFS Development Team
 * @date October 15, 2025
 */

#include "bootstrap_server.h"
#include <iostream>
#include <sstream>
#include <iomanip>

namespace qfs {

// Constructor
BootstrapServer::BootstrapServer(const std::string& host, int port)
    : host_(host)
    , port_(port)
    , running_(false)
{
    // Create HTTP server instance
    server_ = std::make_unique<httplib::Server>();

    // Configure server settings
    server_->set_read_timeout(30, 0);   // 30 seconds
    server_->set_write_timeout(30, 0);  // 30 seconds

    // Setup all HTTP routes
    setupRoutes();

    std::cout << "[INFO] BootstrapServer initialized" << std::endl;
    std::cout << "[INFO]   Host: " << host_ << std::endl;
    std::cout << "[INFO]   Port: " << port_ << std::endl;
}

// Destructor
BootstrapServer::~BootstrapServer() {
    if (running_) {
        stop();
    }
    std::cout << "[INFO] BootstrapServer destroyed" << std::endl;
}

// Setup all HTTP routes
void BootstrapServer::setupRoutes() {
    std::cout << "[INFO] Setting up HTTP endpoints..." << std::endl;

    // GET /health - Health check endpoint
    server_->Get("/health", [this](const httplib::Request& req, httplib::Response& res) {
        handleHealth(req, res);
        logRequest(req, res);
    });

    std::cout << "[INFO]   GET  /health - Health check" << std::endl;

    // Future endpoints will be added here:
    // server_->Post("/announce", [...]);
    // server_->Get("/discover/:cid", [...]);

    // 404 Not Found handler
    server_->set_error_handler([this](const httplib::Request& req, httplib::Response& res) {
        if (res.status == 404) {
            handleNotFound(req, res);
        } else if (res.status >= 500) {
            handleError(req, res);
        }
        logRequest(req, res);
    });

    std::cout << "[INFO] HTTP endpoints configured successfully" << std::endl;
}

// Handle GET /health
void BootstrapServer::handleHealth(const httplib::Request& /* req */, httplib::Response& res) {
    // Calculate uptime
    uint64_t uptime = getUptime();

    // Build JSON response
    std::ostringstream json;
    json << "{\n";
    json << "  \"status\": \"ok\",\n";
    json << "  \"version\": \"1.0.0\",\n";
    json << "  \"uptime\": " << uptime << ",\n";
    json << "  \"service\": \"QFS Bootstrap Node\",\n";
    json << "  \"endpoints\": {\n";
    json << "    \"health\": \"/health\",\n";
    json << "    \"announce\": \"/announce (coming soon)\",\n";
    json << "    \"discover\": \"/discover/:cid (coming soon)\"\n";
    json << "  }\n";
    json << "}";

    // Set response
    res.set_content(json.str(), "application/json");
    res.status = 200;
}

// Handle 404 Not Found
void BootstrapServer::handleNotFound(const httplib::Request& req, httplib::Response& res) {
    std::ostringstream json;
    json << "{\n";
    json << "  \"error\": \"Not Found\",\n";
    json << "  \"message\": \"The requested endpoint does not exist\",\n";
    json << "  \"path\": \"" << req.path << "\",\n";
    json << "  \"method\": \"" << req.method << "\"\n";
    json << "}";

    res.set_content(json.str(), "application/json");
    res.status = 404;
}

// Handle 500 Internal Server Error
void BootstrapServer::handleError(const httplib::Request& req, httplib::Response& res) {
    std::ostringstream json;
    json << "{\n";
    json << "  \"error\": \"Internal Server Error\",\n";
    json << "  \"message\": \"An unexpected error occurred\",\n";
    json << "  \"path\": \"" << req.path << "\"\n";
    json << "}";

    res.set_content(json.str(), "application/json");
    res.status = 500;
}

// Log HTTP requests
void BootstrapServer::logRequest(const httplib::Request& req, const httplib::Response& res) {
    // Get current timestamp
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);

    // Format: [2025-10-15 12:30:45] GET /health - 200 OK
    std::cout << "[" << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S") << "] "
              << req.method << " " << req.path
              << " - " << res.status;

    // Add status text
    if (res.status == 200) {
        std::cout << " OK";
    } else if (res.status == 404) {
        std::cout << " Not Found";
    } else if (res.status >= 500) {
        std::cout << " Server Error";
    }

    std::cout << std::endl;
}

// Start the HTTP server
bool BootstrapServer::start() {
    if (running_) {
        std::cerr << "[ERROR] Server is already running" << std::endl;
        return false;
    }

    std::cout << "\n========================================" << std::endl;
    std::cout << "  Starting Bootstrap Node HTTP Server" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "[INFO] Listening on http://" << host_ << ":" << port_ << std::endl;
    std::cout << "[INFO] Press Ctrl+C to stop the server" << std::endl;
    std::cout << "========================================\n" << std::endl;

    // Record start time
    startTime_ = std::chrono::steady_clock::now();
    running_ = true;

    // Start listening (blocking call)
    bool success = server_->listen(host_.c_str(), port_);

    running_ = false;

    if (!success) {
        std::cerr << "[ERROR] Failed to start server on " << host_ << ":" << port_ << std::endl;
        std::cerr << "[ERROR] Port may already be in use" << std::endl;
        return false;
    }

    std::cout << "[INFO] Server stopped" << std::endl;
    return true;
}

// Stop the HTTP server
void BootstrapServer::stop() {
    if (!running_) {
        return;
    }

    std::cout << "\n[INFO] Shutdown signal received" << std::endl;
    std::cout << "[INFO] Stopping HTTP server..." << std::endl;

    server_->stop();
    running_ = false;

    std::cout << "[INFO] Server stopped successfully" << std::endl;
}

// Check if server is running
bool BootstrapServer::isRunning() const {
    return running_;
}

// Get server uptime
uint64_t BootstrapServer::getUptime() const {
    if (!running_) {
        return 0;
    }

    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - startTime_);
    return static_cast<uint64_t>(duration.count());
}

} // namespace qfs
