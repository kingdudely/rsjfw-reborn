#include "orchestrator.h"
#include "config.h"
#include "gui.h"
#include "downloader/roblox_manager.h"
#include "downloader/wine_manager.h"
#include "downloader/dxvk_manager.h"
#include "path_manager.h"
#include "runner.h"
#include "logger.h"
#include "diagnostics.h"
#include "credential_manager.h"
#include <iostream>
#include <chrono>
#include <fstream>
#include <nlohmann/json.hpp>
#include <sstream>
#include <filesystem>
#include <unistd.h>
#include <algorithm>
#include <iomanip>

namespace rsjfw {

namespace fs = std::filesystem;

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
        case LauncherState::IDLE: return "idle";
        case LauncherState::BOOTSTRAPPING: return "bootstrapping environment";
        case LauncherState::DOWNLOADING_ROBLOX: return "fetching roblox studio";
        case LauncherState::PREPARING_WINE: return "preparing compatibility layer";
        case LauncherState::INSTALLING_DXVK: return "optimizing graphics";
        case LauncherState::APPLYING_CONFIG: return "applying settings";
        case LauncherState::LAUNCHING_STUDIO: return "launching studio";
        case LauncherState::RUNNING: return "running";
        case LauncherState::FINISHED: return "finished";
        case LauncherState::ERROR: return "error";
        default: return "unknown";
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
    rsjfw::cmd::Command::killAll();
}

void Orchestrator::setStatus(float p, const std::string& s) {
    std::lock_guard<std::mutex> l(mutex_);
    progress_ = p;
    status_ = s;
    GUI::instance().setProgress(p, s);
}

void Orchestrator::setState(LauncherState s) {
    auto now = std::chrono::steady_clock::now();
    static auto lastTransition = now;
    auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastTransition).count();
    LOG_INFO("State Transition: %s (%lld ms since last)", stateToString(s).c_str(), diff);
    lastTransition = now;
    state_ = s;
}

void Orchestrator::setError(const std::string& e) {
    std::lock_guard<std::mutex> l(mutex_);
    error_ = e;
    state_ = LauncherState::ERROR;
    setStatus(1.0f, "error: " + e);
}

