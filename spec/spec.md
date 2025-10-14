# QFS (Qik File System) – Complete Project Specification

## 1. Executive Summary

QFS (Qik File System) is a production-ready, content-addressed storage microservice implemented in C++ that provides IPFS-inspired functionality through a RESTful HTTP API. The system handles file storage with automatic chunking, content deduplication, atomic operations, and comprehensive error handling.

**Key Features:**
- Content-addressed storage with SHA-256 based CIDs
- Automatic file chunking (256KB default)
- Merkle DAG structure for efficient storage
- Complete CRUD operations (Create, Read, Update, Delete)
- Asynchronous database operations with timeout protection
- Reference counting for automatic garbage collection
- Production-ready error handling and logging
- Configurable through JSON configuration files

### 1.1 Purpose and Scope

**In-scope:**
- Single-node content-addressed storage microservice
- RESTful HTTP API for file lifecycle operations
- Persistent storage using LevelDB (metadata) and filesystem (chunks)
- Automatic chunking and Merkle DAG construction
- Reference counting and garbage collection
- Asynchronous operations with timeout protection
- Comprehensive logging and error handling
- Configuration management and validation
- Production-ready deployment capabilities

**Out-of-scope (current version):**
- P2P networking and distributed storage
- User authentication and authorization
- Encryption at rest
- Multi-node replication
- Advanced garbage collection algorithms

## 2. System Architecture

### 2.1 High-Level Architecture

```
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│   HTTP Client   │    │   HTTP Server   │    │ Storage Manager │
│                 │◄──►│                 │◄──►│                 │
│ - Upload        │    │ - Route Handler │    │ - File Storage  │
│ - Download      │    │ - CORS/Headers  │    │ - DAG Assembly  │
│ - Update        │    │ - Validation    │    │ - Chunk Mgmt    │
│ - Delete        │    │ - Async Ops     │    │ - Reference Cnt │
└─────────────────┘    └─────────────────┘    └─────────────────┘
                                 │                        │
                                 │                        │
                       ┌─────────────────┐    ┌─────────────────┐
                       │Async DB Manager │    │   Data Layer    │
                       │                 │    │                 │
                       │ - Worker Threads│    │ - LevelDB       │
                       │ - Task Queue    │    │ - Block Store   │
                       │ - Timeouts      │    │ - File System   │
                       └─────────────────┘    └─────────────────┘
```

### 2.2 Core Components

**Layer 1: HTTP API Layer**
- **HttpServer**: RESTful API endpoints, request routing, response handling
- **Request Validation**: Input sanitization, file size limits, CID validation
- **CORS Support**: Cross-origin resource sharing configuration
- **Multipart Support**: File upload handling (multipart/form-data and binary)

**Layer 2: Business Logic Layer**
- **StorageManager**: Core business logic for file operations
- **AsyncDatabaseManager**: Asynchronous database operations with timeout protection
- **ConfigManager**: Centralized configuration management
- **InstanceLock**: Single-instance enforcement

**Layer 3: Data Access Layer**
- **DAG Nodes**: RootNode, ProtoNode, RawNode implementations
- **Chunker**: File chunking and reassembly logic
- **LevelDB Integration**: Metadata persistence
- **Block Store**: Chunk file storage with sharding

**Layer 4: Utility Layer**
- **Logger**: Thread-safe logging with rotation
- **Utils**: Hashing, timestamps, file operations
- **Error Handling**: Comprehensive error management

### 2.3 Data Flow Diagrams

**Upload Flow:**
```
Client → HTTP Server → Storage Manager → Chunker → RawNodes → ProtoNodes → RootNode → LevelDB + BlockStore → CID Response
```

**Download Flow:**
```
Client → HTTP Server → Storage Manager → LevelDB Lookup → DAG Traversal → Chunk Assembly → Binary Response
```

**Update Flow (NEW - Simplified DELETE + UPLOAD):**
```
Client → HTTP Server → Storage Manager → Delete Old File → Upload New File → New CID Response
```

**Delete Flow:**
```
Client → HTTP Server → Storage Manager → DAG Traversal → Chunk Deletion (Disk) → Metadata Deletion (LevelDB) → Success Response
```

## 3. Detailed Component Specifications

### 3.1 HttpServer (`include/http_server.h`, `src/http_server.cpp`)

**Purpose**: RESTful HTTP API layer handling all client interactions

**Key Features:**
- **Endpoints**:
  - `POST /upload` - File upload with multipart/form-data or binary support
  - `PUT /update/{cid}` - File update using DELETE + UPLOAD strategy
  - `GET /download/{cid}` - File download with streaming support
  - `GET /chunk/{cid}` - Individual chunk download
  - `DELETE /delete/{cid}` - File deletion with complete cleanup
  - `GET /status` - Server health and statistics
  - `GET /list` - List all stored files
  - `OPTIONS /*` - CORS preflight handling

**Implementation Details:**
- **Server Framework**: cpp-httplib header-only HTTP server
- **Request Handling**: Async request processing with timeout protection
- **CORS Support**: Configurable cross-origin resource sharing
- **Content Types**: JSON responses, binary file streaming, multipart form handling
- **Error Handling**: Standardized JSON error responses with HTTP status codes
- **Validation**: File size limits, CID format validation, content type checking
- **Security**: Request size limits, timeout protection, input sanitization

**Configuration Dependencies:**
- Server host/port settings
- Request/response timeouts
- Maximum request size limits
- CORS configuration

### 3.2 StorageManager (`include/storage_manager.h`, `src/storage_manager.cpp`)

**Purpose**: Core business logic for file storage operations and DAG management

**Key Features:**
- **File Operations**: Complete CRUD functionality with atomic operations
- **DAG Management**: Merkle DAG construction and traversal
- **Chunk Storage**: Efficient chunk storage with sharding (256 directories)
- **Reference Counting**: Automatic garbage collection through reference counting
- **Concurrency**: Thread-safe operations with mutex protection
- **Error Recovery**: Comprehensive error handling and recovery mechanisms

