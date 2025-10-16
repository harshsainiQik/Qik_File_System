/**
 * Provider Registry Implementation
 *
 * @file provider_registry.cpp
 * @author QFS Development Team
 * @date October 15, 2025
 */

#include "provider_registry.h"
#include <iostream>
#include <algorithm>

namespace qfs {

// Constructor
ProviderRegistry::ProviderRegistry() {
    std::cout << "[INFO] ProviderRegistry initialized" << std::endl;
}

// Destructor
ProviderRegistry::~ProviderRegistry() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::cout << "[INFO] ProviderRegistry destroyed (had "
              << registry_.size() << " CIDs)" << std::endl;
}

// Add or update provider
void ProviderRegistry::addProvider(const std::string& cid, const ProviderInfo& info) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Get or create provider list for this CID
    auto& providers = registry_[cid];

    // Check if provider already exists (by nodeId)
    for (auto& existing : providers) {
        if (existing.nodeId == info.nodeId) {
            // Update existing provider
            existing = info;
            std::cout << "[INFO] Updated provider: " << info.nodeId
                      << " for CID: " << cid.substr(0, 12) << "..." << std::endl;
            return;
        }
    }

    // Add new provider
    providers.push_back(info);
    std::cout << "[INFO] Added provider: " << info.nodeId
              << " (" << info.ip << ":" << info.port << ")"
              << " for CID: " << cid.substr(0, 12) << "..." << std::endl;
}

// Get all providers for a CID
std::vector<ProviderInfo> ProviderRegistry::getProviders(const std::string& cid) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = registry_.find(cid);
    if (it == registry_.end()) {
        // No providers found
        return {};
    }

    // Return copy of provider list
    return it->second;
}

// Check if providers exist
bool ProviderRegistry::hasProviders(const std::string& cid) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = registry_.find(cid);
    return (it != registry_.end() && !it->second.empty());
}

// Remove stale providers
size_t ProviderRegistry::removeStaleProviders(int timeoutSeconds) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto now = std::chrono::system_clock::now();
    size_t removedCount = 0;

    // Iterate through all CIDs
    for (auto it = registry_.begin(); it != registry_.end(); ) {
        auto& providers = it->second;

        // Remove stale providers from this CID's list
        auto originalSize = providers.size();

        providers.erase(
            std::remove_if(providers.begin(), providers.end(),
                [&](const ProviderInfo& p) {
                    auto age = std::chrono::duration_cast<std::chrono::seconds>(
                        now - p.lastSeen
                    ).count();

                    bool isStale = (age > timeoutSeconds);

                    if (isStale) {
                        std::cout << "[INFO] Removing stale provider: " << p.nodeId
                                  << " (age: " << age << "s)" << std::endl;
                    }

                    return isStale;
                }),
            providers.end()
        );

        removedCount += (originalSize - providers.size());

        // If no providers left for this CID, remove the CID entry
        if (providers.empty()) {
            std::cout << "[INFO] Removing CID with no providers: "
                      << it->first.substr(0, 12) << "..." << std::endl;
            it = registry_.erase(it);
        } else {
            ++it;
        }
    }

    if (removedCount > 0) {
        std::cout << "[INFO] Removed " << removedCount << " stale provider(s)" << std::endl;
    }

    return removedCount;
}

// Get total number of CIDs
size_t ProviderRegistry::getTotalCIDs() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return registry_.size();
}

// Get total number of providers
size_t ProviderRegistry::getTotalProviders() const {
    std::lock_guard<std::mutex> lock(mutex_);

    size_t total = 0;
    for (const auto& [cid, providers] : registry_) {
        total += providers.size();
    }

    return total;
}

// Clear all providers
void ProviderRegistry::clear() {
    std::lock_guard<std::mutex> lock(mutex_);

    size_t cidCount = registry_.size();
    size_t providerCount = getTotalProviders();

    registry_.clear();

    std::cout << "[INFO] Registry cleared: " << cidCount << " CIDs, "
              << providerCount << " providers removed" << std::endl;
}

} // namespace qfs
