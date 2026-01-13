#ifndef CREDENTIAL_MANAGER_H
#define CREDENTIAL_MANAGER_H

#include <string>
#include <vector>
#include <memory>
#include "prefix.h"

namespace rsjfw {

class CredentialManager {
public:
    static CredentialManager& instance();

    struct SecurityInfo {
        std::string userId;
        std::string securityCookie;
    };

    std::optional<SecurityInfo> getSecurity(std::shared_ptr<Prefix> prefix);

private:
    CredentialManager() = default;
    std::string keyStream(const std::vector<uint8_t>& key, const std::vector<uint8_t>& data);
};

}

#endif
