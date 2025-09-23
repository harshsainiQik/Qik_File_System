#include "../include/http_server.h"
#include "../include/logger.h"
#include "../include/utils.h"
#include "../include/config_manager.h"
#include <nlohmann-json/json.hpp>
#include <thread>
#include <chrono>
#include <fstream>
#include <sstream>

using json = nlohmann::json;

HttpServer::HttpServer(int serverPort, const std::string& serverHost)
    : port(serverPort), isRunning(false), host(serverHost) {

    g_logger.info("HttpServer created on " + host + ":" + std::to_string(port));
}

HttpServer::~HttpServer() {
    stop();
}

bool HttpServer::initialize() {
    try {
        g_logger.info("Initializing HttpServer...");

        // Initialize storage manager with resilience
        storageManager = std::make_unique<StorageManager>();

        // Try multiple times to initialize storage manager
        bool storageInitialized = false;
        for (int attempt = 1; attempt <= 3; attempt++) {
            g_logger.info("StorageManager initialization attempt " + std::to_string(attempt) + "/3");

            if (storageManager->initialize()) {
                storageInitialized = true;
                g_logger.info("StorageManager initialized successfully on attempt " + std::to_string(attempt));
                break;
            } else {
                g_logger.warning("StorageManager initialization failed on attempt " + std::to_string(attempt));
                if (attempt < 3) {
                    g_logger.info("Waiting before retry...");
                    std::this_thread::sleep_for(std::chrono::seconds(2));
                }
            }
        }

        if (!storageInitialized) {
            g_logger.error("StorageManager failed to initialize after 3 attempts");
            // CRITICAL: Don't fail server startup - allow server to run in degraded mode
            g_logger.warning("Server will start in DEGRADED MODE - some endpoints may be limited");
            // Reset storageManager to nullptr to prevent segfaults
            storageManager.reset();
        } else {
            // Initialize async database manager only if storage manager is ready
            asyncDbManager = std::make_unique<AsyncDatabaseManager>(
                std::shared_ptr<StorageManager>(storageManager.get(), [](StorageManager*){})
            );
            if (asyncDbManager->initialize()) {
                g_logger.info("AsyncDatabaseManager initialized successfully");
            } else {
                g_logger.error("AsyncDatabaseManager failed to initialize");
                asyncDbManager.reset();
            }
        }

        // Configure server timeouts using config values
        std::cout << "Configuring server timeouts..." << std::endl;
        server.set_read_timeout(CONFIG.getRequestTimeout() / 1000); // Convert ms to seconds
        server.set_write_timeout(CONFIG.getResponseTimeout() / 1000);
        server.set_payload_max_length(CONFIG.getMaxRequestSizeMB() * 1024 * 1024);

        std::cout << "Server timeouts configured successfully" << std::endl;
        // TODO: Fix global logger initialization
        // g_logger.info("Server timeouts configured - Read: " + std::to_string(CONFIG.getRequestTimeout()/1000) +
        //              "s, Write: " + std::to_string(CONFIG.getResponseTimeout()/1000) + "s");

        // Setup HTTP routes regardless of storage manager status
        std::cout << "Setting up HTTP routes..." << std::endl;
        setupRoutes();
        std::cout << "HTTP routes setup completed" << std::endl;

        std::cout << "HttpServer initialization completed successfully" <<
                     std::string(storageInitialized ? "" : " (DEGRADED MODE)") << std::endl;
        // TODO: Fix global logger initialization
        // g_logger.info("HttpServer initialized successfully" +
        //              std::string(storageInitialized ? "" : " (DEGRADED MODE)"));
        return true;

    } catch (const std::exception& e) {
        g_logger.error("Exception during HttpServer initialization: " + std::string(e.what()));
        return false;
    }
}

