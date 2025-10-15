# QFS Bootstrap Node

**Version:** 1.0.0
**Status:** In Development - Foundation Phase

---

## What is QFS Bootstrap Node?

QFS Bootstrap Node is a **provider discovery service** for the QFS (Qik File System) peer-to-peer file sharing network. It acts as a lightweight registry that tracks which nodes have which files, enabling efficient peer discovery without the complexity of Distributed Hash Table (DHT) protocols.

### Analogy

Think of Bootstrap Node as a **phonebook** for the P2P network:
- When you upload a file to your QFS node, it "announces" to the Bootstrap Node: *"I have file X"*
- When you want to download a file, you ask the Bootstrap Node: *"Who has file X?"*
- The Bootstrap Node responds with a list of providers
- You then connect **directly** to those providers to download the file

### Key Characteristics

- **Lightweight**: In-memory registry with minimal resource requirements
- **Fast**: Sub-500ms discovery queries
- **Simple**: HTTP REST API, no complex protocols
- **Stateless**: Doesn't persist data; providers re-announce on startup
- **Scalable**: One Bootstrap Node can serve thousands of QFS nodes

---

## Architecture

```
┌─────────────┐
│ QFS Node A  │ ──(1. Announce: "I have file QmXYZ123")──┐
└─────────────┘                                            │
                                                           ▼
                                                  ┌─────────────────┐
                                                  │ Bootstrap Node  │
                                                  │   (Registry)    │
                                                  │                 │
                                                  │  QmXYZ123 →     │
                                                  │  [Node A, ...]  │
                                                  └─────────────────┘
                                                           │
┌─────────────┐                                            │
│ QFS Node B  │ ──(2. Discover: "Who has QmXYZ123?")─────┘
└──────┬──────┘                                            │
       │                                                   │
       │        (3. Returns: Node A at 192.168.1.100)     │
       │◄──────────────────────────────────────────────────┘
       │
       │ (4. Direct P2P download from Node A)
       ▼
┌─────────────┐
│ QFS Node A  │
└─────────────┘
```

---

## Features

### Current Implementation (v1.0.0 - Foundation)

- ✅ Project structure and build system
- ✅ Command-line argument parsing
- ✅ Configuration management
- ⏳ HTTP REST API server (coming in next task)
- ⏳ Provider registry (coming in next task)
- ⏳ Announce endpoint (coming in next task)
- ⏳ Discover endpoint (coming in next task)

### Planned Features

- 🔜 Provider keepalive mechanism
- 🔜 Stale provider cleanup
- 🔜 Provider caching with TTL
- 🔜 Health monitoring and metrics
- 🔜 Multiple Bootstrap Node support (failover)
- 🔜 Provider reputation system
- 🔜 Rate limiting and abuse prevention

---

## Building

### Prerequisites

Ensure you have the required dependencies installed:

**Ubuntu/Debian:**
```bash
sudo apt update
sudo apt install build-essential cmake libssl-dev
```

**macOS:**
```bash
brew install cmake openssl
```

**Windows:**
- Use Visual Studio 2019+ with CMake support
- Or use vcpkg: `vcpkg install openssl`

### Build Instructions

From the project root directory:

```bash
# Navigate to build directory
mkdir -p build
cd build

# Configure CMake (builds both QFS node and Bootstrap Node)
cmake ..

# Build Bootstrap Node only
cmake --build . --target qfs-bootstrap

# Or build everything
cmake --build .
```

This will create the executable:
- **Linux/macOS:** `build/qfs-bootstrap`
- **Windows:** `build/qfs-bootstrap.exe` or `build/Debug/qfs-bootstrap.exe`

---

## Running

### Basic Usage

```bash
# Start with default settings (localhost:8090)
./qfs-bootstrap

# Custom host and port
./qfs-bootstrap --host 0.0.0.0 --port 9000

# Use configuration file
./qfs-bootstrap --config ../bootstrap_node/config/bootstrap.json

# Verbose logging
./qfs-bootstrap --verbose

# Show help
./qfs-bootstrap --help

# Show version
./qfs-bootstrap --version
```

### Command-Line Options

| Option | Description | Default |
|--------|-------------|---------|
| `-h, --help` | Show help message | - |
| `-v, --version` | Show version information | - |
| `-p, --port <port>` | Port to listen on | 8090 |
| `--host <host>` | Host to bind to | 0.0.0.0 |
| `-c, --config <file>` | Configuration file path | - |
| `--verbose` | Enable verbose logging | false |

---

## API Endpoints