**Directory Structure:**
```
data/
├── blocks/           # Chunk storage (sharded by CID prefix)
│   ├── 00/          # Shard directory
│   ├── 01/
│   └── ...
│   └── ff/
├── leveldb/         # LevelDB database files
└── logs/           # Application log files
```

**Operations:**
- `storeFile()`: File upload with chunking and DAG construction
- `updateFile()`: File update using DELETE + UPLOAD strategy (NEW)
- `retrieveFile()`: File download with chunk reassembly
- `deleteFile()`: Complete file deletion including chunks and metadata
- `nodeExists()`: Check if file/node exists
- `listAllFiles()`: Get all stored file CIDs
- `getStorageStats()`: Storage utilization statistics

**Data Persistence:**
- **LevelDB Keys**:
  - `root:{cid}` → RootNode JSON
  - `proto:{cid}` → ProtoNode JSON
  - `raw:{cid}` → RawNode JSON
  - `type:{cid}` → Node type indicator
- **Block Store**: `{shard}/{cid}_chunk_{index}` files

### 3.3 AsyncDatabaseManager (`include/async_db_manager.h`, `src/async_db_manager.cpp`)

**Purpose**: Asynchronous database operations with timeout protection

**Key Features:**
- **Worker Thread Pool**: Configurable number of worker threads
- **Task Queue**: Thread-safe task queuing system
- **Timeout Protection**: Per-operation timeout enforcement
- **Future-based API**: Modern C++ async programming patterns
- **Error Handling**: Comprehensive async error management

**Async Operations:**
- `asyncNodeExists()`: Non-blocking node existence check
- `asyncGetRootNode()`: Async root node retrieval
- `asyncGetProtoNode()`: Async proto node retrieval
- `asyncGetRawNode()`: Async raw node retrieval
- `asyncUpdateFile()`: Async file update operations
- `asyncDeleteFile()`: Async file deletion operations

**Implementation:**
- Uses `std::future` and `std::promise` for async results
- Worker threads process tasks from thread-safe queue
- Timeout monitoring prevents hanging operations
- Graceful shutdown with task cleanup

### 3.4 DAG Nodes (`include/dag_node.h`, `src/dag_node.cpp`)

**Purpose**: Data structure implementations for Merkle DAG

**Node Types:**

**RootNode:**
- **Purpose**: File-level metadata container
- **Content**: Filename, file type, total size, chunk count, checksum
- **Links**: Direct links to RawNodes (≤160) or ProtoNodes (>160)
- **Metadata**: Upload timestamp, pin status, file extension
- **Serialization**: JSON format for LevelDB storage

**ProtoNode:**
- **Purpose**: Intermediate aggregation node for large files
- **Capacity**: Up to 160 child links (MAX_CHILDREN = 160)
- **Links**: References to RawNode CIDs
- **Use Case**: Files with >160 chunks require ProtoNode hierarchy

**RawNode:**
- **Purpose**: Leaf node containing actual chunk data
- **Content**: Binary chunk data, checksum, chunk index
- **Features**: Reference counting for deduplication
- **Storage**: Metadata in LevelDB, data in block store
- **Lifecycle**: Created on upload, ref-counted, GC on zero refs

**Common Features:**
- **CID Generation**: SHA-256 based content identifiers
- **JSON Serialization**: All nodes support JSON (de)serialization
- **Type Safety**: Strong typing with enum-based node types
- **Validation**: Content integrity through checksums

### 3.5 Chunker (`include/chunk.h`, `src/chunk.cpp`)

**Purpose**: File chunking and reassembly logic

**Features:**
- **Fixed-Size Chunking**: Default 256KB (262,144 bytes) chunks
- **Checksum Generation**: SHA-256 checksum per chunk
- **Reassembly**: Ordered chunk reassembly for file reconstruction
- **Validation**: Checksum validation during reassembly
- **Metadata**: ChunkData structure with index and type information

**Chunking Algorithm:**
1. Split file into fixed-size chunks (last chunk may be smaller)
2. Generate SHA-256 checksum for each chunk
3. Create ChunkData structures with metadata
4. Return vector of ChunkData for storage

### 3.6 ConfigManager (`include/config_manager.h`, `src/config_manager.cpp`)

**Purpose**: Centralized configuration management with comprehensive settings

**Implementation:**
- **Singleton Pattern**: Single instance for application-wide access
- **JSON-based**: Uses nlohmann-json for configuration parsing
- **Type Safety**: Strongly typed getter methods
- **Default Values**: Fallback defaults for missing configuration
- **Validation**: Configuration validation and error reporting

**Configuration Categories:**

**Server Configuration:**
- Host/port settings
- Connection limits and timeouts
- CORS settings
- Request size limits

**Storage Configuration:**
- Data directory paths
- Chunk size settings
- File size limits
- Compression options
- Deduplication settings

**Database Configuration:**
- LevelDB tuning parameters
- Write buffer sizes
- Cache settings
- Compression options

**Performance Configuration:**
- Worker thread counts
- Memory cache settings
- Operation timeouts
- Connection pooling

**Security Configuration:**
- Authentication settings
- Rate limiting
- File type restrictions
- API key management

**Logging Configuration:**
- Log levels and destinations
- File rotation settings
- Console output options
- JSON logging format

### 3.7 Logger (`include/logger.h`, `src/logger.cpp`)

**Purpose**: Thread-safe logging system with rotation and multiple outputs

**Features:**
- **Thread Safety**: Mutex-protected logging operations
- **Multiple Outputs**: File and console logging
- **Log Rotation**: Size-based log file rotation
- **Log Levels**: DEBUG, INFO, WARNING, ERROR, CRITICAL
- **Formatting**: Timestamp, level, and message formatting
- **Performance**: Efficient logging with minimal overhead