void HttpServer::setupRoutes() {
    try {
        std::cout << "Setting up CORS and OPTIONS handlers..." << std::endl;
        // Enable CORS for all routes
        server.set_pre_routing_handler([this](const httplib::Request& /*req*/, httplib::Response& res) {
            setResponseHeaders(res);
            return httplib::Server::HandlerResponse::Unhandled;
        });

        // Handle preflight OPTIONS requests
        server.Options(".*", [this](const httplib::Request& /*req*/, httplib::Response& res) {
            setResponseHeaders(res);
            res.status = 200;
        });

        std::cout << "Setting up endpoint handlers..." << std::endl;
        // File upload endpoint
        server.Post("/upload", [this](const httplib::Request& req, httplib::Response& res) {
            handleUpload(req, res);
        });

        // File update endpoint
        server.Put("/update/(.*)", [this](const httplib::Request& req, httplib::Response& res) {
            handleUpdate(req, res);
        });

        // File download endpoint
        server.Get("/download/(.*)", [this](const httplib::Request& req, httplib::Response& res) {
            handleDownload(req, res);
        });

        // Chunk download endpoint
        server.Get("/chunk/(.*)", [this](const httplib::Request& req, httplib::Response& res) {
            handleChunkDownload(req, res);
        });

        // File delete endpoint
        server.Delete("/delete/(.*)", [this](const httplib::Request& req, httplib::Response& res) {
            handleDelete(req, res);
        });

        // Server status endpoint
        server.Get("/status", [this](const httplib::Request& req, httplib::Response& res) {
            handleStatus(req, res);
        });

        // List files endpoint
        server.Get("/list", [this](const httplib::Request& req, httplib::Response& res) {
            handleList(req, res);
        });

        std::cout << "Setting up error handler..." << std::endl;
        // Default route for unsupported endpoints
        server.set_error_handler([this](const httplib::Request& req, httplib::Response& res) {
            setResponseHeaders(res);
            sendErrorResponse(res, 404, "Endpoint not found: " + req.path);
        });

        std::cout << "Routes setup complete, logging success..." << std::endl;
        // TODO: Fix global logger initialization
        // g_logger.info("HTTP routes configured successfully");

    } catch (const std::exception& e) {
        g_logger.error("Error setting up routes: " + std::string(e.what()));
    }
}

void HttpServer::handleUpload(const httplib::Request& req, httplib::Response& res) {
    try {
        g_logger.info("Handling file upload request");

        std::string filename;
        std::vector<uint8_t> fileData;

        // Check if this is a multipart form-data request
        auto contentType = req.get_header_value("Content-Type");
        if (contentType.find("multipart/form-data") != std::string::npos) {
            // Handle multipart form-data upload
            if (!handleMultipartUpload(req, filename, fileData)) {
                sendErrorResponse(res, 400, "Invalid multipart form-data or no file found");
                return;
            }
        } else {
            // Handle binary upload (legacy support)
            if (!validateUploadRequest(req, res)) {
                return;
            }

            // Get filename from request headers
            filename = extractFilenameFromRequest(req);
            if (filename.empty()) {
                sendErrorResponse(res, 400, "Filename not provided or invalid");
                return;
            }

            // Get file data from request body
            fileData = std::vector<uint8_t>(req.body.begin(), req.body.end());
        }

        if (fileData.empty()) {
            sendErrorResponse(res, 400, "No file data provided");
            return;
        }

        if (filename.empty()) {
            sendErrorResponse(res, 400, "No filename provided");
            return;
        }

        // Check file size limit (1GB)
        const size_t MAX_FILE_SIZE = 1024 * 1024 * 1024;
        if (fileData.size() > MAX_FILE_SIZE) {
            sendErrorResponse(res, 413, "File too large. Maximum size is " +
                            Utils::formatFileSize(MAX_FILE_SIZE));
            return;
        }

        // Check if storage manager is available (not in degraded mode)
        if (!storageManager) {
            g_logger.error("Storage manager not available - server running in degraded mode");
            sendErrorResponse(res, 503, "Storage service temporarily unavailable - server in degraded mode");
            return;
        }

        // Store file using storage manager with retry logic
        StorageManager::FileStoreResult storeResult;
        bool storeSuccess = false;

        // Try multiple times with increasing delays
        for (int attempt = 1; attempt <= 3; attempt++) {
            try {
                g_logger.info("File storage attempt " + std::to_string(attempt) + "/3 for: " + filename);

                storeResult = storageManager->storeFile(filename, fileData);
                if (storeResult.success) {
                    storeSuccess = true;
                    g_logger.info("File stored successfully on attempt " + std::to_string(attempt));
                    break;
                } else {
                    g_logger.warning("File storage failed on attempt " + std::to_string(attempt));
                    if (attempt < 3) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(500 * attempt));
                    }
                }
            } catch (const std::exception& e) {
                g_logger.error("Exception during file storage attempt " + std::to_string(attempt) + ": " + e.what());
                if (attempt < 3) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1000 * attempt));
                }
            }
        }

        if (!storeSuccess) {
            g_logger.error("Failed to store file after 3 attempts: " + filename);
            sendErrorResponse(res, 503, "Storage service temporarily unavailable. Please try again later.");
            return;
        }

        // Get both MIME type and extension
        std::string mimeType = Utils::detectMimeType(filename);
        std::string fileExtension = Utils::getFileExtension(filename);

        // Create response JSON
        json response;
        response["success"] = true;
        response["message"] = "File uploaded successfully";
        response["filename"] = filename;
        response["cid"] = storeResult.cid;
        response["chunks_count"] = storeResult.chunksCount;
        response["size"] = fileData.size();
        response["size_formatted"] = Utils::formatFileSize(fileData.size());
        response["file_type"] = fileExtension;
        response["mime_type"] = mimeType;
        response["upload_timestamp"] = Utils::getCurrentTimestamp();

        std::string responseJson = response.dump(2);

        res.set_content(responseJson, "application/json");
        res.status = 200;

        g_logger.info("File uploaded successfully: " + filename +
                     " (CID: " + storeResult.cid +
                     ", size: " + Utils::formatFileSize(fileData.size()) +
                     ", chunks: " + std::to_string(storeResult.chunksCount) +
                     ", type: " + fileExtension + ")");

    } catch (const std::exception& e) {
        g_logger.error("Exception in handleUpload: " + std::string(e.what()));
        sendErrorResponse(res, 500, "Internal server error during upload");
    }
}