### Health Check

Check if Bootstrap Node is running.

**Endpoint:** `GET /health`

**Response:**
```json
{
  "status": "ok",
  "version": "1.0.0",
  "uptime": 3600
}
```

**Example:**
```bash
curl http://localhost:8090/health
```

---

### Announce

Register as a provider for a file.

**Endpoint:** `POST /announce`

**Request Body:**
```json
{
  "cid": "QmXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX",
  "nodeId": "my-node-uuid-1234",
  "ip": "192.168.1.100",
  "port": 8080,
  "bandwidth": 1000000
}
```

**Fields:**
- `cid` (string, required): Content Identifier of the file
- `nodeId` (string, required): Unique identifier for the node
- `ip` (string, required): IP address of the node
- `port` (number, required): Port the node is listening on
- `bandwidth` (number, optional): Available bandwidth in bytes/sec

**Response (Success):**
```json
{
  "status": "success",
  "message": "Provider announced successfully"
}
```

**Response (Error):**
```json
{
  "status": "error",
  "message": "Invalid request: missing required field 'cid'"
}
```

**Example:**
```bash
curl -X POST http://localhost:8090/announce \
  -H "Content-Type: application/json" \
  -d '{
    "cid": "QmTest123",
    "nodeId": "node-1",
    "ip": "192.168.1.100",
    "port": 8080,
    "bandwidth": 1000000
  }'
```

---

### Discover

Find providers for a file.

**Endpoint:** `GET /discover/{cid}`

**Parameters:**
- `cid` (path): Content Identifier of the file to discover

**Response (Found):**
```json
{
  "cid": "QmXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX",
  "providers": [
    {
      "nodeId": "node-1",
      "ip": "192.168.1.100",
      "port": 8080,
      "bandwidth": 1000000,
      "lastSeen": "2025-10-15T10:30:00Z"
    },
    {
      "nodeId": "node-2",
      "ip": "192.168.1.200",
      "port": 8080,
      "bandwidth": 500000,
      "lastSeen": "2025-10-15T10:29:00Z"
    }
  ]
}
```

**Response (Not Found):**
```json
{
  "status": "not_found",
  "message": "No providers found for CID: QmXXX..."
}
```

**Example:**
```bash
curl http://localhost:8090/discover/QmTest123
```

---

## Configuration

### Configuration File Format

Create a JSON configuration file (e.g., `bootstrap.json`):

```json
{
  "server": {
    "port": 8090,
    "host": "0.0.0.0",
    "max_connections": 1000
  },
  "logging": {
    "level": "info",
    "file": "data/logs/bootstrap.log",
    "console": true
  },
  "registry": {
    "cleanup_interval": 300,
    "provider_timeout": 600,
    "max_providers_per_cid": 100
  }
}
```

### Configuration Options

#### Server Settings

| Option | Type | Description | Default |
|--------|------|-------------|---------|
| `port` | number | HTTP server port | 8090 |
| `host` | string | Bind address (0.0.0.0 = all interfaces) | 0.0.0.0 |
| `max_connections` | number | Maximum concurrent connections | 1000 |

#### Logging Settings

| Option | Type | Description | Default |
|--------|------|-------------|---------|
| `level` | string | Log level (debug/info/warn/error) | info |
| `file` | string | Log file path | data/logs/bootstrap.log |
| `console` | boolean | Enable console logging | true |

#### Registry Settings

| Option | Type | Description | Default |
|--------|------|-------------|---------|
| `cleanup_interval` | number | Seconds between stale provider cleanups | 300 |
| `provider_timeout` | number | Seconds before provider considered stale | 600 |
| `max_providers_per_cid` | number | Maximum providers per file | 100 |

---

## Deployment

### Single Bootstrap Node

For small networks, run a single Bootstrap Node:

```bash
# Production server
./qfs-bootstrap --host 0.0.0.0 --port 8090
```

Configure all QFS nodes to use this Bootstrap Node:
```json
{
  "bootstrap_url": "http://bootstrap.example.com:8090"
}
```

### Multiple Bootstrap Nodes (Redundancy)

For larger networks, run multiple Bootstrap Nodes behind a load balancer:

```
                    ┌─────────────────┐
All QFS Nodes ────► │  Load Balancer  │
                    │  (HAProxy/Nginx)│
                    └────────┬────────┘
                             │
              ┌──────────────┼──────────────┐
              │              │              │
              ▼              ▼              ▼
      ┌────────────┐ ┌────────────┐ ┌────────────┐
      │ Bootstrap  │ │ Bootstrap  │ │ Bootstrap  │
      │  Node 1    │ │  Node 2    │ │  Node 3    │
      └────────────┘ └────────────┘ └────────────┘
```