### 3.8 Utils (`include/utils.h`, `src/utils.cpp`)

**Purpose**: Common utility functions for the application

**Functions:**
- **Hashing**: SHA-256 calculation for CID generation
- **File Operations**: File reading/writing, existence checks
- **Time Utilities**: Timestamp generation and formatting
- **CID Operations**: CID validation and generation
- **MIME Detection**: File type detection based on content/extension
- **Size Formatting**: Human-readable file size formatting

### 3.9 InstanceLock (`include/instance_lock.h`, `src/instance_lock.cpp`)

**Purpose**: Prevent multiple QFS instances from running simultaneously

**Implementation:**
- **Platform-specific**: Different implementations for Windows/Unix
- **Lock File**: Uses lock files to prevent concurrent access
- **Automatic Cleanup**: Releases locks on application termination
- **Error Handling**: Graceful handling of lock acquisition failures

## 4. External Dependencies

### 4.1 Core Dependencies

**OpenSSL**
- **Purpose**: Cryptographic hashing operations
- **Usage**: SHA-256 calculation for CID generation and chunk checksums
- **Version**: Compatible with OpenSSL 1.1.x and 3.x
- **Location**: `third_party/openssl/`

**LevelDB**
- **Purpose**: High-performance key-value store for metadata persistence
- **Usage**: Storage of DAG node metadata, file indices, and system state
- **Features**: Snappy compression, configurable cache sizes, atomic batch operations
- **Location**: `third_party/leveldb/`

**nlohmann-json**
- **Purpose**: Modern C++ JSON library for configuration and serialization
- **Usage**: Configuration file parsing, DAG node serialization, API responses
- **Version**: v3.x compatible
- **Location**: `third_party/nlohmann-json/`

**cpp-httplib**
- **Purpose**: Header-only HTTP server library
- **Usage**: RESTful API implementation, request routing, response handling
- **Features**: CORS support, multipart form handling, streaming responses
- **Location**: `third_party/cpp-httplib/`

### 4.2 Build Dependencies

**CMake**
- **Minimum Version**: 3.15
- **Purpose**: Cross-platform build system
- **Configuration**: `CMakeLists.txt` with dependency management

**C++ Compiler**
- **Standard**: C++17 or later
- **Compilers**: GCC 7+, Clang 5+, MSVC 2019+
- **Features**: std::filesystem, std::future, std::shared_ptr

### 4.3 Platform Dependencies

**Windows**
- **Winsock**: Network socket operations
- **Windows API**: File system operations, process management
- **Preprocessor**: `_WIN32_WINNT=0x0A00` for Windows 10 compatibility

**Unix/Linux**
- **POSIX**: Standard file system and threading operations
- **pthread**: Thread management and synchronization
- **Standard libraries**: File I/O, networking, memory management

### 4.4 Development Dependencies

**Testing Framework** (Optional)
- **Google Test**: Unit testing framework
- **Location**: `third_party/googletest/`

**Code Quality Tools**
- **Static Analysis**: Clang-tidy, CppCheck
- **Formatting**: clang-format
- **Documentation**: Doxygen

All dependencies are managed through the CMake build system and are expected to be available under `third_party/` directory structure.

## 5. Build and Deployment

### 5.1 Build Requirements

**Prerequisites:**
- **C++17 Standard**: Required for std::filesystem and modern C++ features
- **CMake**: Minimum version 3.15 for advanced target management
- **Git**: For dependency management and version control
- **Platform-specific tools**: MSVC on Windows, GCC/Clang on Unix/Linux

### 5.2 Build Process

**Unix/Linux Build:**
```bash
# Clone repository
git clone <repository-url>
cd Qik_file_system

# Create build directory
mkdir build && cd build

# Configure with CMake
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build executable
make -j$(nproc)

# Optional: Run tests
make test
```

**Windows Build:**
```powershell
# Clone repository
git clone <repository-url>
cd Qik_file_system

# Create build directory
mkdir build
cd build

# Configure with CMake (Visual Studio 2019)
cmake .. -G "Visual Studio 16 2019" -A x64

# Build executable
cmake --build . --config Release

# Optional: Run tests
ctest -C Release
```

**Alternative Build with Ninja:**
```bash
# Configure with Ninja generator
cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release

# Build with Ninja
ninja
```

### 5.3 Build Targets

**Primary Targets:**
- **qfs**: Main executable
- **qfs_tests**: Unit test executable (if testing enabled)
- **install**: Installation target for deployment

**Build Configuration Options:**
```cmake
# Debug build with symbols
cmake .. -DCMAKE_BUILD_TYPE=Debug

# Release build with optimizations
cmake .. -DCMAKE_BUILD_TYPE=Release

# Enable testing
cmake .. -DENABLE_TESTING=ON

# Custom install prefix
cmake .. -DCMAKE_INSTALL_PREFIX=/opt/qfs
```

### 5.4 Runtime Environment

**Directory Structure (Auto-created):**
```
build/
├── qfs.exe                 # Main executable
├── data/                   # Data directory (auto-created)
│   ├── blocks/            # Chunk storage (256 subdirectories)
│   │   ├── 00/
│   │   ├── 01/
│   │   └── ...
│   │   └── ff/
│   ├── leveldb/           # LevelDB database files
│   └── logs/              # Application log files
│       └── qfs.log
├── config/                # Configuration directory
│   └── config.json       # Configuration file (optional)
└── third_party/          # Dependencies
```

**Environment Variables:**
- `QFS_CONFIG_PATH`: Override default config file location
- `QFS_DATA_DIR`: Override default data directory
- `QFS_LOG_LEVEL`: Override log level (DEBUG, INFO, WARN, ERROR)

### 5.5 Running the Service

**Basic Startup:**
```bash
# Run with default configuration
./qfs.exe

# Run with custom config
./qfs.exe --config=/path/to/config.json

# Run with specific data directory
./qfs.exe --data-dir=/custom/data/path
```