void HttpServer::handleUpdate(const httplib::Request& req, httplib::Response& res) {
    try {
        // Extract CID from URL path
        std::string oldCid = req.matches[1];

        g_logger.info("Handling file update request for CID: " + oldCid);

        // Validate old CID
        if (!Utils::isValidCID(oldCid)) {
            sendErrorResponse(res, 400, "Invalid CID format");
            return;
        }

        // Check if old file exists using async DB manager
        if (!asyncDbManager) {
            g_logger.error("AsyncDatabaseManager not available for update operation");
            sendErrorResponse(res, 503, "Service temporarily unavailable");
            return;
        }

        auto existsFuture = asyncDbManager->asyncNodeExists(oldCid, CONFIG.getUpdateTimeout());
        auto existsResult = existsFuture.get();

        if (!existsResult.success) {
            g_logger.error("Failed to check if node exists: " + existsResult.error_message);
            sendErrorResponse(res, 500, "Database operation failed");
            return;
        }

        if (!existsResult.data) {
            sendErrorResponse(res, 404, "File not found for update");
            return;
        }

        std::string filename;
        std::vector<uint8_t> fileData;

        // Check if this is a multipart form-data request
        auto contentType = req.get_header_value("Content-Type");
        if (contentType.find("multipart/form-data") != std::string::npos) {
            // Handle multipart form-data upload
            if (!handleMultipartUpload(req, filename, fileData)) {
                sendErrorResponse(res, 400, "Invalid multipart form data");
                return;
            }
        } else {
            // Handle direct binary upload
            if (!validateUploadRequest(req, res)) {
                return;
            }

            // Extract filename from headers or use default
            filename = extractFilenameFromRequest(req);

            // Get file data from request body
            fileData = std::vector<uint8_t>(req.body.begin(), req.body.end());
        }

        // Validate file data
        if (fileData.empty()) {
            sendErrorResponse(res, 400, "No file data provided");
            return;
        }

        // Validate filename
        if (!Utils::isValidFilename(filename)) {
            sendErrorResponse(res, 400, "Invalid filename");
            return;
        }

        // Check file size limit (1GB)
        const size_t MAX_FILE_SIZE = 1024 * 1024 * 1024;
        if (fileData.size() > MAX_FILE_SIZE) {
            sendErrorResponse(res, 413, "File too large. Maximum size is " +
                            Utils::formatFileSize(MAX_FILE_SIZE));
            return;
        }

        // Update file using storage manager with timeout protection
        StorageManager::FileStoreResult updateResult;
        bool updateSuccess = false;

        // Timeout-protected update operation
        auto updateTimeoutMs = CONFIG.getUpdateTimeout();
        auto startTime = std::chrono::steady_clock::now();

        g_logger.info("Starting file update with " + std::to_string(updateTimeoutMs) + "ms timeout for: " + filename + " (old CID: " + oldCid + ")");

        try {
            // Use async update operation with timeout
            auto updateFuture = asyncDbManager->asyncUpdateFile(oldCid, filename, fileData, updateTimeoutMs);

            // Wait with timeout instead of blocking indefinitely
            auto timeout = std::chrono::milliseconds(updateTimeoutMs);
            auto status = updateFuture.wait_for(timeout);

            if (status == std::future_status::timeout) {
                auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - startTime).count();
                g_logger.error("Update operation timed out after " + std::to_string(elapsedMs) + "ms");
                sendErrorResponse(res, 504, "Update operation timed out");
                return;
            }

            auto asyncResult = updateFuture.get();
            auto elapsedMs = asyncResult.duration.count();

            if (!asyncResult.success) {
                g_logger.error("Async update operation failed: " + asyncResult.error_message);
                sendErrorResponse(res, 500, "Update operation failed: " + asyncResult.error_message);
                return;
            }

            updateResult = asyncResult.data;
            updateSuccess = updateResult.success;

            g_logger.info("File updated successfully in " + std::to_string(elapsedMs) + "ms");

        } catch (const std::exception& e) {
            auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - startTime).count();
            g_logger.error("Exception during async file update after " + std::to_string(elapsedMs) + "ms: " + e.what());
            sendErrorResponse(res, 500, "Internal server error during update");
            return;
        }

        if (!updateSuccess) {
            g_logger.error("Failed to update file: " + filename);
            sendErrorResponse(res, 503, "File update failed. Please try again later.");
            return;
        }

        // Get both MIME type and extension
        std::string mimeType = Utils::detectMimeType(filename);
        std::string fileExtension = Utils::getFileExtension(filename);

        // Create response JSON
        json response;
        response["success"] = true;
        response["message"] = "File updated successfully";
        response["filename"] = filename;
        response["old_cid"] = oldCid;
        response["new_cid"] = updateResult.cid;
        response["chunks_count"] = updateResult.chunksCount;
        response["size"] = fileData.size();
        response["size_formatted"] = Utils::formatFileSize(fileData.size());
        response["file_type"] = fileExtension;
        response["mime_type"] = mimeType;
        response["update_timestamp"] = Utils::getCurrentTimestamp();

        std::string responseJson = response.dump(2);

        setResponseHeaders(res);
        res.set_content(responseJson, "application/json");
        res.status = 200;

        g_logger.info("File updated successfully: " + filename +
                     " (old CID: " + oldCid +
                     ", new CID: " + updateResult.cid +
                     ", size: " + Utils::formatFileSize(fileData.size()) +
                     ", chunks: " + std::to_string(updateResult.chunksCount) +
                     ", type: " + fileExtension + ")");

    } catch (const std::exception& e) {
        g_logger.error("Exception in handleUpdate: " + std::string(e.what()));
        sendErrorResponse(res, 500, "Internal server error during update");
    }
}