QFS nodes can also have multiple Bootstrap URLs for failover:
```json
{
  "bootstrap_urls": [
    "http://bootstrap1.example.com:8090",
    "http://bootstrap2.example.com:8090",
    "http://bootstrap3.example.com:8090"
  ]
}
```

### Resource Requirements

**Minimum:**
- CPU: 1 core
- RAM: 512 MB
- Network: 10 Mbps

**Recommended (1000 nodes):**
- CPU: 2-4 cores
- RAM: 2 GB
- Network: 100 Mbps

**Large Scale (10000+ nodes):**
- CPU: 8+ cores
- RAM: 8 GB
- Network: 1 Gbps

---

## Development

### Project Structure

```
bootstrap_node/
├── include/            # Header files
│   ├── bootstrap_server.h       # HTTP server (future)
│   ├── provider_registry.h      # Registry implementation (future)
│   └── bootstrap_types.h        # Data structures (future)
├── src/               # Source files
│   ├── main.cpp                 # Entry point ✅
│   ├── bootstrap_server.cpp     # HTTP server impl (future)
│   └── provider_registry.cpp    # Registry impl (future)
├── config/            # Configuration files
│   └── bootstrap.json.example   # Example config (future)
├── CMakeLists.txt     # Build configuration ✅
└── README.md          # This file ✅
```

### Adding New Features

When adding new components:

1. **Create header file** in `include/`
2. **Create implementation** in `src/`
3. **Update CMakeLists.txt** to include new source files
4. **Add tests** (when test framework is set up)
5. **Update this README** with new features

---

## Troubleshooting

### Common Issues

**Issue:** Port already in use
```
Error: Cannot bind to port 8090
```
**Solution:** Change port with `--port 8091` or stop other service using port 8090

---

**Issue:** Build fails with "OpenSSL not found"
```
CMake Error: OpenSSL not found
```
**Solution:** Install OpenSSL development libraries:
- Ubuntu: `sudo apt install libssl-dev`
- macOS: `brew install openssl`
- Windows: Use vcpkg or install OpenSSL manually

---

**Issue:** Can't connect from QFS nodes
```
Connection refused to Bootstrap Node
```
**Solution:**
1. Check Bootstrap Node is running: `curl http://localhost:8090/health`
2. Check firewall rules allow port 8090
3. If running on different machine, use `--host 0.0.0.0` not `localhost`

---

## Differences from QFS Node

| Feature | QFS Node | Bootstrap Node |
|---------|----------|----------------|
| **Purpose** | Store and serve files | Track providers |
| **Storage** | Large (file data) | Minimal (metadata only) |
| **Protocol** | Binary P2P + HTTP API | HTTP REST only |
| **Uptime** | Can be intermittent | Should be always on |
| **Scalability** | Many instances | Few instances |
| **Port** | 8080 | 8090 |

---

## Related Documentation

- **Implementation Strategy:** `../spec/QFS_Implementation_Strategy.pdf.md`
- **Implementation Step 1:** `../spec/implementation_step1.md`
- **Tasks & User Stories:** `../spec/to_do.md`
- **QFS Node README:** `../README.md`

---

## Roadmap

### Phase 1: Foundation ✅
- [x] Project structure
- [x] Build system
- [x] Command-line interface
- [ ] HTTP server skeleton
- [ ] Provider registry

### Phase 2: Core Functionality
- [ ] Announce endpoint
- [ ] Discover endpoint
- [ ] Health endpoint
- [ ] Basic logging

### Phase 3: Integration
- [ ] QFS node integration
- [ ] End-to-end testing
- [ ] Performance benchmarking

### Phase 4: Production Ready
- [ ] Keepalive mechanism
- [ ] Stale provider cleanup
- [ ] Multiple Bootstrap Node support
- [ ] Monitoring and metrics
- [ ] Security hardening

---

## Contributing

This is part of the QFS (Qik File System) project. Contributions are welcome!

When contributing to Bootstrap Node:
1. Follow existing code style (C++17)
2. Add tests for new features
3. Update this README
4. Document all public APIs

---

## License

MIT License - Same as QFS parent project

---

## Contact

For questions or issues:
- Check `../spec/` for detailed documentation
- Review implementation strategy document
- See QFS main README

---

**Bootstrap Node Status:** 🏗️ In Development - Foundation Phase Complete
