cmake_minimum_required(VERSION 3.15)
project(QFS VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set build type to Release if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler-specific options
set(CMAKE_CXX_FLAGS "-Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/third_party)

# Set third-party paths
set(THIRD_PARTY_DIR "${CMAKE_SOURCE_DIR}/third_party")

# Find OpenSSL (still use system version)
find_package(OpenSSL REQUIRED)
if(OPENSSL_FOUND)
    message(STATUS "Found OpenSSL: ${OPENSSL_VERSION}")
else()
    message(FATAL_ERROR "OpenSSL not found. Please install OpenSSL development libraries.")
endif()

# Use local LevelDB - build it as part of our project
set(LEVELDB_SOURCE_DIR "${THIRD_PARTY_DIR}/leveldb")

if(EXISTS ${LEVELDB_SOURCE_DIR}/include/leveldb/db.h)
    message(STATUS "Found LevelDB source in third_party: ${LEVELDB_SOURCE_DIR}")

    # Add LevelDB as a subdirectory to build it
    set(LEVELDB_BUILD_TESTS OFF CACHE BOOL "Build LevelDB tests" FORCE)
    set(LEVELDB_BUILD_BENCHMARKS OFF CACHE BOOL "Build LevelDB benchmarks" FORCE)
    set(LEVELDB_INSTALL OFF CACHE BOOL "Install LevelDB" FORCE)

    add_subdirectory(${LEVELDB_SOURCE_DIR} ${CMAKE_BINARY_DIR}/leveldb-build)

    # Set variables for our use
    set(LEVELDB_INCLUDE_DIR "${LEVELDB_SOURCE_DIR}/include")
    set(LEVELDB_LIBRARY leveldb)

    message(STATUS "LevelDB will be built as part of the project")
else()
    message(FATAL_ERROR "LevelDB source not found in third_party/leveldb/. Please ensure LevelDB source is properly placed.")
endif()

# Use local nlohmann-json instead of jsoncpp
set(NLOHMANN_JSON_INCLUDE_DIR "${THIRD_PARTY_DIR}/nlohmann-json")
if(EXISTS ${NLOHMANN_JSON_INCLUDE_DIR}/json.hpp)
    message(STATUS "Found nlohmann-json in third_party: ${NLOHMANN_JSON_INCLUDE_DIR}")
    set(JSON_FOUND TRUE)
else()
    message(FATAL_ERROR "nlohmann-json not found in third_party/nlohmann-json/. Please ensure json.hpp is in that directory.")
endif()

# Use local cpp-httplib
set(HTTPLIB_INCLUDE_DIR "${THIRD_PARTY_DIR}/cpp-httplib")
if(EXISTS ${HTTPLIB_INCLUDE_DIR}/httplib.h)
    message(STATUS "Found cpp-httplib in third_party: ${HTTPLIB_INCLUDE_DIR}")
else()
    message(FATAL_ERROR "cpp-httplib not found in third_party/cpp-httplib/. Please ensure httplib.h is in that directory.")
endif()

# Source files
set(SOURCES
    src/main.cpp
    src/logger.cpp
    src/utils.cpp
    src/chunk.cpp
    src/dag_node.cpp
    src/storage_manager.cpp
    src/http_server.cpp
    src/instance_lock.cpp
    src/config_manager.cpp
    src/async_db_manager.cpp
)

# Header files (for IDE support)
set(HEADERS
    include/logger.h
    include/utils.h
    include/chunk.h
    include/dag_node.h
    include/storage_manager.h
    include/http_server.h
    include/instance_lock.h
    include/config_manager.h
    include/async_db_manager.h
)

# Create executable
add_executable(qfs ${SOURCES} ${HEADERS})

# Link libraries
target_link_libraries(qfs
    ${OPENSSL_LIBRARIES}
    ${LEVELDB_LIBRARY}
    pthread
)

# Include directories
target_include_directories(qfs PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/third_party
    ${OPENSSL_INCLUDE_DIR}
    ${LEVELDB_INCLUDE_DIR}
    ${NLOHMANN_JSON_INCLUDE_DIR}
    ${HTTPLIB_INCLUDE_DIR}
)

# Compiler definitions
target_compile_definitions(qfs PRIVATE)

# Platform-specific settings
if(WIN32)
    target_link_libraries(qfs ws2_32 wsock32 psapi)
    # Use Windows 10 version (0x0A00) for httplib compatibility
    target_compile_definitions(qfs PRIVATE _WIN32_WINNT=0x0A00)
endif()

# Create directories for runtime
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/data)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/data/blocks)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/data/leveldb)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/data/logs)

# Installation
install(TARGETS qfs DESTINATION bin)

# Create config directory
install(DIRECTORY DESTINATION etc/qfs)

# Install example config
install(FILES
    ${CMAKE_SOURCE_DIR}/config/qfs.json.example
    DESTINATION etc/qfs
    OPTIONAL
)

# Print build information
message(STATUS "")
message(STATUS "===============================================")
message(STATUS "QFS (Qik File System) Build Configuration")
message(STATUS "===============================================")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "")
message(STATUS "Dependencies:")
message(STATUS "  OpenSSL: ${OPENSSL_VERSION}")
message(STATUS "  LevelDB: ${LEVELDB_LIBRARY}")
message(STATUS "  jsoncpp: ${JSONCPP_LIBRARIES}")
if(HTTPLIB_INCLUDE_DIR)
    message(STATUS "  httplib: ${HTTPLIB_INCLUDE_DIR}")
else()
    message(STATUS "  httplib: NOT FOUND (see warning above)")
endif()
message(STATUS "===============================================")
message(STATUS "")

# Additional build targets
add_custom_target(clean-data
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/data
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/data
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/data/blocks
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/data/leveldb
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/data/logs
    COMMENT "Cleaning data directories"
)

# Help target
add_custom_target(help-build
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "QFS Build Help:"
    COMMAND ${CMAKE_COMMAND} -E echo "==============="
    COMMAND ${CMAKE_COMMAND} -E echo "make qfs         - Build the QFS executable"
    COMMAND ${CMAKE_COMMAND} -E echo "make install     - Install QFS to system"
    COMMAND ${CMAKE_COMMAND} -E echo "make clean       - Clean build files"
    COMMAND ${CMAKE_COMMAND} -E echo "make clean-data  - Clean data directories"
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "To run QFS:"
    COMMAND ${CMAKE_COMMAND} -E echo "./qfs            - Start with default settings"
    COMMAND ${CMAKE_COMMAND} -E echo "./qfs --help     - Show usage information"
    COMMAND ${CMAKE_COMMAND} -E echo ""
)