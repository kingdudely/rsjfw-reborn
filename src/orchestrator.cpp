#include "orchestrator.h"
#include "config.h"
#include "gui.h"
#include "downloader/roblox_manager.h"
#include "downloader/wine_manager.h"
#include "downloader/dxvk_manager.h"
#include "path_manager.h"
#include "runner.h"
#include "logger.h"

#include <iostream>
#include <chrono>
#include <fstream>
#include <nlohmann/json.hpp>
#include <sstream>
#include <filesystem>
#include <unistd.h>
#include <algorithm>

namespace rsjfw {

Orchestrator& Orchestrator::instance() {
    static Orchestrator inst;
    return inst;
}

Orchestrator::~Orchestrator() {
    cancel();
    if (workerThread_.joinable()) workerThread_.join();
}

static std::string stateToString(LauncherState s) {
    switch (s) {
        case LauncherState::IDLE: return "IDLE";
        case LauncherState::BOOTSTRAPPING: return "BOOTSTRAPPING";
        case LauncherState::DOWNLOADING_ROBLOX: return "DOWNLOADING_ROBLOX";
        case LauncherState::PREPARING_WINE: return "PREPARING_WINE";
        case LauncherState::INSTALLING_DXVK: return "INSTALLING_DXVK";
        case LauncherState::APPLYING_CONFIG: return "APPLYING_CONFIG";
        case LauncherState::LAUNCHING_STUDIO: return "LAUNCHING_STUDIO";
        case LauncherState::RUNNING: return "RUNNING";
        case LauncherState::FINISHED: return "FINISHED";
        case LauncherState::ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

void Orchestrator::startLaunch(const std::string& arg) {
    if (state_ != LauncherState::IDLE && state_ != LauncherState::FINISHED && state_ != LauncherState::ERROR) return;
    
    stop_ = false;
    setState(LauncherState::BOOTSTRAPPING);
    if (workerThread_.joinable()) workerThread_.join();
    workerThread_ = std::thread(&Orchestrator::worker, this, arg);
}

void Orchestrator::cancel() {
    stop_ = true;
}

void Orchestrator::setStatus(float p, const std::string& s) {
    std::lock_guard<std::mutex> l(mutex_);
    progress_ = p;
    status_ = s;
    GUI::instance().setProgress(p, s);
}

void Orchestrator::setState(LauncherState s) {
    LOG_INFO("State Transition: %s", stateToString(s).c_str());
    state_ = s;
}

void Orchestrator::setError(const std::string& e) {
    std::lock_guard<std::mutex> l(mutex_);
    error_ = e;
    state_ = LauncherState::ERROR;
    setStatus(1.0f, "Error: " + e);
}

void Orchestrator::worker(std::string arg) {
    try {
        bool installOnly = (arg == "INSTALL_ONLY");
        std::vector<std::string> launchArgs;
        bool isProtocol = false;
        
        if (!installOnly) {
            if (arg.find("roblox-studio-auth:") == 0) {
                launchArgs.push_back(arg);
                isProtocol = true;
            } else if (arg.find("roblox-studio:") == 0) {
                launchArgs.push_back("-protocolString");
                launchArgs.push_back(arg);
                isProtocol = true;
            }
        }

        setState(LauncherState::BOOTSTRAPPING);
        setStatus(0.1f, "Resolving Roblox Version...");
        
        auto& rbx = downloader::RobloxManager::instance();
        std::string guid;

        if (isProtocol) {
            auto versions = rbx.getInstalledVersions();
            if (!versions.empty()) {
                std::sort(versions.begin(), versions.end(), [&](const std::string& a, const std::string& b) {
                    auto pathA = PathManager::instance().versions() / a / "AppSettings.xml";
                    auto pathB = PathManager::instance().versions() / b / "AppSettings.xml";
                    try {
                        return std::filesystem::last_write_time(pathA) > std::filesystem::last_write_time(pathB);
                    } catch(...) { return a > b; }
                });
                guid = versions[0];
                LOG_INFO("Fast path: Using latest local version %s", guid.c_str());
            }
        }

        auto& cfg = Config::instance().getGeneral();

        if (guid.empty() && !stop_) {
            try {
                guid = rbx.getLatestVersionGUID(cfg.channel);
            } catch (...) {}

            if (guid.empty()) {
                auto versions = rbx.getInstalledVersions();
                if (!versions.empty()) {
                    std::sort(versions.begin(), versions.end(), [&](const std::string& a, const std::string& b) {
                        auto pathA = PathManager::instance().versions() / a / "AppSettings.xml";
                        auto pathB = PathManager::instance().versions() / b / "AppSettings.xml";
                        try {
                            return std::filesystem::last_write_time(pathA) > std::filesystem::last_write_time(pathB);
                        } catch(...) { return a > b; }
                    });
                    guid = versions[0];
                    LOG_INFO("Offline: Using latest local version %s", guid.c_str());
                } else {
                    setError("Failed to resolve Roblox version (Offline and no local versions)");
                    return;
                }
            }
        }

        if (stop_) return;

        setState(LauncherState::DOWNLOADING_ROBLOX);
        if (!rbx.isInstalled(guid)) {
            bool ok = rbx.installVersion(guid, [&](float p, std::string s) {
                setStatus(0.2f + (p * 0.3f), "Roblox: " + s);
            });
            if (!ok) {
                setError("Failed to install Roblox Studio");
                return;
            }
        }
        
        if (stop_) return;

        setState(LauncherState::PREPARING_WINE);
        setStatus(0.6f, "Checking Environment...");
        
        auto& wine = downloader::WineManager::instance();
        auto& dxvk = downloader::DxvkManager::instance();

        auto checkSourceChange = [](const std::string& installedRoot, const std::string& repo, const std::string& tag, const std::string& metaFile) {
            if (installedRoot.empty() || !std::filesystem::exists(installedRoot)) return true;
            try {
                std::ifstream f(std::filesystem::path(installedRoot) / metaFile);
                if (!f.is_open()) return true;
                auto j = nlohmann::json::parse(f);
                if (j.value("repo", "") != repo) return true;
                if (tag != "latest" && j.value("tag", "") != tag) return true;
            } catch (...) { return true; }
            return false;
        };

        if (cfg.runnerType == "Proton") {
            if (checkSourceChange(cfg.protonSource.installedRoot, cfg.protonSource.repo, cfg.protonSource.version, "rsjfw_meta.json")) {
                cfg.protonSource.installedRoot = "";
            }

            bool needInstall = cfg.protonSource.installedRoot.empty() || 
                               !std::filesystem::exists(cfg.protonSource.installedRoot);
            
            if (needInstall && !cfg.protonSource.useCustomRoot) {
                setStatus(0.65f, "Downloading Proton...");
                bool ok = wine.installVersion(
                    cfg.protonSource.repo, 
                    cfg.protonSource.version, 
                    cfg.protonSource.asset, 
                    [&](float p, std::string s){
                        setStatus(0.6f + (p * 0.1f), "Proton: " + s);
                    }
                );
                
                if (ok) {
                    auto installs = wine.getInstalledVersions();
                    if (!installs.empty()) {
                        cfg.protonSource.installedRoot = installs.back().path;
                        Config::instance().save();
                    }
                } else {
                    setError("Failed to install Proton");
                    return;
                }
            }
        } else {
            if (checkSourceChange(cfg.wineSource.installedRoot, cfg.wineSource.repo, cfg.wineSource.version, "rsjfw_meta.json")) {
                cfg.wineSource.installedRoot = "";
            }

            bool needInstall = cfg.wineSource.installedRoot.empty() || 
                               !std::filesystem::exists(cfg.wineSource.installedRoot);
            
            if (needInstall && !cfg.wineSource.useCustomRoot) {
                setStatus(0.65f, "Downloading Wine...");
                bool ok = wine.installVersion(
                    cfg.wineSource.repo, 
                    cfg.wineSource.version, 
                    cfg.wineSource.asset, 
                    [&](float p, std::string s){
                        setStatus(0.6f + (p * 0.1f), "Wine: " + s);
                    }
                );
                
                if (ok) {
                    auto installs = wine.getInstalledVersions();
                    if (!installs.empty()) {
                        cfg.wineSource.installedRoot = installs.back().path;
                        Config::instance().save();
                    }
                } else {
                    setError("Failed to install Wine");
                    return;
                }
            }
        }

        if (stop_) return;

        setState(LauncherState::INSTALLING_DXVK);
        if (cfg.dxvk) {
             if (checkSourceChange(cfg.dxvkSource.installedRoot, cfg.dxvkSource.repo, cfg.dxvkSource.version, "rsjfw_dxvk.json")) {
                 cfg.dxvkSource.installedRoot = "";
             }

             bool needInstall = cfg.dxvkSource.installedRoot.empty() || 
                               !std::filesystem::exists(cfg.dxvkSource.installedRoot);
             
             if (needInstall) {
                 setStatus(0.75f, "Downloading DXVK...");
                 bool ok = dxvk.installVersion(
                     cfg.dxvkSource.repo,
                     cfg.dxvkSource.version,
                     [&](float p, std::string s) {
                         setStatus(0.7f + (p * 0.1f), "DXVK: " + s);
                     }
                 );
                 if (ok) {
                     auto installs = dxvk.getInstalledVersions();
                     if (!installs.empty()) {
                         cfg.dxvkSource.installedRoot = installs.back().path;
                         Config::instance().save();
                     }
                 }
             }
        }

        std::string runnerRoot;
        if (cfg.runnerType == "Proton") {
            runnerRoot = cfg.protonSource.installedRoot;
            if (cfg.protonSource.useCustomRoot) runnerRoot = cfg.protonSource.customRootPath;
        } else {
            runnerRoot = cfg.wineSource.installedRoot;
            if (cfg.wineSource.useCustomRoot) runnerRoot = cfg.wineSource.customRootPath;
        }

        if (runnerRoot.empty()) {
             auto installs = wine.getInstalledVersions();
             if (!installs.empty()) runnerRoot = installs[0].path;
        }
        
        if (runnerRoot.empty()) {
            setError(cfg.runnerType == "Proton" ? "No valid Proton installation found" : "No valid Wine installation found");
            return;
        }

        if (stop_) return;

        std::unique_ptr<Runner> runner;
        if (cfg.runnerType == "Proton") {
            runner = Runner::createProtonRunner(runnerRoot);
        } else {
            runner = Runner::createWineRunner(runnerRoot);
        }
        
        if (!cfg.launchWrapper.empty()) {
            std::vector<std::string> wrapArgs;
            std::stringstream ss(cfg.launchWrapper);
            std::string part;
            while (ss >> part) wrapArgs.push_back(part);
            if (runner->getPrefix()) {
                runner->getPrefix()->setWrapper(wrapArgs);
            }
        }

        setStatus(0.85f, "Configuring Runner...");
        if (!runner->configure([&](float p, std::string s){
            setStatus(0.8f + (p * 0.1f), s);
        })) {
            setError("Runner configuration failed");
            return;
        }

        if (stop_) return;

        setState(LauncherState::APPLYING_CONFIG);
        setStatus(0.95f, "Applying Configuration...");
        
        nlohmann::json clientSettings = Config::instance().getClientAppSettings();
        std::filesystem::path versionDir = std::filesystem::path(PathManager::instance().versions()) / guid;
        std::filesystem::path settingsDir = versionDir / "ClientSettings";
        std::filesystem::create_directories(settingsDir);
        std::ofstream(settingsDir / "ClientAppSettings.json") << clientSettings.dump(4);

        if (cfg.dxvk) {
            auto installs = dxvk.getInstalledVersions();
            if (!installs.empty()) {
                std::filesystem::path dxvkRoot = installs[0].path;
                std::filesystem::path src64 = dxvkRoot / "x64";
                if (!std::filesystem::exists(src64)) src64 = dxvkRoot / "x86_64-windows";
                if (std::filesystem::exists(src64)) {
                    for (const auto& entry : std::filesystem::directory_iterator(src64)) {
                        if (entry.path().extension() == ".dll") {
                            std::filesystem::copy_file(entry.path(), versionDir / entry.path().filename(), std::filesystem::copy_options::overwrite_existing);
                        }
                    }
                }
            }
        }

        if (installOnly) {
            setState(LauncherState::FINISHED);
            setStatus(1.0f, "Installation Complete");
            return;
        }

        setState(LauncherState::LAUNCHING_STUDIO);
        setStatus(1.0f, "Launching Studio...");
        
        setState(LauncherState::RUNNING);
        
        stream_buffer_t outBuf;
        outBuf.connect([](std::string_view s){ 
            std::cout << "[Studio] " << s << std::flush; 
        });

        auto result = runner->runStudio(guid, launchArgs, outBuf);
        if (result.exitCode != 0) {
             setError("Roblox Studio exited with error: " + std::to_string(result.exitCode));
        } else {
             setState(LauncherState::FINISHED);
             setStatus(1.0f, "Session Ended");
        }

    } catch (const std::exception& e) {
        setError(e.what());
    }
}

}