**Docker Deployment:**
```dockerfile
FROM ubuntu:20.04

# Install runtime dependencies
RUN apt-get update && apt-get install -y \
    libssl1.1 \
    && rm -rf /var/lib/apt/lists/*

# Copy executable and configuration
COPY qfs /usr/local/bin/
COPY config/ /etc/qfs/

# Create data directory
VOLUME ["/var/lib/qfs"]

# Expose API port
EXPOSE 8080

# Run QFS
CMD ["qfs", "--config=/etc/qfs/config.json", "--data-dir=/var/lib/qfs"]
```

**Service Configuration (systemd):**
```ini
[Unit]
Description=QFS (Qik File System) Service
After=network.target

[Service]
Type=simple
User=qfs
Group=qfs
ExecStart=/usr/local/bin/qfs --config=/etc/qfs/config.json
Restart=always
RestartSec=10

[Install]
WantedBy=multi-user.target
```

## 6. Configuration Management

### 6.1 Configuration File Structure (config/config.json)

The QFS microservice uses a comprehensive JSON configuration file with the following structure:

```json
{
  "server": {
    "host": "0.0.0.0",
    "port": 8080,
    "max_connections": 100,
    "request_timeout": 30000,
    "response_timeout": 30000,
    "max_request_size_mb": 100,
    "cors": {
      "enabled": true,
      "allowed_origins": ["*"],
      "allowed_methods": ["GET", "POST", "PUT", "DELETE", "OPTIONS"],
      "allowed_headers": ["*"]
    }
  },
  "storage": {
    "data_directory": "data",
    "chunk_size_bytes": 262144,
    "max_file_size_mb": 1024,
    "compression": {
      "enabled": false,
      "level": 6,
      "type": "gzip"
    },
    "shard_count": 256,
    "deduplication": {
      "enabled": true,
      "cleanup_interval_hours": 24
    }
  },
  "database": {
    "leveldb": {
      "write_buffer_size_mb": 16,
      "max_file_size_mb": 64,
      "compression_type": "snappy",
      "paranoid_checks": false,
      "block_cache_size_mb": 32,
      "bloom_filter_bits": 10,
      "max_open_files": 1000,
      "sync_writes": false
    },
    "connection_pool": {
      "max_connections": 10,
      "min_connections": 2,
      "connection_timeout": 5000,
      "idle_timeout": 300000,
      "max_retries": 3,
      "retry_delay": 1000
    }
  },
  "logging": {
    "level": "INFO",
    "file_path": "logs/qfs.log",
    "max_file_size_mb": 100,
    "max_files": 10,
    "console": {
      "enabled": true,
      "colored": true
    },
    "json_format": false,
    "verbose": false
  },
  "security": {
    "authentication": {
      "enabled": false,
      "api_key_required": false
    },
    "rate_limiting": {
      "enabled": false,
      "requests_per_minute": 60,
      "burst_size": 10
    },
    "file_restrictions": {
      "allowed_extensions": [],
      "blocked_extensions": [".exe", ".bat", ".sh"],
      "max_filename_length": 255
    }
  },
  "performance": {
    "threads": {
      "worker_threads": 4,
      "io_threads": 2,
      "thread_pool_enabled": true
    },
    "cache": {
      "memory_cache_enabled": false,
      "cache_size_mb": 256,
      "cache_ttl_minutes": 60
    },
    "timeouts": {
      "upload_timeout": 600000,
      "download_timeout": 300000,
      "update_timeout": 600000,
      "delete_timeout": 300000,
      "chunk_timeout": 30000,
      "list_timeout": 10000
    }
  },
  "monitoring": {
    "metrics": {
      "enabled": false,
      "endpoint": "/metrics"
    },
    "health_check": {
      "interval": 30,
      "storage_usage_alert": 90,
      "memory_usage_alert": 80
    }
  },
  "development": {
    "debug_mode": false,
    "verbose_logging": false,
    "test_data_path": "test/data"
  },
  "version": "1.0.0",
  "environment": "production"
}
```

### 6.2 Configuration Categories

**Server Configuration (`server`):**
- Network binding and connection management
- Request/response timeouts and size limits
- CORS (Cross-Origin Resource Sharing) settings
- Connection pooling and concurrency limits

**Storage Configuration (`storage`):**
- Data directory locations and structure
- File chunking parameters (chunk size, max file size)
- Compression settings and algorithms
- Deduplication and cleanup policies
- Block storage sharding configuration

**Database Configuration (`database`):**
- LevelDB performance tuning parameters
- Connection pooling for async operations
- Cache sizes and compression options
- Durability and consistency settings

**Logging Configuration (`logging`):**
- Log levels and output destinations
- File rotation and retention policies
- Output formatting (JSON, colored console)
- Debug and verbose logging options

**Security Configuration (`security`):**
- Authentication and authorization settings
- Rate limiting and abuse prevention
- File type restrictions and validation
- API key management (future feature)

**Performance Configuration (`performance`):**
- Thread pool sizing and management
- Memory caching policies
- Operation timeouts for all endpoints
- Async operation configuration

**Monitoring Configuration (`monitoring`):**
- Health check intervals and thresholds
- Metrics collection and exposure
- Alert thresholds for resource usage
- Performance monitoring settings

### 6.3 Configuration Access Patterns

**Recommended Usage:**
```cpp
// Use ConfigManager getters (type-safe)
int port = CONFIG.getServerPort();
size_t chunkSize = CONFIG.getChunkSizeBytes();
bool corsEnabled = CONFIG.isCorsEnabled();

// Avoid direct JSON access
// json config = CONFIG.getValue("server.port", 8080); // Not recommended
```

**Configuration Validation:**
- All configuration values have sensible defaults
- Type checking and validation on startup
- Error reporting for invalid configurations
- Graceful fallback to defaults for missing values

