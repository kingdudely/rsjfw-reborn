#ifndef RSJFW_DOWNLOADER_ROBLOX_MANAGER_H
#define RSJFW_DOWNLOADER_ROBLOX_MANAGER_H

#include <string>
#include <vector>
#include <filesystem>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include "roblox_api.h"
#include "common.h"

namespace rsjfw::downloader {

class RobloxManager {
public:
    static RobloxManager& instance();

    std::string getLatestVersionGUID(const std::string& channel = "LIVE");
    bool isInstalled(const std::string& guid);
    std::vector<std::string> getInstalledVersions();
    bool installVersion(const std::string& guid, rsjfw::ProgressCallback cb = nullptr);
    bool deleteVersion(const std::string& guid);

    bool downloadPackage(const std::string& guid, const RobloxPackage& pkg, const std::string& targetDir, rsjfw::ProgressCallback cb);
    bool extractPackage(const std::string& guid, const RobloxPackage& pkg, const std::string& targetDir, rsjfw::ProgressCallback cb);

private:
    RobloxManager();
    std::filesystem::path versionsDir_;
    std::string getDestinationSubfolder(const std::string& zipName);
};

}

#endif
