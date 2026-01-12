#include "discovery_manager.h"
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <iostream>
#include <algorithm>

namespace rsjfw {

namespace fs = std::filesystem;

DiscoveryManager& DiscoveryManager::instance() {
    static DiscoveryManager inst;
    return inst;
}

void DiscoveryManager::scan() {
    runners_.clear();
    scanSteamProton();
    scanSystemWine();
}

std::vector<RunnerInfo> DiscoveryManager::getRunners(RunnerType type) const {
    std::vector<RunnerInfo> filtered;
    for (const auto& r : runners_) {
        if (r.type == type) filtered.push_back(r);
    }
    return filtered;
}

std::vector<RunnerInfo> DiscoveryManager::getAllRunners() const {
    return runners_;
}

void DiscoveryManager::scanSteamProton() {
    std::vector<fs::path> steamPaths;
    const char* home = getenv("HOME");
    if (home) {
        fs::path h(home);
        steamPaths.push_back(h / ".steam/root/compatibilitytools.d");
        steamPaths.push_back(h / ".local/share/Steam/compatibilitytools.d");
        steamPaths.push_back(h / ".var/app/com.valvesoftware.Steam/.local/share/Steam/compatibilitytools.d");
    }
    for (const auto& dir : steamPaths) {
        if (!fs::exists(dir)) continue;
        for (const auto& entry : fs::directory_iterator(dir)) {
            if (entry.is_directory()) {
                fs::path p = entry.path();
                if (fs::exists(p / "proton") || fs::exists(p / "compatibilitytool.vdf")) {
                    RunnerInfo r;
                    r.name = p.filename().string();
                    r.path = p;
                    r.type = RunnerType::Proton;
                    r.version = r.name;
                    runners_.push_back(r);
                }
            }
        }
    }
}

void DiscoveryManager::scanSystemWine() {
    if (system("which wine > /dev/null 2>&1") == 0) {
        RunnerInfo r;
        r.name = "system wine";
        r.path = "/usr/bin/wine";
        r.type = RunnerType::System;
        r.version = "system";
        runners_.push_back(r);
    }
}

}