**Dynamic Configuration:**
- Configuration reload without service restart (planned)
- Environment variable overrides
- Command-line parameter support
- Configuration file validation tools

## 7. Data Model and Persistence

### 7.1 Content Identifier (CID) System

**CID Generation:**
- **Algorithm**: SHA-256 hash of node content
- **Format**: Hexadecimal string representation
- **Properties**: Content-addressable, deterministic, collision-resistant
- **Usage**: Unique identifier for all nodes in the DAG

**CID Examples:**
```
RootNode CID:  Qm7fa8b1234567890abcdef...
ProtoNode CID: Qm9ab2c3456789012345678...
RawNode CID:   Qm3cd4e5678901234567890...
```

### 7.2 LevelDB Key-Value Schema

**Key Patterns:**
```
root:{cid}     → RootNode JSON metadata
proto:{cid}    → ProtoNode JSON metadata
raw:{cid}      → RawNode JSON metadata
type:{cid}     → Node type indicator ("ROOT", "PROTO", "RAW")
```

**Value Formats:**
- **JSON Serialization**: All metadata stored as JSON for readability
- **Atomic Operations**: Batch writes for consistency
- **Compression**: Optional Snappy compression in LevelDB
- **Indexing**: Efficient key-based lookups with bloom filters

### 7.3 Block Store File System

**Storage Structure:**
```
data/blocks/
├── 00/                    # Shard 0x00
│   ├── Qm{cid}_chunk_0
│   ├── Qm{cid}_chunk_1
│   └── ...
├── 01/                    # Shard 0x01
│   └── ...
└── ff/                    # Shard 0xff
```

**Sharding Algorithm:**
- **Shard Count**: 256 directories (00-ff)
- **Shard Key**: First byte of CID hex representation
- **File Naming**: `{cid}_chunk_{index}`
- **Benefits**: Distributes I/O load, improves filesystem performance

### 7.4 DAG Node Relationships

**Hierarchy Structure:**
```
RootNode (File Metadata)
├── ProtoNode (Optional, for >160 chunks)
│   ├── RawNode (Chunk 0)
│   ├── RawNode (Chunk 1)
│   └── ...
└── RawNode (Direct link for ≤160 chunks)
```

**Reference Counting:**
- **Purpose**: Enable deduplication and garbage collection
- **Implementation**: Each RawNode maintains reference count
- **Lifecycle**: Increment on link creation, decrement on deletion
- **Garbage Collection**: Delete when reference count reaches zero

### 7.5 Data Consistency and Integrity

**ACID Properties:**
- **Atomicity**: Batch operations in LevelDB
- **Consistency**: Reference counting maintains DAG integrity
- **Isolation**: Mutex protection for concurrent operations
- **Durability**: Persistent storage with configurable sync options

**Checksums:**
- **Chunk Level**: SHA-256 checksum per chunk
- **File Level**: Optional file-level checksum in RootNode
- **Validation**: Checksum verification during read operations
- **Error Detection**: Corruption detection and reporting

## 8. Algorithms and Implementation Details

### 8.1 File Chunking Algorithm

**Chunking Strategy:**
- **Chunk Size**: 262,144 bytes (256KB) fixed size
- **Last Chunk**: Variable size (≤256KB) for file remainder
- **Boundary Handling**: No content-based chunking (fixed boundaries)
- **Performance**: Optimized for balanced chunk distribution

**Chunking Process:**
```cpp
std::vector<ChunkData> createChunks(const std::vector<uint8_t>& data) {
    const size_t CHUNK_SIZE = 262144;
    std::vector<ChunkData> chunks;

    for (size_t offset = 0; offset < data.size(); offset += CHUNK_SIZE) {
        size_t chunkSize = std::min(CHUNK_SIZE, data.size() - offset);
        ChunkData chunk;
        chunk.data.assign(data.begin() + offset, data.begin() + offset + chunkSize);
        chunk.index = chunks.size();
        chunk.checksum = calculateSHA256(chunk.data);
        chunks.push_back(chunk);
    }

    return chunks;
}
```

### 8.2 DAG Construction Algorithm

**Tree Construction Rules:**
- **Small Files (≤160 chunks)**: Direct RootNode → RawNode links
- **Large Files (>160 chunks)**: RootNode → ProtoNode → RawNode hierarchy
- **Fan-out Limits**: 160 children per node (ROOT_MAX_CHILDREN = 160)
- **ProtoNode Grouping**: Group RawNodes into ProtoNodes of 160 chunks each

**Construction Algorithm:**
```
if (totalChunks <= 160) {
    // Direct linking
    rootNode.addChildLinks(rawNodeCIDs);
} else {
    // ProtoNode hierarchy
    protoNodes = createProtoNodes(rawNodeCIDs, 160);
    rootNode.addChildLinks(protoNodeCIDs);
}
```

### 8.3 Content Deduplication

**Deduplication Strategy:**
- **Level**: RawNode (chunk) level deduplication
- **Detection**: SHA-256 hash comparison
- **Reference Counting**: Automatic reference management
- **Storage Efficiency**: Shared chunks reduce storage usage

**Deduplication Process:**
1. Calculate chunk hash
2. Check if RawNode with hash exists
3. If exists: increment reference count
4. If not exists: create new RawNode
5. Link to parent node

### 8.4 File Update Algorithm (NEW)

**Update Strategy: DELETE + UPLOAD**
- **Step 1**: Validate old file exists
- **Step 2**: Delete old file completely (chunks + metadata)
- **Step 3**: Upload new file with standard upload process
- **Step 4**: Return new CID

**Benefits:**
- **Simplicity**: Uses proven delete and upload operations
- **Reliability**: No complex reference counting logic
- **Atomicity**: Clear operation boundaries
- **Error Handling**: Easy rollback and error reporting

### 8.5 Performance Limits and Constraints