void HttpServer::handleDownload(const httplib::Request& req, httplib::Response& res) {
    try {
        // Extract CID from URL path
        std::string cid = req.matches[1];

        g_logger.info("Handling download request for CID: " + cid);

        if (!Utils::isValidCID(cid)) {
            sendErrorResponse(res, 400, "Invalid CID format");
            return;
        }

        // Check if storage manager is available
        if (!storageManager) {
            g_logger.error("Storage manager not available - server running in degraded mode");
            sendErrorResponse(res, 503, "Storage service temporarily unavailable - server in degraded mode");
            return;
        }

        // Check if file exists
        if (!storageManager->nodeExists(cid)) {
            sendErrorResponse(res, 404, "File not found");
            return;
        }

        // Retrieve file data
        std::vector<uint8_t> fileData = storageManager->retrieveFile(cid);

        if (fileData.empty()) {
            sendErrorResponse(res, 500, "Failed to retrieve file data");
            return;
        }

        // Get file metadata
        auto rootNode = storageManager->getRootNode(cid);
        if (!rootNode) {
            sendErrorResponse(res, 500, "Failed to retrieve file metadata");
            return;
        }

        // Get the correct MIME type for the file
        std::string mimeType = Utils::detectMimeType(rootNode->getOriginalFilename());

        // Set response headers
        setResponseHeaders(res);
        res.set_header("Content-Type", mimeType);
        res.set_header("Content-Disposition",
                      "attachment; filename=\"" + rootNode->getOriginalFilename() + "\"");
        res.set_header("Content-Length", std::to_string(fileData.size()));

        // Set response body
        res.set_content(reinterpret_cast<const char*>(fileData.data()),
                       fileData.size(), mimeType);
        res.status = 200;

        g_logger.info("File downloaded successfully: " + rootNode->getOriginalFilename() +
                     " (size: " + Utils::formatFileSize(fileData.size()) + ")");

    } catch (const std::exception& e) {
        g_logger.error("Exception in handleDownload: " + std::string(e.what()));
        sendErrorResponse(res, 500, "Internal server error during download");
    }
}

