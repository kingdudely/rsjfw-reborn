#include "credential_manager.h"
#include "logger.h"
#include "path_manager.h"
#include "roblox_api.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

namespace rsjfw {

using json = nlohmann::json;
namespace fs = std::filesystem;

CredentialManager &CredentialManager::instance() {
  static CredentialManager inst;
  return inst;
}

std::optional<CredentialManager::SecurityInfo>
CredentialManager::getSecurity(std::shared_ptr<Prefix> prefix) {
  return {};
}

static bool hasActualCredentials(std::shared_ptr<Prefix> prefix) {
  std::string path = "HKCU\\Software\\Roblox\\RobloxStudio\\LoggedInUsersStore"
                     "\\https:\\www.roblox.com";
  auto res = prefix->registryQuery(path, "users");
  if (!res)
    return false;

  std::string raw = res.value();
  while (!raw.empty() && (raw.back() == ';' || raw.back() == ' '))
    raw.pop_back();
  if (raw.empty() || raw == "{}" || raw == "null" || raw == "{} ")
    return false;

  // Check for cookies
  std::string credPath = "HKCU\\Software\\Wine\\Credential Manager";
  auto enc = prefix->registryQuery(credPath, "EncryptionKey");
  return enc.has_value();
}

void CredentialManager::syncAllRunners(std::shared_ptr<Prefix> activePrefix) {
  auto &pm = PathManager::instance();

  std::vector<std::string> prefixes = {pm.prefix().string(),
                                       (pm.proton() / "pfx").string(),
                                       (pm.umu() / "pfx").string()};

  std::string activePath = fs::absolute(activePrefix->getPath()).string();
  bool activeHasCreds = hasActualCredentials(activePrefix);

  if (activeHasCreds) {
    LOG_INFO(
        "Active runner has credentials, pushing session to other prefixes...");
    for (const auto &p : prefixes) {
      std::string absP = fs::absolute(p).string();
      if (fs::exists(p) && absP != activePath &&
          fs::exists(fs::path(p) / "user.reg")) {
        LOG_DEBUG("Syncing credentials to prefix at %s", p.c_str());
        Registry targetReg(p);
        targetReg.transplant("HKCU\\Software\\Roblox\\RobloxStudio",
                             activePrefix->getRegistry());
        targetReg.transplant("HKCU\\Software\\Wine\\Credential Manager",
                             activePrefix->getRegistry());
        targetReg.commit();
      }
    }
  } else {
    LOG_INFO(
        "Active runner has NO credentials, searching for session source...");
    std::string bestPrefixPath = "";
    uint64_t bestTime = 0;

    for (const auto &p : prefixes) {
      std::string absP = fs::absolute(p).string();
      if (absP == activePath || !fs::exists(p) ||
          !fs::exists(fs::path(p) / "user.reg"))
        continue;

      auto candidate = std::make_shared<Prefix>(p, "");
      if (hasActualCredentials(candidate)) {
        auto rbxKey = candidate->getRegistry().getCurrentUser()->query(
            "Software\\Roblox\\RobloxStudio");
        if (rbxKey && rbxKey->modified > bestTime) {
          bestTime = rbxKey->modified;
          bestPrefixPath = p;
        }
      }
    }

    if (!bestPrefixPath.empty()) {
      LOG_INFO("Found existing session in %s, pulling to active runner...",
               bestPrefixPath.c_str());
      Registry sourceReg(bestPrefixPath);
      activePrefix->getRegistry().transplant(
          "HKCU\\Software\\Roblox\\RobloxStudio", sourceReg);
      activePrefix->getRegistry().transplant(
          "HKCU\\Software\\Wine\\Credential Manager", sourceReg);
      activePrefix->registryCommit();
    } else {
      LOG_DEBUG("No active sessions found in any prefix.");
    }
  }
}

std::vector<RobloxUser>
CredentialManager::getLoggedInUsers(std::shared_ptr<Prefix> prefix) {
  std::vector<RobloxUser> users;
  std::string path = "HKCU\\Software\\Roblox\\RobloxStudio\\LoggedInUsersStore"
                     "\\https:\\www.roblox.com";
  auto res = prefix->registryQuery(path, "users");

  if (res) {
    std::string raw = res.value();
    while (!raw.empty() && (raw.back() == ';' || raw.back() == ' '))
      raw.pop_back();

    try {
      json j = json::parse(raw);
      for (auto &[id, data] : j.items()) {
        auto info = RobloxAPI::getUserInfo(id);
        RobloxUser user;
        user.userId = id;
        user.username = info.username;
        user.profilePicUrl = info.profilePicUrl;
        users.push_back(user);
      }
    } catch (const std::exception &e) {
      LOG_ERROR("Failed to parse LoggedInUsersStore JSON: %s", e.what());
    }
  }
  return users;
}

} // namespace rsjfw
