cmake_minimum_required(VERSION 3.15)
project(QFSBootstrapNode VERSION 1.0.0 LANGUAGES CXX)

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
include_directories(${CMAKE_SOURCE_DIR}/../third_party)

# Set third-party paths (relative to parent project)
set(THIRD_PARTY_DIR "${CMAKE_SOURCE_DIR}/../third_party")

# Find OpenSSL (for future cryptographic operations)
find_package(OpenSSL REQUIRED)
if(OPENSSL_FOUND)
    message(STATUS "Found OpenSSL: ${OPENSSL_VERSION}")
else()
    message(FATAL_ERROR "OpenSSL not found. Please install OpenSSL development libraries.")
endif()

# Use local nlohmann-json from parent project
set(NLOHMANN_JSON_INCLUDE_DIR "${THIRD_PARTY_DIR}/nlohmann-json")
if(EXISTS ${NLOHMANN_JSON_INCLUDE_DIR}/json.hpp)
    message(STATUS "Found nlohmann-json in third_party: ${NLOHMANN_JSON_INCLUDE_DIR}")
    set(JSON_FOUND TRUE)
else()
    message(FATAL_ERROR "nlohmann-json not found in third_party/nlohmann-json/. Please ensure json.hpp is in that directory.")
endif()

# Use local cpp-httplib from parent project
set(HTTPLIB_INCLUDE_DIR "${THIRD_PARTY_DIR}/cpp-httplib")
if(EXISTS ${HTTPLIB_INCLUDE_DIR}/httplib.h)
    message(STATUS "Found cpp-httplib in third_party: ${HTTPLIB_INCLUDE_DIR}")
else()
    message(FATAL_ERROR "cpp-httplib not found in third_party/cpp-httplib/. Please ensure httplib.h is in that directory.")
endif()

# Source files
set(BOOTSTRAP_SOURCES
    src/main.cpp
    src/bootstrap_server.cpp
    src/provider_registry.cpp
)

# Header files (for IDE support)
set(BOOTSTRAP_HEADERS
    include/bootstrap_server.h
    include/provider_registry.h
)

# Create executable
add_executable(qfs-bootstrap ${BOOTSTRAP_SOURCES} ${BOOTSTRAP_HEADERS})

# Link libraries
target_link_libraries(qfs-bootstrap
    ${OPENSSL_LIBRARIES}
    pthread
)

# Include directories for the target
target_include_directories(qfs-bootstrap PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${THIRD_PARTY_DIR}
    ${OPENSSL_INCLUDE_DIR}
    ${NLOHMANN_JSON_INCLUDE_DIR}
    ${HTTPLIB_INCLUDE_DIR}
)

# Platform-specific settings
if(WIN32)
    target_link_libraries(qfs-bootstrap ws2_32 wsock32)
    # Use Windows 10 version (0x0A00) for httplib compatibility
    target_compile_definitions(qfs-bootstrap PRIVATE _WIN32_WINNT=0x0A00)
endif()

# Create directories for runtime
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/data)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/data/logs)

# Installation
install(TARGETS qfs-bootstrap DESTINATION bin)

# Install example config
install(FILES
    ${CMAKE_SOURCE_DIR}/config/bootstrap.json.example
    DESTINATION etc/qfs-bootstrap
    OPTIONAL
)

# Print build information
message(STATUS "")
message(STATUS "===============================================")
message(STATUS "QFS Bootstrap Node Build Configuration")
message(STATUS "===============================================")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "")
message(STATUS "Dependencies:")
message(STATUS "  OpenSSL: ${OPENSSL_VERSION}")
if(HTTPLIB_INCLUDE_DIR)
    message(STATUS "  httplib: ${HTTPLIB_INCLUDE_DIR}")
endif()
if(NLOHMANN_JSON_INCLUDE_DIR)
    message(STATUS "  nlohmann-json: ${NLOHMANN_JSON_INCLUDE_DIR}")
endif()
message(STATUS "===============================================")
message(STATUS "")