**File Size Limits:**
- **Default Maximum**: 1GB per file (configurable)
- **Theoretical Limit**: Limited by available disk space
- **Chunk Limit**: ~4 million chunks per file (RootNode → ProtoNode → RawNode)

**Concurrency Limits:**
- **Worker Threads**: Configurable (default: 4 workers)
- **Database Connections**: Pooled connections (default: 10 max)
- **Request Concurrency**: Limited by server configuration

**Memory Usage:**
- **Chunk Processing**: One chunk in memory at a time
- **Metadata Cache**: Configurable LevelDB cache
- **Request Buffers**: Bounded by max request size

**Performance Characteristics:**
- **Upload Throughput**: ~100MB/s (SSD storage)
- **Download Throughput**: ~200MB/s (sequential reads)
- **Latency**: <10ms for metadata operations
- **Scalability**: Single-node, vertical scaling

## 9. Complete HTTP API Specification

### 9.1 File Upload API

**Endpoint:** `POST /upload`

**Description:** Upload a new file to the QFS storage system with automatic chunking and CID generation.

**Request Format:**
```http
POST /upload HTTP/1.1
Host: localhost:8080
Content-Type: multipart/form-data; boundary=----FormBoundary
X-Filename: example.txt

------FormBoundary
Content-Disposition: form-data; name="file"; filename="example.txt"
Content-Type: text/plain

[Binary file content]
------FormBoundary--
```

**Alternative Binary Upload:**
```http
POST /upload HTTP/1.1
Host: localhost:8080
Content-Type: application/octet-stream
X-Filename: example.txt
Content-Length: 1048576

[Binary file content]
```

**Response (Success):**
```json
{
  "status": "success",
  "data": {
    "cid": "Qm7fa8b1234567890abcdef...",
    "filename": "example.txt",
    "size": 1048576,
    "chunksCount": 4,
    "fileType": "text/plain",
    "uploadTimestamp": "2024-01-15T10:30:00Z"
  }
}
```

**Response (Error):**
```json
{
  "status": "error",
  "message": "File size exceeds maximum limit",
  "code": "FILE_TOO_LARGE"
}
```

### 9.2 File Update API

**Endpoint:** `PUT /update/{cid}`

**Description:** Update an existing file using DELETE + UPLOAD strategy for reliable operation.

**Request Format:**
```http
PUT /update/Qm7fa8b1234567890abcdef... HTTP/1.1
Host: localhost:8080
Content-Type: multipart/form-data; boundary=----FormBoundary
X-Filename: updated_example.txt

------FormBoundary
Content-Disposition: form-data; name="file"; filename="updated_example.txt"
Content-Type: text/plain

[Updated binary file content]
------FormBoundary--
```

**Response (Success):**
```json
{
  "status": "success",
  "data": {
    "oldCid": "Qm7fa8b1234567890abcdef...",
    "newCid": "Qm9ab2c3456789012345678...",
    "filename": "updated_example.txt",
    "size": 2097152,
    "chunksCount": 8,
    "updateTimestamp": "2024-01-15T11:00:00Z"
  }
}
```

### 9.3 File Download API

**Endpoint:** `GET /download/{cid}`

**Description:** Download a file by CID with streaming support and automatic chunk reassembly.

**Request Format:**
```http
GET /download/Qm7fa8b1234567890abcdef... HTTP/1.1
Host: localhost:8080
Accept: */*
```

**Response (Success):**
```http
HTTP/1.1 200 OK
Content-Type: text/plain
Content-Length: 1048576
Content-Disposition: attachment; filename="example.txt"
X-CID: Qm7fa8b1234567890abcdef...
X-Chunks-Count: 4
X-File-Size: 1048576

[Binary file content stream]
```

**Response (Not Found):**
```json
{
  "status": "error",
  "message": "File not found",
  "code": "FILE_NOT_FOUND"
}
```

### 9.4 Chunk Access API

**Endpoint:** `GET /chunk/{cid}`

**Description:** Download a specific chunk by its CID for granular access.

**Request Format:**
```http
GET /chunk/Qm3cd4e5678901234567890... HTTP/1.1
Host: localhost:8080
```

**Response (Success):**
```http
HTTP/1.1 200 OK
Content-Type: application/octet-stream
Content-Length: 262144
X-Chunk-Index: 0
X-Chunk-Checksum: sha256:a1b2c3d4e5f6...

[Binary chunk content]
```

### 9.5 File Deletion API

**Endpoint:** `DELETE /delete/{cid}`

**Description:** Delete a file and all associated chunks with complete cleanup.

**Request Format:**
```http
DELETE /delete/Qm7fa8b1234567890abcdef... HTTP/1.1
Host: localhost:8080
```

**Response (Success):**
```json
{
  "status": "success",
  "data": {
    "cid": "Qm7fa8b1234567890abcdef...",
    "chunksDeleted": 4,
    "metadataDeleted": true,
    "deleteTimestamp": "2024-01-15T12:00:00Z"
  }
}
```

### 9.6 System Status API

**Endpoint:** `GET /status`

**Description:** Get system health, statistics, and operational status.

**Request Format:**
```http
GET /status HTTP/1.1
Host: localhost:8080
```

**Response:**
```json
{
  "status": "healthy",
  "version": "1.0.0",
  "uptime": 3600,
  "stats": {
    "totalFiles": 156,
    "totalChunks": 2847,
    "storageUsedBytes": 524288000,
    "metadataEntries": 3159
  },
  "system": {
    "memoryUsage": "45.2MB",
    "diskUsage": "78.3%",
    "activeConnections": 3,
    "queueSize": 0
  },
  "timestamp": "2024-01-15T12:30:00Z"
}
```

### 9.7 File Listing API

**Endpoint:** `GET /list`

**Description:** List all files stored in the system with metadata.

**Request Format:**
```http
GET /list HTTP/1.1
Host: localhost:8080
```

