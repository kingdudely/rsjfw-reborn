#include "preset_manager.h"
#include "config.h"
#include "path_manager.h"
#include "logger.h"
#include <fstream>
#include <iostream>

namespace rsjfw {

namespace fs = std::filesystem;
using json = nlohmann::json;

PresetManager& PresetManager::instance() {
    static PresetManager inst;
    return inst;
}

PresetManager::PresetManager() {
    auto& pm = PathManager::instance();
    presetsDir_ = pm.root() / "presets";
    if (!fs::exists(presetsDir_)) {
        fs::create_directories(presetsDir_);
    }
    refreshPresets();
}

void PresetManager::refreshPresets() {
    presets_.clear();
    if (!fs::exists(presetsDir_)) return;

    for (const auto& entry : fs::directory_iterator(presetsDir_)) {
        if (entry.is_regular_file() && entry.path().extension() == ".rsjfwpreset") {
            PresetInfo info;
            info.name = entry.path().stem().string();
            info.path = entry.path();
            presets_.push_back(info);
        }
    }
}

bool PresetManager::savePreset(const std::string& name) {
    if (name.empty()) return false;
    json j = Config::instance().serialize();
    j = sanitizeForExport(j);
    fs::path p = presetsDir_ / (name + ".rsjfwpreset");
    std::ofstream f(p);
    if (f.is_open()) {
        f << j.dump(4);
        refreshPresets();
        return true;
    }
    return false;
}

bool PresetManager::loadPreset(const std::string& name) {
    fs::path p = presetsDir_ / (name + ".rsjfwpreset");
    if (!fs::exists(p)) return false;
    try {
        std::ifstream f(p);
        json j = json::parse(f);
        Config::instance().deserialize(j);
        return true;
    } catch (...) {
        return false;
    }
}

bool PresetManager::deletePreset(const std::string& name) {
    fs::path p = presetsDir_ / (name + ".rsjfwpreset");
    if (fs::exists(p)) {
        fs::remove(p);
        refreshPresets();
        return true;
    }
    return false;
}

bool PresetManager::importPreset(const fs::path& path) {
    if (!fs::exists(path)) return false;
    try {
        std::ifstream f(path);
        json j = json::parse(f);
        if (!j.contains("general") && !j.contains("fflags")) return false;
        std::string name = path.stem().string();
        fs::path dest = presetsDir_ / (name + ".rsjfwpreset");
        if (fs::exists(dest)) {
            dest = presetsDir_ / (name + "_imported.rsjfwpreset");
        }
        fs::copy_file(path, dest);
        refreshPresets();
        return true;
    } catch (...) {
        return false;
    }
}

bool PresetManager::exportPreset(const std::string& name, const fs::path& dest) {
    fs::path src = presetsDir_ / (name + ".rsjfwpreset");
    if (!fs::exists(src)) return false;
    try {
        std::ifstream f(src);
        json j = json::parse(f);
        auto checkPath = [](const std::string& path) {
            if (path.find(".steam") != std::string::npos || 
                path.find("Steam") != std::string::npos ||
                path.find("compatibilitytools.d") != std::string::npos) {
                return true;
            }
            return false;
        };
        if (j["general"].contains("wineSource")) {
            auto& w = j["general"]["wineSource"];
            if (w.value("useCustomRoot", false)) {
                std::string p = w.value("customRootPath", "");
                if (checkPath(p)) {
                    LOG_ERROR("cannot export preset: specific path detected");
                    return false;
                }
            }
        }
        if (j["general"].contains("protonSource")) {
            auto& p = j["general"]["protonSource"];
            if (p.value("useCustomRoot", false)) {
                std::string path = p.value("customRootPath", "");
                if (checkPath(path)) {
                    LOG_ERROR("cannot export preset: specific path detected");
                    return false;
                }
            }
        }
        std::ofstream out(dest);
        out << j.dump(4);
        return true;
    } catch (...) {
        return false;
    }
}

json PresetManager::sanitizeForExport(const json& inJ) {
    json j = inJ;
    auto sanitizeSource = [](json& s) {
        if (s.contains("useCustomRoot") && s["useCustomRoot"] == true) {
            std::string p = s.value("customRootPath", "");
            bool isSystem = (p.find("/usr/") == 0 || p.find("/opt/") == 0 || p.find("/bin/") == 0);
            if (!isSystem) {
                s["useCustomRoot"] = false;
                s["customRootPath"] = "";
            }
        }
        s["installedRoot"] = ""; 
    };
    if (j["general"].contains("wineSource")) sanitizeSource(j["general"]["wineSource"]);
    if (j["general"].contains("protonSource")) sanitizeSource(j["general"]["protonSource"]);
    if (j["general"].contains("dxvkSource")) {
        j["general"]["dxvkSource"]["installedRoot"] = "";
    }
    return j;
}

}
