/**
 * Simple test program for Provider Registry
 *
 * This program tests the basic functionality of the ProviderRegistry class.
 */

#include "provider_registry.h"
#include <iostream>
#include <cassert>

using namespace qfs;

void testBasicAddAndGet() {
    std::cout << "\n=== Test 1: Basic Add and Get ===" << std::endl;

    ProviderRegistry registry;

    // Create provider info
    ProviderInfo alice("alice-node", "192.168.1.100", 8080, 1000000);

    // Add provider
    registry.addProvider("QmTest123", alice);

    // Check if providers exist
    assert(registry.hasProviders("QmTest123") == true);
    assert(registry.hasProviders("QmNonExistent") == false);

    // Get providers
    auto providers = registry.getProviders("QmTest123");
    assert(providers.size() == 1);
    assert(providers[0].nodeId == "alice-node");
    assert(providers[0].ip == "192.168.1.100");
    assert(providers[0].port == 8080);

    std::cout << "✅ Test 1 passed!" << std::endl;
}

void testMultipleProviders() {
    std::cout << "\n=== Test 2: Multiple Providers ===" << std::endl;

    ProviderRegistry registry;

    // Add multiple providers for same CID
    ProviderInfo alice("alice", "192.168.1.100", 8080, 1000000);
    ProviderInfo bob("bob", "192.168.1.200", 8080, 500000);
    ProviderInfo charlie("charlie", "192.168.1.150", 8080, 2000000);

    registry.addProvider("QmPhoto", alice);
    registry.addProvider("QmPhoto", bob);
    registry.addProvider("QmPhoto", charlie);

    // Get all providers
    auto providers = registry.getProviders("QmPhoto");
    assert(providers.size() == 3);

    std::cout << "✅ Test 2 passed! Found " << providers.size() << " providers" << std::endl;
}

void testUpdateProvider() {
    std::cout << "\n=== Test 3: Update Existing Provider ===" << std::endl;

    ProviderRegistry registry;

    // Add Alice
    ProviderInfo alice("alice", "192.168.1.100", 8080, 1000000);
    registry.addProvider("QmFile", alice);

    // Update Alice with new bandwidth
    ProviderInfo aliceUpdated("alice", "192.168.1.100", 8080, 2000000);
    registry.addProvider("QmFile", aliceUpdated);

    // Should still have only 1 provider (updated, not duplicated)
    auto providers = registry.getProviders("QmFile");
    assert(providers.size() == 1);
    assert(providers[0].bandwidth == 2000000);  // Updated value

    std::cout << "✅ Test 3 passed! Provider updated correctly" << std::endl;
}

void testEmptyRegistry() {
    std::cout << "\n=== Test 4: Empty Registry ===" << std::endl;

    ProviderRegistry registry;

    // Query non-existent CID
    auto providers = registry.getProviders("QmNonExistent");
    assert(providers.empty());
    assert(!registry.hasProviders("QmNonExistent"));

    // Statistics
    assert(registry.getTotalCIDs() == 0);
    assert(registry.getTotalProviders() == 0);

    std::cout << "✅ Test 4 passed!" << std::endl;
}

void testStatistics() {
    std::cout << "\n=== Test 5: Registry Statistics ===" << std::endl;

    ProviderRegistry registry;

    // Add providers
    ProviderInfo alice("alice", "192.168.1.100", 8080, 1000000);
    ProviderInfo bob("bob", "192.168.1.200", 8080, 500000);

    registry.addProvider("QmFile1", alice);
    registry.addProvider("QmFile1", bob);
    registry.addProvider("QmFile2", alice);

    // Check statistics
    assert(registry.getTotalCIDs() == 2);      // 2 different CIDs
    assert(registry.getTotalProviders() == 3); // 3 total providers

    std::cout << "✅ Test 5 passed!" << std::endl;
    std::cout << "  Total CIDs: " << registry.getTotalCIDs() << std::endl;
    std::cout << "  Total Providers: " << registry.getTotalProviders() << std::endl;
}

void testClear() {
    std::cout << "\n=== Test 6: Clear Registry ===" << std::endl;

    ProviderRegistry registry;

    // Add some providers
    ProviderInfo alice("alice", "192.168.1.100", 8080, 1000000);
    registry.addProvider("QmFile1", alice);
    registry.addProvider("QmFile2", alice);

    assert(registry.getTotalCIDs() == 2);

    // Clear registry
    registry.clear();

    assert(registry.getTotalCIDs() == 0);
    assert(registry.getTotalProviders() == 0);
    assert(!registry.hasProviders("QmFile1"));

    std::cout << "✅ Test 6 passed!" << std::endl;
}

void testJsonConversion() {
    std::cout << "\n=== Test 7: JSON Conversion ===" << std::endl;

    ProviderInfo alice("alice-node", "192.168.1.100", 8080, 1000000);

    std::string json = alice.toJson();

    // Check JSON contains expected fields
    assert(json.find("alice-node") != std::string::npos);
    assert(json.find("192.168.1.100") != std::string::npos);
    assert(json.find("8080") != std::string::npos);

    std::cout << "✅ Test 7 passed!" << std::endl;
    std::cout << "  JSON output:\n" << json << std::endl;
}

int main() {
    std::cout << "\n╔══════════════════════════════════════╗" << std::endl;
    std::cout << "║  Provider Registry Test Suite       ║" << std::endl;
    std::cout << "╚══════════════════════════════════════╝" << std::endl;

    try {
        testBasicAddAndGet();
        testMultipleProviders();
        testUpdateProvider();
        testEmptyRegistry();
        testStatistics();
        testClear();
        testJsonConversion();

        std::cout << "\n╔══════════════════════════════════════╗" << std::endl;
        std::cout << "║  ✅ ALL TESTS PASSED!               ║" << std::endl;
        std::cout << "╚══════════════════════════════════════╝\n" << std::endl;

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "\n❌ TEST FAILED: " << e.what() << std::endl;
        return 1;
    }
}