**Query Parameters:**
- `limit`: Maximum number of files to return (default: 100)
- `offset`: Number of files to skip (default: 0)
- `sort`: Sort order ("date", "size", "name") (default: "date")

**Response:**
```json
{
  "status": "success",
  "data": {
    "files": [
      {
        "cid": "Qm7fa8b1234567890abcdef...",
        "filename": "example.txt",
        "size": 1048576,
        "chunksCount": 4,
        "fileType": "text/plain",
        "uploadTimestamp": "2024-01-15T10:30:00Z"
      }
    ],
    "total": 156,
    "limit": 100,
    "offset": 0
  }
}
```

### 9.8 CORS and Options Handling

**Endpoint:** `OPTIONS /*`

**Description:** Handle CORS preflight requests for cross-origin access.

**Response Headers:**
```http
Access-Control-Allow-Origin: *
Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS
Access-Control-Allow-Headers: Content-Type, X-Filename, Authorization
Access-Control-Max-Age: 86400
```

### 9.9 Error Response Format

**Standard Error Response:**
```json
{
  "status": "error",
  "message": "Human-readable error description",
  "code": "ERROR_CODE_CONSTANT",
  "timestamp": "2024-01-15T12:30:00Z",
  "requestId": "req_123456789"
}
```

**Common HTTP Status Codes:**
- `200 OK`: Successful operation
- `201 Created`: File uploaded successfully
- `400 Bad Request`: Invalid request format or parameters
- `404 Not Found`: File or resource not found
- `413 Payload Too Large`: File exceeds size limits
- `500 Internal Server Error`: Server-side error
- `503 Service Unavailable`: Server overloaded or maintenance

## 10. Error Handling and Logging

### 10.1 Error Classification

**Client Errors (4xx):**
- **Invalid Request Format**: Malformed JSON, missing headers
- **File Size Violations**: Exceeds configured limits
- **Unsupported File Types**: Blocked extensions or invalid formats
- **Invalid CID Format**: Malformed content identifiers
- **Missing Files**: Requested CID does not exist

**Server Errors (5xx):**
- **Storage Failures**: Disk full, permission issues, I/O errors
- **Database Errors**: LevelDB corruption, connection failures
- **Memory Exhaustion**: Out of memory conditions
- **Timeout Errors**: Operation timeouts, network issues
- **Integrity Failures**: Checksum mismatches, corruption detection

### 10.2 Error Response Schema

**Standard Error Format:**
```json
{
  "status": "error",
  "message": "Descriptive error message",
  "code": "ERROR_CODE",
  "details": {
    "field": "specific_field",
    "value": "problematic_value",
    "constraint": "validation_rule"
  },
  "timestamp": "2024-01-15T12:30:00Z",
  "requestId": "req_123456789"
}
```

**Error Codes:**
```
FILE_NOT_FOUND         - Requested CID does not exist
FILE_TOO_LARGE         - File exceeds size limits
INVALID_CID_FORMAT     - Malformed CID string
UNSUPPORTED_FILE_TYPE  - File type not allowed
STORAGE_FULL           - Insufficient storage space
DATABASE_ERROR         - LevelDB operation failed
CHECKSUM_MISMATCH      - Data integrity check failed
OPERATION_TIMEOUT      - Request timeout exceeded
RATE_LIMIT_EXCEEDED    - Too many requests
SERVER_OVERLOADED      - Server capacity exceeded
```

### 10.3 Logging System

**Log Levels:**
- **DEBUG**: Detailed debugging information
- **INFO**: General operational information
- **WARNING**: Potentially problematic situations
- **ERROR**: Error conditions that don't stop operation
- **CRITICAL**: Serious errors that may stop operation

**Log Format:**
```
[2024-01-15 12:30:00.123] [INFO] [HttpServer] File uploaded successfully: example.txt (CID: Qm7fa8b...)
[2024-01-15 12:30:01.456] [ERROR] [StorageManager] Failed to store chunk: disk full
[2024-01-15 12:30:02.789] [WARNING] [AsyncDBManager] Database operation timeout: getRootNode
```

**Log Destinations:**
- **File Logging**: Rotated log files in `logs/qfs.log`
- **Console Logging**: Optional colored console output
- **Structured Logging**: Optional JSON format for log aggregation

**Log Rotation:**
- **Size-based**: Rotate when file exceeds configured size
- **Retention**: Keep configurable number of old log files
- **Compression**: Optional compression of archived logs

### 10.4 Monitoring and Alerting

**Health Checks:**
- **Database Connectivity**: LevelDB connection status
- **Storage Availability**: Disk space and I/O health
- **Memory Usage**: Available memory and GC pressure
- **Thread Pool Status**: Worker thread availability

**Performance Metrics:**
- **Request Rates**: Requests per second by endpoint
- **Response Times**: Latency percentiles (p50, p95, p99)
- **Error Rates**: Error percentage by type
- **Storage Growth**: Storage utilization trends

**Alert Conditions:**
- **Storage Usage**: >90% disk utilization
- **Memory Usage**: >80% memory utilization
- **Error Rate**: >5% error rate sustained
- **Response Time**: >5s p95 latency
- **Database Issues**: Connection failures or corruption

### 11. Concurrency and Performance
- Thread safety: mutex guards in `StorageManager` for shared resources
- Thread pools: configurable worker and I/O thread counts
- Caching: optional memory cache for chunks/metadata with TTLs
- Timeouts: per-operation timeouts from config enforce SLA boundaries

### 12. Security and Compliance
- Content integrity via checksums/CIDs; data tampering changes identifiers
- Optional rate limiting; upload restrictions for disallowed extensions
- Authentication disabled by default; must be added before production use
- No encryption-at-rest by default; can be layered via OS/filesystem or future feature

