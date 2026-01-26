#ifndef CREDENTIAL_MANAGER_H
#define CREDENTIAL_MANAGER_H

#include "prefix.h"
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <map>

namespace rsjfw {

struct RobloxUser {
    std::string userId;
    std::string username;
    std::string profilePicUrl;
};

class CredentialManager {
public:
  static CredentialManager &instance();

  struct SecurityInfo {
    std::string userId;
    std::string securityCookie;
  };

  std::optional<SecurityInfo> getSecurity(std::shared_ptr<Prefix> prefix);
  std::vector<RobloxUser> getLoggedInUsers(std::shared_ptr<Prefix> prefix);
  void syncAllRunners(std::shared_ptr<Prefix> activePrefix);

private:
  CredentialManager() = default;
};

} // namespace rsjfw

#endif
