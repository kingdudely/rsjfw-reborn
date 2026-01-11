#include "downloader/dxvk_manager.h"
#include "downloader/github_client.h"
#include "path_manager.h"
#include "http.h"
#include "zip_util.h"
#include "logger.h"
#include <fstream>
#include <set>
#include <nlohmann/json.hpp>

namespace rsjfw::downloader {

namespace fs = std::filesystem;
using json = nlohmann::json;

DxvkManager& DxvkManager::instance() {
    static DxvkManager inst;
    return inst;
}

DxvkManager::DxvkManager() {
    auto& pm = PathManager::instance();
    dxvkDir_ = pm.root() / "dxvk"; 
    downloadsDir_ = pm.root() / "downloads";
    fs::create_directories(dxvkDir_);
    fs::create_directories(downloadsDir_);
}

std::vector<std::string> DxvkManager::fetchVersions(const std::string& repo) {
    auto releases = GithubClient::fetchReleases(repo);
    std::vector<std::string> tags;
    for (const auto& r : releases) tags.push_back(r.tag);
    return tags;
}

bool DxvkManager::installVersion(const std::string& repo, const std::string& tag, rsjfw::ProgressCallback cb) {
    auto releases = GithubClient::fetchReleases(repo);
    const GithubRelease* target = nullptr;

    if (tag == "latest" && !releases.empty()) target = &releases[0];
    else {
        for (const auto& r : releases) {
            if (r.tag == tag) {
                target = &r;
                break;
            }
        }
    }

    if (!target) {
        LOG_ERROR("DXVK version not found: %s @ %s", repo.c_str(), tag.c_str());
        return false;
    }

    const GithubAsset* asset = nullptr;
    for (const auto& a : target->assets) {
        if (a.name.find(".tar.gz") != std::string::npos && 
            a.name.find(".asc") == std::string::npos && 
            a.name.find(".sha256") == std::string::npos) {
            asset = &a;
            break;
        }
    }

    if (!asset) {
        LOG_ERROR("No suitable DXVK asset found");
        return false;
    }

    if (cb) cb(0.0f, "Downloading " + asset->name);

    fs::path dlPath = downloadsDir_ / asset->name;
    if (!HTTP::download(asset->url, dlPath.string(), makeSubProgress(0.0f, 0.7f, "DL " + asset->name, cb))) {
        LOG_ERROR("Failed to download asset: %s", asset->url.c_str());
        return false;
    }

    if (cb) cb(0.7f, "Extracting: " + asset->name);

    std::string safeTag = target->tag; 
    
    fs::path installDir = dxvkDir_ / (repo == "doitsujin/dxvk" ? ("official_" + safeTag) : ("custom_" + safeTag));
    fs::create_directories(installDir);

    if (!ZipUtil::extract(dlPath.string(), installDir.string(), [&](float p, std::string s){
        if (cb) cb(0.7f + (p * 0.3f), "Extracting");
    })) {
        LOG_ERROR("Extract failed for %s", dlPath.c_str());
        return false;
    }
    
    fs::path root = installDir;
    for(const auto& p : fs::directory_iterator(installDir)) {
        if (fs::is_directory(p) && fs::exists(p.path() / "x64")) {
            root = p;
            break;
        }
    }
    
    json meta;
    meta["repo"] = repo;
    meta["tag"] = target->tag;
    meta["path"] = root.string(); 
    std::ofstream(root / "rsjfw_dxvk.json") << meta.dump(4);

    fs::remove(dlPath);
    if (cb) cb(1.0f, "Installed to " + root.string());
    return true;
}

std::vector<InstalledDxvk> DxvkManager::getInstalledVersions() {
    std::vector<InstalledDxvk> list;
    if (!fs::exists(dxvkDir_)) return list;

    for (const auto& entry : fs::recursive_directory_iterator(dxvkDir_)) {
        if (entry.is_directory()) {
             fs::path m = entry.path() / "rsjfw_dxvk.json";
             if (fs::exists(m)) {
                 InstalledDxvk d;
                 d.path = entry.path().string();
                 d.name = entry.path().parent_path().filename().string();
                 
                 try {
                     auto j = json::parse(std::ifstream(m));
                     d.repo = j.value("repo", "");
                     d.tag = j.value("tag", "");
                 } catch(...) {}
                 
                 list.push_back(d);
             }
        }
    }
    return list;
}

bool DxvkManager::deleteVersion(const std::string& path) {
    if (fs::exists(path)) {
        fs::remove_all(path);
        return true;
    }
    return false;
}

std::vector<std::string> DxvkManager::getOverrides() const {
    return {"d3d11", "d3d10core", "dxgi", "d3d9"};
}

}