void HttpServer::handleDelete(const httplib::Request& req, httplib::Response& res) {
    try {
        // Extract CID from URL path
        std::string cid = req.matches[1];

        g_logger.info("Handling delete request for CID: " + cid);

        if (!Utils::isValidCID(cid)) {
            sendErrorResponse(res, 400, "Invalid CID format");
            return;
        }

        // Check if async DB manager is available
        if (!asyncDbManager) {
            g_logger.error("AsyncDatabaseManager not available for delete operation");
            sendErrorResponse(res, 503, "Service temporarily unavailable");
            return;
        }

        auto deleteTimeoutMs = CONFIG.getDeleteTimeout();
        g_logger.info("Starting async file deletion with " + std::to_string(deleteTimeoutMs) + "ms timeout for CID: " + cid);

        try {
            // Check if file exists using async operation
            auto existsFuture = asyncDbManager->asyncNodeExists(cid, deleteTimeoutMs);
            auto existsResult = existsFuture.get();

            if (!existsResult.success) {
                g_logger.error("Failed to check if node exists: " + existsResult.error_message);
                sendErrorResponse(res, 500, "Database operation failed");
                return;
            }

            if (!existsResult.data) {
                sendErrorResponse(res, 404, "File not found");
                return;
            }

            // Get file info before deletion
            auto rootFuture = asyncDbManager->asyncGetRootNode(cid, deleteTimeoutMs);
            auto rootResult = rootFuture.get();
            std::string filename = "unknown";

            if (rootResult.success && rootResult.data) {
                filename = rootResult.data->getOriginalFilename();
            }

            // Delete file using async operation
            auto deleteFuture = asyncDbManager->asyncDeleteFile(cid, deleteTimeoutMs);
            auto deleteResult = deleteFuture.get();

            if (!deleteResult.success || !deleteResult.data) {
                g_logger.error("Async delete operation failed: " + deleteResult.error_message);
                sendErrorResponse(res, 500, "Failed to delete file: " + deleteResult.error_message);
                return;
            }

            auto elapsedMs = deleteResult.duration.count();
            g_logger.info("File deleted successfully in " + std::to_string(elapsedMs) + "ms");

            // Create response
            json response;
            response["success"] = true;
            response["message"] = "File deleted successfully";
            response["cid"] = cid;
            response["filename"] = filename;
            response["deleted_timestamp"] = Utils::getCurrentTimestamp();

            std::string responseJson = response.dump(2);

            setResponseHeaders(res);
            res.set_content(responseJson, "application/json");
            res.status = 200;

            g_logger.info("File deleted successfully: " + filename + " (CID: " + cid + ")");

        } catch (const std::exception& e) {
            g_logger.error("Exception during async delete operation: " + std::string(e.what()));
            sendErrorResponse(res, 500, "Failed to delete file: " + std::string(e.what()));
        }

    } catch (const std::exception& e) {
        g_logger.error("Exception in handleDelete: " + std::string(e.what()));
        sendErrorResponse(res, 500, "Internal server error during deletion");
    }
}

