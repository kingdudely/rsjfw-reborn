#include "downloader/roblox_manager.h"
#include "path_manager.h"
#include "http.h"
#include "zip_util.h"
#include "logger.h"
#include <fstream>
#include <unordered_map>
#include <thread>
#include <iostream>

namespace rsjfw::downloader {

namespace fs = std::filesystem;

RobloxManager& RobloxManager::instance() {
    static RobloxManager inst;
    return inst;
}

RobloxManager::RobloxManager() {
    versionsDir_ = PathManager::instance().versions();
}

std::string RobloxManager::getLatestVersionGUID(const std::string& channel) {
    return RobloxAPI::getLatestVersionGUID(channel);
}

bool RobloxManager::isInstalled(const std::string& guid) {
    return fs::exists(versionsDir_ / guid / "AppSettings.xml");
}

std::vector<std::string> RobloxManager::getInstalledVersions() {
    std::vector<std::string> vers;
    if (!fs::exists(versionsDir_)) return vers;
    for (const auto& entry : fs::directory_iterator(versionsDir_)) {
        if (entry.is_directory() && fs::exists(entry.path() / "AppSettings.xml")) {
            vers.push_back(entry.path().filename().string());
        }
    }
    return vers;
}

bool RobloxManager::deleteVersion(const std::string& guid) {
    fs::path p = versionsDir_ / guid;
    if (fs::exists(p)) {
        fs::remove_all(p);
        return true;
    }
    return false;
}

std::string RobloxManager::getDestinationSubfolder(const std::string& zipName) {
    static const std::unordered_map<std::string, std::string> map = {
        {"ApplicationConfig.zip", "ApplicationConfig/"},
        {"content-avatar.zip", "content/avatar/"},
        {"content-configs.zip", "content/configs/"},
        {"content-fonts.zip", "content/fonts/"},
        {"content-sky.zip", "content/sky/"},
        {"content-sounds.zip", "content/sounds/"},
        {"content-textures2.zip", "content/textures/"},
        {"content-studio_svg_textures.zip", "content/studio_svg_textures/"},
        {"content-models.zip", "content/models/"},
        {"content-textures3.zip", "PlatformContent/pc/textures/"},
        {"content-terrain.zip", "PlatformContent/pc/terrain/"},
        {"content-platform-fonts.zip", "PlatformContent/pc/fonts/"},
        {"content-platform-dictionaries.zip", "PlatformContent/pc/shared_compression_dictionaries/"},
        {"content-qt_translations.zip", "content/qt_translations/"},
        {"content-api-docs.zip", "content/api_docs/"},
        {"extracontent-scripts.zip", "ExtraContent/scripts/"},
        {"extracontent-luapackages.zip", "ExtraContent/LuaPackages/"},
        {"extracontent-translations.zip", "ExtraContent/translations/"},
        {"extracontent-models.zip", "ExtraContent/models/"},
        {"extracontent-textures.zip", "ExtraContent/textures/"},
        {"studiocontent-models.zip", "StudioContent/models/"},
        {"studiocontent-textures.zip", "StudioContent/textures/"},
        {"shaders.zip", "shaders/"},
        {"BuiltInPlugins.zip", "BuiltInPlugins/"},
        {"BuiltInStandalonePlugins.zip", "BuiltInStandalonePlugins/"},
        {"Plugins.zip", "Plugins/"},
        {"RibbonConfig.zip", "RibbonConfig/"},
        {"StudioFonts.zip", "StudioFonts/"},
        {"ssl.zip", "ssl/"}
    };
    auto it = map.find(zipName);
    return (it != map.end()) ? it->second : "";
}

bool RobloxManager::downloadPackage(const std::string& guid, const RobloxPackage& pkg, const std::string& targetDir, rsjfw::ProgressCallback cb) {
    auto& pm = PathManager::instance();
    std::string url = "https://setup.rbxcdn.com/" + guid + "-" + pkg.name;
    fs::path cachePath = pm.cache() / (guid + "_" + pkg.name);

    if (!fs::exists(cachePath)) {
        if (!HTTP::download(url, cachePath.string(), cb)) {
            fs::remove(cachePath);
            return false;
        }
    }
    return true;
}

bool RobloxManager::extractPackage(const std::string& guid, const RobloxPackage& pkg, const std::string& targetDir, rsjfw::ProgressCallback cb) {
    auto& pm = PathManager::instance();
    fs::path cachePath = pm.cache() / (guid + "_" + pkg.name);
    fs::path destSub = fs::path(targetDir) / getDestinationSubfolder(pkg.name);
    fs::create_directories(destSub);
    return ZipUtil::extract(cachePath.string(), destSub.string(), cb);
}

struct TaskState {
    std::mutex mtx;
    std::condition_variable cv;
    int activeDownloads = 0;
    int activeExtracts = 0;
    std::queue<RobloxPackage> downloadQueue;
    std::queue<RobloxPackage> extractQueue;
    std::atomic<int> completedPackages{0};
    int totalPackages = 0;
    bool failed = false;
};

static void workerLoop(std::shared_ptr<TaskState> state, int type, std::string guid, std::string targetDir, RobloxManager* mgr, rsjfw::ProgressCallback mainCb) {
    while (true) {
        RobloxPackage pkg;
        bool isDownload = (type == 0);

        {
            std::unique_lock<std::mutex> lk(state->mtx);
            if (state->failed) return;
            if (state->completedPackages == state->totalPackages) return;

            if (isDownload) {
                if (state->downloadQueue.empty()) return;
                pkg = state->downloadQueue.front();
                state->downloadQueue.pop();
                state->activeDownloads++;
            } else {
                state->cv.wait(lk, [&] {
                    return !state->extractQueue.empty() || state->failed ||
                           (state->downloadQueue.empty() && state->activeDownloads == 0);
                });

                if (state->failed || (state->extractQueue.empty() && state->downloadQueue.empty() && state->activeDownloads == 0))
                    return;

                pkg = state->extractQueue.front();
                state->extractQueue.pop();
                state->activeExtracts++;
            }
        }

        bool ok = false;
        if (isDownload) ok = mgr->downloadPackage(guid, pkg, targetDir, nullptr);
        else ok = mgr->extractPackage(guid, pkg, targetDir, nullptr);

        {
            std::unique_lock<std::mutex> lk(state->mtx);
            if (!ok) {
                state->failed = true;
                state->cv.notify_all();
                return;
            }

            if (isDownload) {
                state->activeDownloads--;
                state->extractQueue.push(pkg);
                state->cv.notify_all();
            } else {
                state->activeExtracts--;
                state->completedPackages++;
                if (mainCb) {
                    float p = (float)state->completedPackages / state->totalPackages;
                    mainCb(p, "Installed " + std::to_string(state->completedPackages) + "/" + std::to_string(state->totalPackages));
                }
            }
        }
    }
}

bool RobloxManager::installVersion(const std::string& guid, rsjfw::ProgressCallback cb) {
    if (isInstalled(guid)) {
        if (cb) cb(1.0f, "Version already installed");
        return true;
    }

    try {
        if (cb) cb(0.0f, "Fetching Manifest...");
        auto pkgs = RobloxAPI::getPackageManifest(guid);

        size_t total = pkgs.size();
        fs::path targetDir = versionsDir_ / guid;
        fs::create_directories(targetDir);

        auto state = std::make_shared<TaskState>();
        state->totalPackages = total;
        for (const auto& p : pkgs) state->downloadQueue.push(p);

        std::vector<std::thread> threads;
        for (int i = 0; i < 4; ++i) threads.emplace_back(workerLoop, state, 0, guid, targetDir.string(), this, cb);
        for (int i = 0; i < 3; ++i) threads.emplace_back(workerLoop, state, 1, guid, targetDir.string(), this, cb);
        for (auto& t : threads) t.join();

        if (state->failed) return false;

        fs::path settingsPath = targetDir / "AppSettings.xml";
        std::ofstream ofs(settingsPath);
        ofs << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n<Settings>\r\n\t<ContentFolder>content</ContentFolder>\r\n\t<BaseUrl>http://www.roblox.com</BaseUrl>\r\n</Settings>\r\n";
        
        if (cb) cb(1.0f, "Complete");
        return true;

    } catch (const std::exception& e) {
        LOG_ERROR("Install Error: %s", e.what());
        return false;
    }
}

}