### 13. Operational Observability
- Metrics endpoint (if enabled) for basic counters
- Health checks with interval; alerts for storage/memory thresholds
- Log-based troubleshooting guidance

### 14. Testing Strategy (initial)
- Unit tests for: chunking, DAG serialization, storage paths, integrity validation
- Integration tests: upload→download roundtrip; update; delete; list; status
- Non-goals for now: distributed behavior, replication chaos testing

### 15. Constraints and Assumptions
- Single-node instance; local disk durability
- Max file size default 1GB (configurable)
- Chunk size fixed for a given deployment; changing requires migration
- LevelDB settings tuned via config; snappy compression optional

### 16. Future Work
- Garbage collection with pin sets and reachability scanning
- Content deduplication across files and versioning improvements
- Authentication/authorization, API keys/tokens
- P2P or multi-node replication; cluster membership and consistency model
- Compression pluggability and adaptive chunk sizing
- Richer metadata indexing and search

## 17. Glossary and Terminology

### 17.1 Core Concepts

**CID (Content Identifier)**
- Cryptographic hash-based identifier for content
- Generated using SHA-256 algorithm
- Ensures content immutability and integrity
- Format: Hexadecimal string (e.g., "Qm7fa8b1234567890abcdef...")

**DAG (Directed Acyclic Graph)**
- Tree-like structure organizing file chunks
- Ensures efficient storage and retrieval
- Enables content deduplication
- Forms the basis of the content-addressed storage system

**Chunking**
- Process of splitting files into fixed-size pieces
- Default chunk size: 256KB (262,144 bytes)
- Enables efficient storage and transfer
- Facilitates content deduplication

### 17.2 Node Types

**RootNode**
- Top-level node representing a complete file
- Contains file metadata (name, size, type, timestamp)
- Links to ProtoNodes or RawNodes
- Serves as the entry point for file operations

**ProtoNode**
- Intermediate aggregation node for large files
- Groups up to 160 child nodes
- Reduces fan-out for files with many chunks
- Optional layer between RootNode and RawNodes

**RawNode**
- Leaf node containing actual chunk data
- Stores binary chunk content and metadata
- Implements reference counting for deduplication
- Represents the smallest unit of content storage

### 17.3 Technical Terms

**Reference Counting**
- Mechanism for tracking chunk usage
- Enables automatic garbage collection
- Supports content deduplication
- Prevents premature data deletion

**Sharding**
- Distribution of chunks across multiple directories
- Improves file system performance
- Reduces directory size limitations
- Enhances parallel I/O operations

**Atomic Operations**
- Database operations that complete entirely or not at all
- Ensures data consistency
- Prevents partial updates
- Critical for system reliability

**Checksum**
- Cryptographic hash for data integrity verification
- Uses SHA-256 algorithm
- Detects data corruption
- Ensures content authenticity

### 17.4 Storage Terms

**Block Store**
- File system storage for chunk data
- Organized using sharding strategy
- Separate from metadata storage
- Optimized for sequential access

**LevelDB**
- Key-value database for metadata storage
- Provides fast lookups and atomic operations
- Supports compression and caching
- Ensures metadata consistency

**Deduplication**
- Elimination of duplicate chunk storage
- Reduces storage space requirements
- Implemented through content addressing
- Automatic and transparent to users

### 17.5 API Terms

**CORS (Cross-Origin Resource Sharing)**
- Web security feature for cross-domain requests
- Configurable in QFS for web applications
- Enables browser-based file operations
- Security mechanism for web APIs

**Multipart Upload**
- HTTP protocol for file uploads
- Supports metadata and binary data
- Standard web form upload mechanism
- Alternative to binary upload method

**Streaming Response**
- HTTP response sent in chunks
- Enables large file downloads
- Reduces memory usage
- Improves user experience

## 18. References and Resources

### 18.1 Source Code Structure

**Core Implementation:**
```
src/
├── http_server.cpp         # HTTP API implementation
├── storage_manager.cpp     # Core storage logic
├── async_db_manager.cpp    # Async database operations
├── dag_node.cpp           # DAG node implementations
├── chunk.cpp              # File chunking logic
├── config_manager.cpp     # Configuration management
├── logger.cpp             # Logging system
├── utils.cpp              # Utility functions
└── instance_lock.cpp      # Instance management
```

**Header Files:**
```
include/
├── http_server.h
├── storage_manager.h
├── async_db_manager.h
├── dag_node.h
├── chunk.h
├── config_manager.h
├── logger.h
├── utils.h
└── instance_lock.h
```

### 18.2 Build and Configuration

**Build System:**
- `CMakeLists.txt` - Primary build configuration
- `config/config.json` - Runtime configuration template
- `third_party/` - External dependencies

**Documentation:**
- `README.md` - Project overview and quick start
- `spec/spec.md` - This comprehensive specification
- `docs/` - Additional documentation (if available)

### 18.3 Related Technologies

**IPFS (InterPlanetary File System)**
- Inspiration for content-addressed storage
- Similar DAG-based architecture
- Distributed and P2P focus (vs. QFS single-node)

**Git Version Control**
- Similar content-addressed storage model
- DAG-based data structure
- Inspiration for Merkle tree concepts

**BitTorrent Protocol**
- Chunked file distribution
- Content integrity verification
- Peer-to-peer content sharing concepts

### 18.4 Standards and Specifications

**HTTP/1.1 (RFC 7230-7237)**
- Foundation for RESTful API
- Multipart form data handling
- Status codes and error handling

**SHA-256 (FIPS 180-4)**
- Cryptographic hash function
- Content identifier generation
- Data integrity verification

**JSON (RFC 7159)**
- Configuration file format
- API request/response format
- Metadata serialization

**Semantic Versioning (SemVer)**
- Version numbering scheme
- Backward compatibility guarantees
- Release management strategy

---

**Document Version:** 2.0
**Last Updated:** January 2025
**Authors:** QFS Development Team
**Status:** Production Ready