void HttpServer::handleStatus(const httplib::Request& /*req*/, httplib::Response& res) {
    try {
        g_logger.debug("Handling status request");

        // Create status response
        json response;
        response["service"] = "QFS (Qik File System)";
        response["version"] = "1.0.0";
        response["status"] = "running";
        response["uptime"] = "N/A"; // Could be implemented with start time tracking
        response["timestamp"] = Utils::getCurrentTimestamp();

        json storage;

        // Get storage statistics safely (check for null in degraded mode)
        if (storageManager) {
            try {
                auto stats = storageManager->getStorageStats();
                storage["total_files"] = stats.totalFiles;
                storage["total_chunks"] = stats.totalChunks;
                storage["total_size_bytes"] = stats.totalSizeBytes;
                storage["total_size_formatted"] = Utils::formatFileSize(stats.totalSizeBytes);
                storage["metadata_entries"] = stats.metadataEntries;
                response["mode"] = "normal";
            } catch (const std::exception& e) {
                storage["total_files"] = 0;
                storage["total_chunks"] = 0;
                storage["total_size_bytes"] = 0;
                storage["total_size_formatted"] = "0.00 B";
                storage["metadata_entries"] = 0;
                storage["error"] = "Storage manager error: " + std::string(e.what());
                response["mode"] = "degraded";
            }
        } else {
            storage["total_files"] = 0;
            storage["total_chunks"] = 0;
            storage["total_size_bytes"] = 0;
            storage["total_size_formatted"] = "0.00 B";
            storage["metadata_entries"] = 0;
            storage["error"] = "Storage manager not available";
            response["mode"] = "degraded";
        }

        response["storage"] = storage;

        json endpoints = json::array();
        endpoints.push_back("POST /upload - Upload a file");
        endpoints.push_back("PUT /update/{cid} - Update an existing file");
        endpoints.push_back("GET /download/{cid} - Download a file by CID");
        endpoints.push_back("GET /chunk/{cid} - Download individual chunk by CID");
        endpoints.push_back("DELETE /delete/{cid} - Delete a file by CID");
        endpoints.push_back("GET /status - Get server status");
        endpoints.push_back("GET /list - List all files");

        response["available_endpoints"] = endpoints;

        std::string responseJson = response.dump(2);

        res.set_content(responseJson, "application/json");
        res.status = 200;

    } catch (const std::exception& e) {
        g_logger.error("Exception in handleStatus: " + std::string(e.what()));
        sendErrorResponse(res, 500, "Internal server error");
    }
}

void HttpServer::handleList(const httplib::Request& /*req*/, httplib::Response& res) {
    try {
        g_logger.debug("Handling list files request");

        // Check if storage manager is available
        if (!storageManager) {
            g_logger.error("Storage manager not available - server running in degraded mode");
            sendErrorResponse(res, 503, "Storage service temporarily unavailable - server in degraded mode");
            return;
        }

        // Get list of all files
        std::vector<std::string> fileCIDs = storageManager->listAllFiles();

        json response;
        response["success"] = true;
        response["total_files"] = fileCIDs.size();
        response["timestamp"] = Utils::getCurrentTimestamp();

        json files = json::array();

        for (const auto& cid : fileCIDs) {
            auto rootNode = storageManager->getRootNode(cid);
            if (rootNode) {
                json fileInfo;
                fileInfo["cid"] = cid;
                fileInfo["filename"] = rootNode->getOriginalFilename();
                fileInfo["file_type"] = rootNode->getFileType();
                fileInfo["file_extension"] = rootNode->getFileExtension();
                fileInfo["size_bytes"] = rootNode->getTotalSize();
                fileInfo["size_formatted"] = Utils::formatFileSize(rootNode->getTotalSize());
                fileInfo["total_chunks"] = rootNode->getTotalChunks();
                fileInfo["upload_timestamp"] = rootNode->getUploadTimestamp();

                files.push_back(fileInfo);
            }
        }

        response["files"] = files;

        std::string responseJson = response.dump(2);

        res.set_content(responseJson, "application/json");
        res.status = 200;

        g_logger.debug("Listed " + std::to_string(fileCIDs.size()) + " files");

    } catch (const std::exception& e) {
        g_logger.error("Exception in handleList: " + std::string(e.what()));
        sendErrorResponse(res, 500, "Internal server error");
    }
}