void Orchestrator::worker(std::string arg) {
    auto startTime = std::chrono::steady_clock::now();
    try {
        bool installOnly = (arg == "INSTALL_ONLY");
        std::vector<std::string> launchArgs;
        bool isProtocol = false;
        bool isFile = false;
        std::string targetPath = arg;

        if (!installOnly && !arg.empty()) {
            if (arg.find("roblox-studio-auth:") == 0) {
                launchArgs.push_back(arg);
                isProtocol = true;
            } else if (arg.find("roblox-studio:") == 0) {
                launchArgs.push_back("-protocolString");
                launchArgs.push_back(arg);
                isProtocol = true;
            } else {
                if (targetPath.find("file://") == 0) {
                    targetPath = targetPath.substr(7);
                    size_t pos = 0;
                    while ((pos = targetPath.find("%20", pos)) != std::string::npos) {
                        targetPath.replace(pos, 3, " ");
                        pos += 1;
                    }
                }
                
                if (fs::exists(targetPath)) {
                    isFile = true;
                    LOG_INFO("detected local file for launch: %s", targetPath.c_str());
                }
            }
        }
        
        auto& cfg = Config::instance().getGeneral();

        setState(LauncherState::BOOTSTRAPPING);
        LOG_DEBUG("Orchestrator worker started with arg: %s", arg.c_str());

        setStatus(0.05f, "running system audit...");
        auto& diag = Diagnostics::instance();
        diag.runChecks();
        if (cfg.autoApplyFixes) {
            for (const auto& r : diag.getResults()) {
                if (!r.second.ok && r.second.fixable && !r.second.ignored) {
                    LOG_INFO("resolving environment issue: %s", r.first.c_str());
                    diag.fixIssue(r.first, [&](float p, std::string s){
                        setStatus(0.05f + (p * 0.05f), "fixing " + r.first + ": " + s);
                    });
                }
            }
        }

        setStatus(0.1f, "resolving roblox version...");
        auto& rbx = downloader::RobloxManager::instance();
        std::string guid;

        auto installed = rbx.getInstalledVersions();
        if (!installed.empty()) {
            std::sort(installed.begin(), installed.end(), [&](const std::string& a, const std::string& b) {
                try {
                    return fs::last_write_time(PathManager::instance().versions() / a / "AppSettings.xml") > 
                           fs::last_write_time(PathManager::instance().versions() / b / "AppSettings.xml");
                } catch(...) { return a > b; }
            });
            guid = installed[0];
            LOG_DEBUG("using local version for speed: %s", guid.c_str());
            
            // Trigger background update check
            std::thread([&rbx, &cfg](){
                try {
                    auto latest = rbx.getLatestVersionGUID(cfg.channel);
                    LOG_DEBUG("background update check result: %s", latest.c_str());
                } catch(...) {}
            }).detach();
        }

        if (guid.empty() && !stop_) {
            guid = rbx.getLatestVersionGUID(cfg.channel);
        }

        if (guid.empty()) {
             auto versions = rbx.getInstalledVersions();
             if (!versions.empty()) {
                 std::sort(versions.begin(), versions.end());
                 guid = versions.back();
             }
        }

        if (guid.empty()) {
            setError("failed to resolve roblox version");
            return;
        }

        LOG_DEBUG("resolved studio version: %s", guid.c_str());

        if (stop_) return;

        setState(LauncherState::DOWNLOADING_ROBLOX);
        if (!rbx.isInstalled(guid)) {
            LOG_INFO("installing roblox studio version %s", guid.c_str());
            bool ok = rbx.installVersion(guid, [&](float p, std::string s) {
                setStatus(p, "roblox: " + s);
            });
            if (!ok) {
                setError("failed to install roblox studio");
                return;
            }
        } else {
            LOG_DEBUG("roblox version %s already installed", guid.c_str());
            setStatus(1.0f, "roblox: already installed");
        }
        
        if (stop_) return;

        auto& wine = downloader::WineManager::instance();
        auto& dxvk = downloader::DxvkManager::instance();

        setState(LauncherState::INSTALLING_DXVK);
        if (cfg.dxvk) {
             bool needInstall = cfg.dxvkSource.installedRoot.empty() || !fs::exists(cfg.dxvkSource.installedRoot);
             if (needInstall) {
                 LOG_INFO("provisioning dxvk...");
                 bool ok = dxvk.installVersion(cfg.dxvkSource.repo, cfg.dxvkSource.version, [&](float p, std::string s) {
                     setStatus(p, "dxvk: " + s);
                 });
                 if (ok) {
                     auto installs = dxvk.getInstalledVersions();
                     if (!installs.empty()) {
                         cfg.dxvkSource.installedRoot = installs.back().path;
                         Config::instance().save();
                     }
                 }
             } else {
                 LOG_DEBUG("dxvk already installed at %s", cfg.dxvkSource.installedRoot.c_str());
                 setStatus(1.0f, "dxvk: already installed");
             }
        }

        if (stop_) return;

        setState(LauncherState::PREPARING_WINE);
        std::string runnerRoot;
        if (cfg.runnerType == "Proton") {
            bool needInstall = cfg.protonSource.installedRoot.empty() || !fs::exists(cfg.protonSource.installedRoot);
            if (needInstall && !cfg.protonSource.useCustomRoot) {
                LOG_INFO("provisioning proton...");
                bool ok = wine.installVersion(cfg.protonSource.repo, cfg.protonSource.version, cfg.protonSource.asset, [&](float p, std::string s){
                    setStatus(p, "proton: " + s);
                });
                if (ok) {
                    auto installs = wine.getInstalledVersions();
                    if (!installs.empty()) {
                        cfg.protonSource.installedRoot = installs.back().path;
                        Config::instance().save();
                    }
                } else {
                    setError("failed to install proton");
                    return;
                }
            }
            runnerRoot = cfg.protonSource.useCustomRoot ? cfg.protonSource.customRootPath : cfg.protonSource.installedRoot;
        } else {
            bool needInstall = cfg.wineSource.installedRoot.empty() || !fs::exists(cfg.wineSource.installedRoot);
            if (needInstall && !cfg.wineSource.useCustomRoot) {
                LOG_INFO("provisioning wine...");
                bool ok = wine.installVersion(cfg.wineSource.repo, cfg.wineSource.version, cfg.wineSource.asset, [&](float p, std::string s){
                    setStatus(p, "wine: " + s);
                });
                if (ok) {
                    auto installs = wine.getInstalledVersions();
                    if (!installs.empty()) {
                        cfg.wineSource.installedRoot = installs.back().path;
                        Config::instance().save();
                    }
                } else {
                    setError("failed to install wine");
                    return;
                }
            }
            runnerRoot = cfg.wineSource.useCustomRoot ? cfg.wineSource.customRootPath : cfg.wineSource.installedRoot;
        }

        if (runnerRoot.empty()) {
             auto installs = wine.getInstalledVersions();
             if (!installs.empty()) runnerRoot = installs[0].path;
        }
        
        if (runnerRoot.empty()) {
            setError("no valid runner installation found");
            return;
        }

        LOG_DEBUG("selected runner root: %s", runnerRoot.c_str());

        if (stop_) return;

        std::unique_ptr<Runner> runner;
        if (cfg.runnerType == "Proton") {
            runner = Runner::createProtonRunner(runnerRoot);
        } else {
            runner = Runner::createWineRunner(runnerRoot);
        }
        
        if (isFile) {
            std::string winPath = runner->resolveWindowsPath(targetPath);
            if (!winPath.empty()) {
                LOG_INFO("Opening local file: %s -> %s", targetPath.c_str(), winPath.c_str());
                launchArgs.clear();
                launchArgs.push_back(winPath);
            } else {
                LOG_WARN("failed to resolve windows path for: %s", targetPath.c_str());
            }
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

        setStatus(0.85f, "preparing environment...");
        LOG_DEBUG("running configuration step for %s", cfg.runnerType.c_str());
        if (!runner->configure([&](float p, std::string s){
            setStatus(p, s);
        })) {
            setError("runner configuration failed");
            return;
        }

        if (stop_) return;

        setState(LauncherState::APPLYING_CONFIG);
        setStatus(0.95f, "applying studio configuration...");
        LOG_DEBUG("syncing fflags and graphics overrides...");

        if (runner->getPrefix()) {
            LOG_DEBUG("forcing studio theme preference: %s", cfg.darkMode ? "dark" : "light");
            runner->getPrefix()->registryAdd("HKCU\\Software\\Roblox\\RobloxStudio", "Theme", cfg.darkMode ? "Dark" : "Light", "REG_SZ");
            runner->getPrefix()->registryCommit();
        }
        
        nlohmann::json clientSettings = Config::instance().getClientAppSettings();
        fs::path versionDir = fs::path(PathManager::instance().versions()) / guid;
        fs::path settingsDir = versionDir / "ClientSettings";
        fs::create_directories(settingsDir);
        std::ofstream(settingsDir / "ClientAppSettings.json") << clientSettings.dump(4);

        if (cfg.dxvk) {
            auto installs = dxvk.getInstalledVersions();
            if (!installs.empty()) {
                fs::path dxvkRoot = installs[0].path;
                LOG_DEBUG("deploying dxvk dlls from %s", dxvkRoot.c_str());
                fs::path src64 = dxvkRoot / "x64";
                if (!fs::exists(src64)) src64 = dxvkRoot / "x86_64-windows";
                if (fs::exists(src64)) {
                    for (const auto& entry : fs::directory_iterator(src64)) {
                        if (entry.path().extension() == ".dll") {
                            fs::copy_file(entry.path(), versionDir / entry.path().filename(), fs::copy_options::overwrite_existing);
                        }
                    }
                }
            }
        }

        if (installOnly) {
            setState(LauncherState::FINISHED);
            setStatus(1.0f, "installation complete");
            LOG_INFO("Headless installation finished successfully.");
            return;
        }

        setState(LauncherState::LAUNCHING_STUDIO);
        setStatus(1.0f, "launching studio...");
        LOG_INFO("Ready. Executing Studio binary.");
        
        try {
            auto& creds = CredentialManager::instance();
            auto info = creds.getSecurity(runner->getPrefix());
            if (info) {
                LOG_INFO("Active Roblox session detected for user: %s", info->userId.c_str());
            }
        } catch (...) {
            LOG_WARN("failed to audit credentials");
        }

        auto endPrep = std::chrono::steady_clock::now();
        auto prepTime = std::chrono::duration_cast<std::chrono::seconds>(endPrep - startTime).count();
        LOG_INFO("Total preparation time: %llds", prepTime);

        setState(LauncherState::RUNNING);
        
        stream_buffer_t outBuf;
        outBuf.connect([](std::string_view s){ 
            LOG_INFO("[Studio] %s", std::string(s).data()); 
        });

        auto result = runner->runStudio(guid, launchArgs, outBuf);
        if (result.exitCode != 0) {
             setError("studio exited with code: " + std::to_string(result.exitCode));
        } else {
             setState(LauncherState::FINISHED);
             setStatus(1.0f, "session ended");
        }

    } catch (const std::exception& e) {
        LOG_ERROR("Orchestrator exception: %s", e.what());
        setError(e.what());
    }
}

}
