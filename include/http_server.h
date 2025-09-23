#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include "storage_manager.h"
#include "async_db_manager.h"
#include <httplib.h>
#include <string>
#include <memory>
#include <thread>
#include <atomic>

class HttpServer {
private:
    httplib::Server server;
    std::unique_ptr<StorageManager> storageManager;
    std::unique_ptr<AsyncDatabaseManager> asyncDbManager;
    std::thread serverThread;
    int port;
    std::atomic<bool> isRunning;
    std::string host;

    // Route handlers
    void setupRoutes();
    void handleUpload(const httplib::Request& req, httplib::Response& res);
    void handleUpdate(const httplib::Request& req, httplib::Response& res);
    void handleDownload(const httplib::Request& req, httplib::Response& res);
    void handleChunkDownload(const httplib::Request& req, httplib::Response& res);
    void handleDelete(const httplib::Request& req, httplib::Response& res);
    void handleStatus(const httplib::Request& req, httplib::Response& res);
    void handleList(const httplib::Request& req, httplib::Response& res);

    // Utility methods
    void setResponseHeaders(httplib::Response& res);
    void sendJsonResponse(httplib::Response& res, int status,
                         const std::string& message, const std::string& data = "");
    void sendErrorResponse(httplib::Response& res, int status, const std::string& error);

    std::string extractFilenameFromRequest(const httplib::Request& req);
    bool validateUploadRequest(const httplib::Request& req, httplib::Response& res);
    bool handleMultipartUpload(const httplib::Request& req, std::string& filename, std::vector<uint8_t>& fileData);

public:
    HttpServer(int serverPort = 8001, const std::string& serverHost = "localhost");
    ~HttpServer();

    bool initialize();
    bool start();
    void stop();
    bool isServerRunning() const { return isRunning.load(); }

    int getPort() const { return port; }
    std::string getHost() const { return host; }
};

#endif // HTTP_SERVER_H