void HttpServer::handleChunkDownload(const httplib::Request& req, httplib::Response& res) {
    try {
        // Extract chunk CID from URL path
        std::string chunkCID = req.matches[1];

        g_logger.info("Handling chunk download request for CID: " + chunkCID);

        if (!Utils::isValidCID(chunkCID)) {
            sendErrorResponse(res, 400, "Invalid chunk CID format");
            return;
        }

        // Check if async DB manager is available
        if (!asyncDbManager) {
            g_logger.error("AsyncDatabaseManager not available for chunk retrieval");
            sendErrorResponse(res, 503, "Service temporarily unavailable");
            return;
        }

        std::vector<uint8_t> chunkData;
        std::string mimeType = "application/octet-stream";
        auto chunkTimeoutMs = CONFIG.getChunkTimeout();

        g_logger.info("Starting async chunk retrieval with " + std::to_string(chunkTimeoutMs) + "ms timeout for CID: " + chunkCID);

        try {
            // Try to get as RawNode (chunk) first
            auto rawFuture = asyncDbManager->asyncGetRawNode(chunkCID, chunkTimeoutMs);
            auto rawResult = rawFuture.get();

            if (rawResult.success && rawResult.data) {
                chunkData = rawResult.data->getData();
                g_logger.debug("Found RawNode chunk: " + chunkCID +
                              " (size: " + Utils::formatFileSize(chunkData.size()) + ") in " + std::to_string(rawResult.duration.count()) + "ms");
            } else {
                // Check if this is a ProtoNode
                auto protoFuture = asyncDbManager->asyncGetProtoNode(chunkCID, chunkTimeoutMs);
                auto protoResult = protoFuture.get();

                if (protoResult.success && protoResult.data) {
                    sendErrorResponse(res, 400, "ProtoNode CID provided - use individual chunk CIDs for data download");
                    return;
                } else {
                    // Check if this is a RootNode
                    auto rootFuture = asyncDbManager->asyncGetRootNode(chunkCID, chunkTimeoutMs);
                    auto rootResult = rootFuture.get();

                    if (rootResult.success && rootResult.data) {
                        sendErrorResponse(res, 400, "RootNode CID provided - use /download/" + chunkCID + " for full file or individual chunk CIDs");
                        return;
                    } else {
                        sendErrorResponse(res, 404, "Chunk not found");
                        return;
                    }
                }
            }
        } catch (const std::exception& e) {
            g_logger.error("Exception during async chunk retrieval: " + std::string(e.what()));
            sendErrorResponse(res, 500, "Failed to retrieve chunk: " + std::string(e.what()));
            return;
        }

        if (chunkData.empty()) {
            sendErrorResponse(res, 404, "Chunk data is empty");
            return;
        }

        // Set response headers
        setResponseHeaders(res);
        res.set_header("Content-Type", mimeType);
        res.set_header("Content-Length", std::to_string(chunkData.size()));
        res.set_header("Content-Disposition", "attachment; filename=\"chunk_" + chunkCID + ".bin\"");

        // Set the response body with raw chunk data
        res.set_content(reinterpret_cast<const char*>(chunkData.data()),
                       chunkData.size(), mimeType);
        res.status = 200;

        g_logger.info("Chunk download successful: " + chunkCID +
                     " (size: " + Utils::formatFileSize(chunkData.size()) + ")");

    } catch (const std::exception& e) {
        g_logger.error("Exception in handleChunkDownload: " + std::string(e.what()));
        sendErrorResponse(res, 500, "Internal server error during chunk download");
    }
}

void HttpServer::setResponseHeaders(httplib::Response& res) {
    res.set_header("Access-Control-Allow-Origin", "*");
    res.set_header("Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS");
    res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization, X-Filename");
    res.set_header("Server", "QFS/1.0");
    res.set_header("Cache-Control", "no-cache");
}

void HttpServer::sendJsonResponse(httplib::Response& res, int status,
                                 const std::string& message, const std::string& data) {
    try {
        json response;
        response["success"] = (status >= 200 && status < 300);
        response["message"] = message;
        response["timestamp"] = Utils::getCurrentTimestamp();

        if (!data.empty()) {
            response["data"] = data;
        }

        std::string responseJson = response.dump(2);

        setResponseHeaders(res);
        res.set_content(responseJson, "application/json");
        res.status = status;

    } catch (const std::exception& e) {
        g_logger.error("Error creating JSON response: " + std::string(e.what()));
        res.set_content("{\"error\":\"Internal server error\"}", "application/json");
        res.status = 500;
    }
}

void HttpServer::sendErrorResponse(httplib::Response& res, int status, const std::string& error) {
    try {
        json response;
        response["success"] = false;
        response["error"] = error;
        response["timestamp"] = Utils::getCurrentTimestamp();

        std::string responseJson = response.dump(2);

        setResponseHeaders(res);
        res.set_content(responseJson, "application/json");
        res.status = status;

        g_logger.warning("Error response sent: " + std::to_string(status) + " - " + error);

    } catch (const std::exception& e) {
        g_logger.error("Error creating error response: " + std::string(e.what()));
        res.set_content("{\"error\":\"Internal server error\"}", "application/json");
        res.status = 500;
    }
}

