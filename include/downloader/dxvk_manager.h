#ifndef RSJFW_DOWNLOADER_DXVK_MANAGER_H
#define RSJFW_DOWNLOADER_DXVK_MANAGER_H

#include <string>
#include <vector>
#include <filesystem>
#include "github_types.h"
#include "common.h"

namespace rsjfw::downloader {

struct InstalledDxvk {
    std::string name;
    std::string path;
    std::string repo;
    std::string tag;
};

class DxvkManager {
public:
    static DxvkManager& instance();

    std::vector<std::string> fetchVersions(const std::string& repo);
    bool installVersion(const std::string& repo, const std::string& tag, rsjfw::ProgressCallback cb = nullptr);
    std::vector<InstalledDxvk> getInstalledVersions();
    bool deleteVersion(const std::string& path);

    std::vector<std::string> getOverrides() const;

private:
    DxvkManager();
    std::filesystem::path dxvkDir_;
    std::filesystem::path downloadsDir_;
};

}

#endif
