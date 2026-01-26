#include "roblox_api.h"
#include "http.h"
#include "logger.h"
#include <nlohmann/json.hpp>
#include <sstream>
#include <vector>

namespace rsjfw {

const std::string RobloxAPI::BASE_URL = "https://setup.rbxcdn.com/";

std::string RobloxAPI::getLatestVersionGUID(const std::string &channel) {
  std::string url =
      "https://clientsettings.roblox.com/v2/client-version/WindowsStudio64";
  if (channel != "LIVE")
    url += "?channel=" + channel;

  std::string resp = HTTP::get(url);

  auto j = nlohmann::json::parse(resp);
  return j["clientVersionUpload"];
}

std::vector<RobloxPackage>
RobloxAPI::getPackageManifest(const std::string &guid) {
  std::string url = BASE_URL + guid + "-rbxPkgManifest.txt";
  std::string response = HTTP::get(url);

  std::vector<RobloxPackage> packages;
  std::stringstream ss(response);
  std::string line;

  if (!std::getline(ss, line))
    return packages;

  while (std::getline(ss, line)) {
    if (line.empty())
      continue;

    RobloxPackage pkg;
    pkg.name = trim(line);

    if (!std::getline(ss, line))
      break;
    pkg.checksum = trim(line);

    if (!std::getline(ss, line))
      break;
    try {
      pkg.size = std::stoull(trim(line));
    } catch (...) {
      pkg.size = 0;
    }

    if (!std::getline(ss, line))
      break;
    try {
      pkg.packedSize = std::stoull(trim(line));
    } catch (...) {
      pkg.packedSize = 0;
    }

    packages.push_back(pkg);
  }

  return packages;
}

RobloxUserInfo RobloxAPI::getUserInfo(const std::string &userId) {
  RobloxUserInfo info;
  info.userId = userId;

  try {
    std::string userUrl = "https://users.roblox.com/v1/users/" + userId;
    std::string userResp = HTTP::get(userUrl);
    auto userJ = nlohmann::json::parse(userResp);

    info.username = userJ.value("name", "Unknown");
    info.displayName = userJ.value("displayName", info.username);

    std::string thumbUrl =
        "https://thumbnails.roblox.com/v1/users/avatar-headshot?userIds=" +
        userId + "&size=150x150&format=Png&isCircular=false";
    std::string thumbResp = HTTP::get(thumbUrl);
    auto thumbJ = nlohmann::json::parse(thumbResp);

    if (thumbJ.contains("data") && !thumbJ["data"].empty()) {
      info.profilePicUrl = thumbJ["data"][0].value("imageUrl", "");
    }
  } catch (const std::exception &e) {
    LOG_ERROR("Failed to fetch user info for %s: %s", userId.c_str(), e.what());
    info.username = "Error";
  }

  return info;
}

} // namespace rsjfw