bool HttpServer::handleMultipartUpload(const httplib::Request& req, std::string& filename, std::vector<uint8_t>& fileData) {
    try {
        // Check if we have multipart data
        if (req.form.files.empty()) {
            g_logger.warning("No files found in multipart request");
            return false;
        }

        // Look for a file with common form field names
        std::vector<std::string> possibleNames = {"file", "upload", "attachment", "document", "data"};

        httplib::FormData fileItem;
        bool fileFound = false;

        // Try to find a file with one of the common names
        for (const auto& name : possibleNames) {
            auto it = req.form.files.find(name);
            if (it != req.form.files.end()) {
                fileItem = it->second;
                fileFound = true;
                g_logger.debug("Found file in form field: " + name);
                break;
            }
        }

        // If not found with common names, take the first file
        if (!fileFound && !req.form.files.empty()) {
            fileItem = req.form.files.begin()->second;
            fileFound = true;
            g_logger.debug("Using first file found in form: " + req.form.files.begin()->first);
        }

        if (!fileFound) {
            g_logger.error("No file found in multipart form data");
            return false;
        }

        // Extract filename
        filename = fileItem.filename;
        if (filename.empty()) {
            g_logger.warning("No filename provided in multipart form data");
            return false;
        }

        // Validate filename
        if (!Utils::isValidFilename(filename)) {
            g_logger.error("Invalid filename: " + filename);
            return false;
        }

        // Extract file data
        fileData = std::vector<uint8_t>(fileItem.content.begin(), fileItem.content.end());

        if (fileData.empty()) {
            g_logger.error("Empty file content in multipart form data");
            return false;
        }

        g_logger.info("Successfully parsed multipart upload: " + filename +
                     " (size: " + Utils::formatFileSize(fileData.size()) + ")");

        return true;

    } catch (const std::exception& e) {
        g_logger.error("Exception in handleMultipartUpload: " + std::string(e.what()));
        return false;
    }
}

std::string HttpServer::extractFilenameFromRequest(const httplib::Request& req) {
    try {
        // Try to get filename from X-Filename header first
        auto it = req.headers.find("X-Filename");
        if (it != req.headers.end() && !it->second.empty()) {
            std::string filename = it->second;
            if (Utils::isValidFilename(filename)) {
                return filename;
            }
        }

        // Try to get from Content-Disposition header
        auto cd_it = req.headers.find("Content-Disposition");
        if (cd_it != req.headers.end()) {
            std::string cd = cd_it->second;
            size_t filenamePos = cd.find("filename=");
            if (filenamePos != std::string::npos) {
                size_t start = filenamePos + 9;
                if (start < cd.length() && cd[start] == '"') {
                    start++;
                }
                size_t end = cd.find('"', start);
                if (end == std::string::npos) {
                    end = cd.length();
                }
                std::string filename = cd.substr(start, end - start);
                if (Utils::isValidFilename(filename)) {
                    return filename;
                }
            }
        }

        // Default filename with timestamp
        return "upload_" + Utils::getCurrentTimestamp() + ".bin";

    } catch (const std::exception& e) {
        g_logger.error("Error extracting filename: " + std::string(e.what()));
        return "upload_default.bin";
    }
}

bool HttpServer::validateUploadRequest(const httplib::Request& req, httplib::Response& res) {
    try {
        // Check if request has body
        if (req.body.empty()) {
            sendErrorResponse(res, 400, "No file data provided");
            return false;
        }

        // Check content type (optional, allow any)
        // You could add specific content type validation here if needed

        return true;

    } catch (const std::exception& e) {
        g_logger.error("Error validating upload request: " + std::string(e.what()));
        sendErrorResponse(res, 500, "Internal server error during validation");
        return false;
    }
}

bool HttpServer::start() {
    try {
        if (isRunning.load()) {
            g_logger.warning("Server is already running");
            return true;
        }

        g_logger.info("Starting HTTP server on " + host + ":" + std::to_string(port));

        // Start server in a separate thread
        serverThread = std::thread([this]() {
            isRunning.store(true);

            if (!server.listen(host.c_str(), port)) {
                g_logger.error("Failed to start HTTP server");
                isRunning.store(false);
                return;
            }

            g_logger.info("HTTP server stopped");
            isRunning.store(false);
        });

        // Give server time to start
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        if (isRunning.load()) {
            g_logger.info("HTTP server started successfully on http://" +
                         host + ":" + std::to_string(port));
            return true;
        } else {
            g_logger.error("Failed to start HTTP server");
            return false;
        }

    } catch (const std::exception& e) {
        g_logger.error("Exception starting HTTP server: " + std::string(e.what()));
        return false;
    }
}

void HttpServer::stop() {
    try {
        if (!isRunning.load()) {
            return;
        }

        g_logger.info("Stopping HTTP server...");

        server.stop();
        isRunning.store(false);

        if (serverThread.joinable()) {
            serverThread.join();
        }

        g_logger.info("HTTP server stopped successfully");

    } catch (const std::exception& e) {
        g_logger.error("Exception stopping HTTP server: " + std::string(e.what()));
    }
}