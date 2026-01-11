#ifndef RSJFW_DOWNLOADER_WINE_MANAGER_H
#define RSJFW_DOWNLOADER_WINE_MANAGER_H

#include <string>
#include <vector>
#include <filesystem>
#include "github_types.h"
#include "prefix.h" 

namespace rsjfw::downloader {

struct InstalledWine {
    std::string name;
    std::string path;
    std::string repo;
    std::string tag;
    std::string asset;
};

class WineManager {
public:
    static WineManager& instance();

    std::vector<std::string> fetchVersions(const std::string& repo);
    bool installVersion(const std::string& repo, const std::string& tag, const std::string& assetName = "", ProgressCb cb = nullptr);
    std::vector<InstalledWine> getInstalledVersions();
    bool deleteVersion(const std::string& path);

private:
    WineManager();
    std::filesystem::path wineDir_;
    std::filesystem::path downloadsDir_;
};

}

#endif
