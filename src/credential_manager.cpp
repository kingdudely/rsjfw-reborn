#include "credential_manager.h"
#include "logger.h"
#include "rc4.h"
#include <sstream>
#include <iomanip>
#include <random>

namespace rsjfw {

CredentialManager& CredentialManager::instance() {
    static CredentialManager inst;
    return inst;
}

std::string CredentialManager::keyStream(const std::vector<uint8_t>& key, const std::vector<uint8_t>& data) {
    RC4 rc4(key);
    std::vector<uint8_t> result = data;
    rc4.xorStream(result);
    return std::string(result.begin(), result.end());
}

std::optional<CredentialManager::SecurityInfo> CredentialManager::getSecurity(std::shared_ptr<Prefix> prefix) {
    LOG_DEBUG("retrieving roblox credentials...");
    
    std::string credPath = "HKCU\\Software\\Wine\\Credential Manager";
    auto encKeyHex = prefix->registryQuery(credPath, "EncryptionKey");
    
    if (!encKeyHex) {
        LOG_INFO("generating new credential manager encryption key...");
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 255);
        
        std::stringstream ss;
        for (int i = 0; i < 16; i++) {
            ss << std::hex << std::setw(2) << std::setfill('0') << dis(gen);
            if (i < 15) ss << ",";
        }
        
        prefix->registryAdd(credPath, "EncryptionKey", ss.str(), "REG_BINARY");
        if (!prefix->registryCommit()) {
            LOG_ERROR("failed to save encryption key to registry");
            return std::nullopt;
        }
        
        // Re-query to confirm or just use the generated hex
        encKeyHex = ss.str();
    }

    auto hexToBytes = [](const std::string& hex) {
        std::vector<uint8_t> bytes;
        std::stringstream ss(hex);
        std::string segment;
        while (std::getline(ss, segment, ',')) {
            if (segment.empty()) continue;
            try {
                bytes.push_back((uint8_t)std::stoul(segment, nullptr, 16));
            } catch (...) {}
        }
        
        // Handle raw hex without commas if needed (though regedit /e uses commas)
        if (bytes.empty() && !hex.empty()) {
            for (size_t i = 0; i < hex.length(); i += 2) {
                try {
                    bytes.push_back((uint8_t)std::stoul(hex.substr(i, 2), nullptr, 16));
                } catch (...) {}
            }
        }
        return bytes;
    };

    std::vector<uint8_t> key = hexToBytes(*encKeyHex);
    std::string authPrefix = "Generic: https://www.roblox.com:RobloxStudioAuth";
    
    auto userIdEnc = prefix->registryQuery(credPath + "\\" + authPrefix + "userid", "Password");
    if (!userIdEnc) {
        LOG_DEBUG("no active roblox session found in credentials");
        return std::nullopt;
    }

    std::string userId = keyStream(key, hexToBytes(*userIdEnc));
    LOG_INFO("found credentials for user: %s", userId.c_str());

    auto cookieEnc = prefix->registryQuery(credPath + "\\" + authPrefix + ".ROBLOSECURITY" + userId, "Password");
    if (!cookieEnc) {
        LOG_WARN("ROBLOSECURITY cookie not found for user %s", userId.c_str());
        return std::nullopt;
    }

    SecurityInfo info;
    info.userId = userId;
    info.securityCookie = keyStream(key, hexToBytes(*cookieEnc));
    
    return info;
}

}
