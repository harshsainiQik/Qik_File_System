# QFS - Qik File System

A simplified, beginner-friendly IPFS-like microservice written in C++ that provides content-addressed file storage with chunking and Merkle DAG structure.

## Features

- **File Chunking**: Splits files into 256KB chunks for efficient storage and retrieval
- **Merkle DAG**: Creates a content-addressed DAG structure for data integrity
- **Content Addressing**: Each file and chunk gets a unique CID (Content Identifier)
- **HTTP API**: Simple REST API for file operations
- **Multiple File Types**: Supports all file types (text, binary, media, documents)
- **Logging**: Comprehensive logging with configurable levels
- **Error Handling**: Robust error handling with try-catch blocks
- **Thread-Safe**: Safe for concurrent operations

## Architecture

```
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│   HTTP Client   │───▶│   QFS Server    │───▶│   Storage       │
│                 │    │                 │    │                 │
│ - Upload Files  │    │ - REST API      │    │ - LevelDB       │
│ - Download Files│    │ - Chunking      │    │ - Blockstore    │
│ - Manage Files  │    │ - Merkle DAG    │    │ - File System   │
└─────────────────┘    └─────────────────┘    └─────────────────┘
```

## Quick Start

### Prerequisites

Install required dependencies:

**Ubuntu/Debian:**
```bash
sudo apt update
sudo apt install build-essential cmake libssl-dev libleveldb-dev libjsoncpp-dev
```

**macOS:**
```bash
brew install cmake openssl leveldb jsoncpp
```

**Windows:**
```bash
# Use vcpkg
vcpkg install openssl leveldb jsoncpp
```

### Download httplib.h

QFS uses cpp-httplib, which is header-only. Download it:

```bash
# Create third_party directory
mkdir -p Qik_file_system/third_party/httplib

# Download httplib.h
curl -o Qik_file_system/third_party/httplib/httplib.h \
  https://raw.githubusercontent.com/yhirose/cpp-httplib/master/httplib.h
```

Or place `httplib.h` in `/usr/local/include/`

### Build

```bash
cd Qik_file_system
mkdir build
cd build
cmake ..
make
```

### Run

```bash
# Start with default settings (localhost:8080)
./qfs

# Custom host and port
./qfs -h 0.0.0.0 -p 9000

# Verbose logging
./qfs -v

# Custom data directory
./qfs -d /path/to/data

# Show help
./qfs --help
```

## API Endpoints

### Upload File
```bash
curl -X POST http://localhost:8080/upload \
  -H "X-Filename: test.txt" \
  --data-binary @test.txt
```

### Download File
```bash
curl http://localhost:8080/download/{CID} -o downloaded_file.txt
```

### Delete File
```bash
curl -X DELETE http://localhost:8080/delete/{CID}
```

### Server Status
```bash
curl http://localhost:8080/status
```

### List Files
```bash
curl http://localhost:8080/list
```

## File Storage Process

1. **Upload**: File is split into 256KB chunks
2. **Chunking**: Each chunk gets a SHA-256 checksum and CID
3. **DAG Creation**:
   - ≤160 chunks: Direct links from RootNode to RawNodes
   - >160 chunks: ProtoNodes group chunks (max 160 per ProtoNode)
4. **Storage**:
   - Metadata stored in LevelDB
   - Chunk data stored in filesystem blockstore
5. **Retrieval**: DAG is traversed to reassemble original file

## Directory Structure

```
data/
├── blocks/           # Chunk storage (sharded by CID prefix)
│   ├── Qm/          # CIDs starting with "Qm"
│   └── ...
├── leveldb/         # Metadata database
└── logs/           # Application logs
```

## Configuration

Default settings:
- **Port**: 8080
- **Host**: localhost
- **Chunk Size**: 256KB
- **Max File Size**: 1GB
- **Data Directory**: ./data

## Development

### Project Structure

```
Qik_file_system/
├── include/         # Header files
│   ├── logger.h
│   ├── utils.h
│   ├── chunk.h
│   ├── dag_node.h
│   ├── storage_manager.h
│   └── http_server.h
├── src/            # Source files
│   ├── main.cpp
│   ├── logger.cpp
│   ├── utils.cpp
│   ├── chunk.cpp
│   ├── dag_node.cpp
│   ├── storage_manager.cpp
│   └── http_server.cpp
├── data/           # Runtime data (created automatically)
├── tests/          # Test files (future)
└── CMakeLists.txt  # Build configuration
```

### Key Components

- **Logger**: Thread-safe logging to file and console
- **Chunker**: Splits files into fixed-size chunks
- **DAG Nodes**: RootNode, ProtoNode, RawNode for Merkle DAG
- **Storage Manager**: Handles LevelDB and blockstore operations
- **HTTP Server**: REST API using cpp-httplib
- **Utils**: Cryptographic, file I/O, and utility functions

## Examples

### Upload a text file:
```bash
echo "Hello QFS!" > hello.txt
curl -X POST http://localhost:8080/upload \
  -H "X-Filename: hello.txt" \
  --data-binary @hello.txt
```

### Upload an image:
```bash
curl -X POST http://localhost:8080/upload \
  -H "X-Filename: photo.jpg" \
  --data-binary @photo.jpg
```

### Upload large video:
```bash
curl -X POST http://localhost:8080/upload \
  -H "X-Filename: movie.mp4" \
  --data-binary @movie.mp4
```

## Differences from Original QFS

This simplified version focuses on:

✅ **Beginner-Friendly Code**: Clear, well-commented C++ without complex modern features
✅ **Essential Features**: Core chunking and DAG functionality
✅ **Robust Error Handling**: Try-catch blocks throughout
✅ **Comprehensive Logging**: Detailed logging for debugging
✅ **Simple Build**: Standard CMake with common dependencies

❌ **Advanced Features** (not included):
- Garbage collection
- Peer-to-peer networking
- Content deduplication
- Advanced compression
- Complex serialization

## Troubleshooting

### Common Issues

1. **Build Errors**: Ensure all dependencies are installed
2. **Port in Use**: Change port with `-p` flag
3. **Permission Denied**: Check data directory permissions
4. **httplib.h Not Found**: Download and place in correct location

### Logs

Check logs for detailed error information:
```bash
tail -f data/logs/qfs.log
```

## Performance

- **Memory Usage**: Fixed memory footprint regardless of file size
- **Chunk Size**: 256KB optimized for balance between metadata and I/O
- **Concurrent**: Thread-safe operations with mutex protection
- **Scalable**: Handles files from KB to GB efficiently

## Security

- **Content Addressing**: Files can't be tampered without changing CID
- **Checksums**: SHA-256 verification for data integrity
- **No Authentication**: Basic version - add auth for production use

## Contributing

This is a learning-focused implementation. Contributions welcome for:
- Bug fixes
- Documentation improvements
- Performance optimizations
- Additional error handling

## License

MIT License - Feel free to use for learning and development.# Qik_File_System
