#include "config.h"
#include "logger.h"
#include <fstream>
#include <iostream>

namespace rsjfw {

using json = nlohmann::json;

Config& Config::instance() {
    static Config inst;
    return inst;
}

void Config::load(const std::filesystem::path& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    configPath_ = path;
    
    if (!std::filesystem::exists(path)) {
        return;
    }

    try {
        std::ifstream f(path);
        json j = json::parse(f);

        if (j.contains("general")) {
            auto& g = j["general"];
            general_.runnerType = g.value("runnerType", "Wine");
            general_.renderer = g.value("renderer", "D3D11");
            general_.dxvk = g.value("dxvk", true);
            general_.channel = g.value("channel", "LIVE");
            general_.robloxVersion = g.value("robloxVersion", "");
            general_.targetFps = g.value("targetFps", 60);
            general_.lightingTechnology = g.value("lightingTechnology", "Default");
            general_.darkMode = g.value("darkMode", true);

            general_.desktopMode = g.value("desktopMode", false);
            general_.multipleDesktops = g.value("multipleDesktops", false);
            general_.desktopResolution = g.value("desktopResolution", "1920x1080");
            
            general_.launchWrapper = g.value("launchWrapper", "");
            general_.enableMangoHud = g.value("enableMangoHud", false);
            general_.enableFsync = g.value("enableFsync", true);
            general_.enableEsync = g.value("enableEsync", true);
            general_.enableWebView2 = g.value("enableWebView2", true);

            if (g.contains("wineSource")) {
                auto& w = g["wineSource"];
                general_.wineSource.repo = w.value("repo", "vinegarhq/wine-builds");
                general_.wineSource.version = w.value("version", "latest");
                general_.wineSource.asset = w.value("asset", "");
                general_.wineSource.installedRoot = w.value("installedRoot", "");
                general_.wineSource.useCustomRoot = w.value("useCustomRoot", false);
                general_.wineSource.customRootPath = w.value("customRootPath", "");
            }

            if (g.contains("protonSource")) {
                auto& p = g["protonSource"];
                general_.protonSource.repo = p.value("repo", "GloriousEggroll/proton-ge-custom");
                general_.protonSource.version = p.value("version", "latest");
                general_.protonSource.asset = p.value("asset", "");
                general_.protonSource.installedRoot = p.value("installedRoot", "");
                general_.protonSource.useCustomRoot = p.value("useCustomRoot", false);
                general_.protonSource.customRootPath = p.value("customRootPath", "");
            }

            if (g.contains("dxvkSource")) {
                auto& d = g["dxvkSource"];
                general_.dxvkSource.repo = d.value("repo", "doitsujin/dxvk");
                general_.dxvkSource.version = d.value("version", "latest");
                general_.dxvkSource.asset = d.value("asset", "");
                general_.dxvkSource.installedRoot = d.value("installedRoot", "");
            }
            
            if (g.contains("customEnv")) {
                for (auto& [k, v] : g["customEnv"].items()) {
                    general_.customEnv[k] = v.get<std::string>();
                }
            }
        }

        if (j.contains("fflags")) {
            for (auto& [k, v] : j["fflags"].items()) {
                fflags_[k] = v;
            }
        }

    } catch (const std::exception& e) {
        LOG_ERROR("Config load error: %s", e.what());
    }
}

void Config::save() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (configPath_.empty()) return;

    json j;
    j["general"]["runnerType"] = general_.runnerType;
    j["general"]["renderer"] = general_.renderer;
    j["general"]["dxvk"] = general_.dxvk;
    j["general"]["channel"] = general_.channel;
    j["general"]["robloxVersion"] = general_.robloxVersion;
    j["general"]["targetFps"] = general_.targetFps;
    j["general"]["lightingTechnology"] = general_.lightingTechnology;
    j["general"]["darkMode"] = general_.darkMode;

    j["general"]["desktopMode"] = general_.desktopMode;
    j["general"]["multipleDesktops"] = general_.multipleDesktops;
    j["general"]["desktopResolution"] = general_.desktopResolution;
    
    j["general"]["launchWrapper"] = general_.launchWrapper;
    j["general"]["enableMangoHud"] = general_.enableMangoHud;
    j["general"]["enableFsync"] = general_.enableFsync;
    j["general"]["enableEsync"] = general_.enableEsync;
    j["general"]["enableWebView2"] = general_.enableWebView2;

    j["general"]["wineSource"]["repo"] = general_.wineSource.repo;
    j["general"]["wineSource"]["version"] = general_.wineSource.version;
    j["general"]["wineSource"]["asset"] = general_.wineSource.asset;
    j["general"]["wineSource"]["installedRoot"] = general_.wineSource.installedRoot;
    j["general"]["wineSource"]["useCustomRoot"] = general_.wineSource.useCustomRoot;
    j["general"]["wineSource"]["customRootPath"] = general_.wineSource.customRootPath;

    j["general"]["protonSource"]["repo"] = general_.protonSource.repo;
    j["general"]["protonSource"]["version"] = general_.protonSource.version;
    j["general"]["protonSource"]["asset"] = general_.protonSource.asset;
    j["general"]["protonSource"]["installedRoot"] = general_.protonSource.installedRoot;
    j["general"]["protonSource"]["useCustomRoot"] = general_.protonSource.useCustomRoot;
    j["general"]["protonSource"]["customRootPath"] = general_.protonSource.customRootPath;

    j["general"]["dxvkSource"]["repo"] = general_.dxvkSource.repo;
    j["general"]["dxvkSource"]["version"] = general_.dxvkSource.version;
    j["general"]["dxvkSource"]["asset"] = general_.dxvkSource.asset;
    j["general"]["dxvkSource"]["installedRoot"] = general_.dxvkSource.installedRoot;

    j["general"]["customEnv"] = general_.customEnv;
    j["fflags"] = fflags_;

    std::ofstream f(configPath_);
    if (f.is_open()) {
        f << j.dump(4);
        LOG_INFO("Config saved successfully");
    } else {
        LOG_ERROR("Failed to open config file for writing: %s", configPath_.c_str());
    }
}

nlohmann::json Config::getClientAppSettings() const {
    json settings;
    
    for (const auto& [k, v] : fflags_) {
        settings[k] = v;
    }

    if (general_.renderer == "Vulkan") {
        settings["FFlagDebugGraphicsPreferVulkan"] = true;
    } else if (general_.renderer == "OpenGL") {
        settings["FFlagDebugGraphicsPreferOpenGL"] = true;
    } else if (general_.renderer == "D3D11") {
        settings["FFlagDebugGraphicsPreferD3D11"] = true;
    }

    if (general_.targetFps > 0) {
        settings["DFIntTaskSchedulerTargetFps"] = (int)general_.targetFps;
    }

    if (general_.lightingTechnology == "Future") {
        settings["FFlagDebugForceFutureIsBrightPhase3"] = true;
    }
    
    settings["FLogStudioLogToConsole"] = "true";
    settings["FLogStudioLogStdout"] = "true";
    settings["FLogAllChannelStdout"] = "true";

    return settings;
}

}
