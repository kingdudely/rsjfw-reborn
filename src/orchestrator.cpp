#include "orchestrator.h"
#include "config.h"
#include "credential_manager.h"
#include "diagnostics.h"
#include "downloader/dxvk_manager.h"
#include "downloader/roblox_manager.h"
#include "downloader/wine_manager.h"
#include "gui.h"
#include "logger.h"
#include "path_manager.h"
#include "runner_manager.h"
#include <algorithm>
#include <chrono>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <nlohmann/json.hpp>
#include <sstream>
#include <unistd.h>

namespace rsjfw {

namespace fs = std::filesystem;

Orchestrator &Orchestrator::instance() {
  static Orchestrator inst;
  return inst;
}

Orchestrator::~Orchestrator() {
  cancel();
  if (workerThread_.joinable())
    try {
      workerThread_.join();
    } catch (const std::exception &e) {
    }
}

static std::string stateToString(LauncherState s) {
  switch (s) {
  case LauncherState::IDLE:
    return "idle";
  case LauncherState::BOOTSTRAPPING:
    return "bootstrapping environment";
  case LauncherState::DOWNLOADING_ROBLOX:
    return "fetching roblox studio";
  case LauncherState::PREPARING_WINE: {
    auto rt = Config::instance().getGeneral().runnerType;
    if (rt == "UMU")
      return "preparing umu";
    return "preparing " + rt;
  }
  case LauncherState::INSTALLING_DXVK:
    return "optimizing graphics";
  case LauncherState::APPLYING_CONFIG:
    return "applying settings";
  case LauncherState::LAUNCHING_STUDIO:
    return "launching studio";
  case LauncherState::RUNNING:
    return "running";
  case LauncherState::FINISHED:
    return "finished";
  case LauncherState::ERROR:
    return "error";
  default:
    return "unknown";
  }
}

void Orchestrator::startLaunch(const std::string &arg) {
  if (state_ != LauncherState::IDLE && state_ != LauncherState::FINISHED &&
      state_ != LauncherState::ERROR)
    return;
  stop_ = false;
  setState(LauncherState::BOOTSTRAPPING);
  if (workerThread_.joinable())
    workerThread_.join();
  workerThread_ = std::thread(&Orchestrator::worker, this, arg);
}

void Orchestrator::cancel() {
  stop_ = true;
  rsjfw::cmd::Command::killAll();
}

void Orchestrator::shutdown() {
  cancel();
  if (workerThread_.joinable()) {
    try {
      workerThread_.join();
    } catch (...) {
    }
  }
}

void Orchestrator::setStatus(float p, const std::string &s) {
  std::lock_guard<std::mutex> l(mutex_);
  progress_ = p;
  status_ = s;
  GUI::instance().setProgress(p, s);
}

void Orchestrator::setState(LauncherState s) {
  auto now = std::chrono::steady_clock::now();
  static auto lastTransition = now;
  auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(
                  now - lastTransition)
                  .count();
  LOG_INFO("State Transition: %s (%lld ms since last)",
           stateToString(s).c_str(), diff);
  lastTransition = now;
  state_ = s;
}

void Orchestrator::setError(const std::string &e) {
  {
    std::lock_guard<std::mutex> l(mutex_);
    error_ = e;
    state_ = LauncherState::ERROR;
  }
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

    auto &cfg = Config::instance().getGeneral();

    setState(LauncherState::BOOTSTRAPPING);
    LOG_DEBUG("Orchestrator worker started with arg: %s", arg.c_str());

    bool fastPath = isProtocol && (arg.find("roblox-studio-auth:") == 0);

    if (!fastPath) {
        setStatus(0.05f, "running system audit...");
        auto &diag = Diagnostics::instance();
        diag.runChecks();
        if (cfg.autoApplyFixes) {
          for (const auto &r : diag.getResults()) {
            if (!r.second.ok && r.second.fixable && !r.second.ignored) {
              LOG_INFO("resolving environment issue: %s", r.first.c_str());
              diag.fixIssue(r.first, [&](float p, std::string s) {
                setStatus(0.05f + (p * 0.05f), "fixing " + r.first + ": " + s);
              });
            }
          }
        }
    } else {
        LOG_INFO("Fast-path active: skipping diagnostics");
    }

    setStatus(0.1f, "resolving roblox version...");
    auto &rbx = downloader::RobloxManager::instance();
    std::string guid;

    auto installed = rbx.getInstalledVersions();
    if (!installed.empty()) {
      std::sort(
          installed.begin(), installed.end(),
          [&](const std::string &a, const std::string &b) {
            try {
              return fs::last_write_time(PathManager::instance().versions() /
                                         a / "AppSettings.xml") >
                     fs::last_write_time(PathManager::instance().versions() /
                                         b / "AppSettings.xml");
            } catch (...) {
              return a > b;
            }
          });
      guid = installed[0];
      LOG_DEBUG("using local version for speed: %s", guid.c_str());

      if (!fastPath) {
          std::thread([&rbx, &cfg]() {
            try {
              auto latest = rbx.getLatestVersionGUID(cfg.channel);
              LOG_DEBUG("background update check result: %s", latest.c_str());
            } catch (...) {
            }
          }).detach();
      }
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

    if (stop_) {
      setState(LauncherState::FINISHED);
      return;
    }

    setState(LauncherState::DOWNLOADING_ROBLOX);
    if (!rbx.isInstalled(guid)) {
      LOG_INFO("installing roblox studio version %s", guid.c_str());
      bool ok = rbx.installVersion(
          guid, [&](float p, std::string s) { setStatus(p, "roblox: " + s); });
      if (!ok) {
        setError("failed to install roblox studio");
        return;
      }
    } else {
      LOG_DEBUG("roblox version %s already installed", guid.c_str());
      setStatus(1.0f, "roblox: already installed");
    }

    if (stop_) {
      setState(LauncherState::FINISHED);
      return;
    }

    auto &wine = downloader::WineManager::instance();
    auto &dxvk = downloader::DxvkManager::instance();

    setState(LauncherState::INSTALLING_DXVK);
    if (cfg.dxvk) {
      bool needInstall = cfg.dxvkSource.installedRoot.empty() ||
                         !fs::exists(cfg.dxvkSource.installedRoot);
      if (needInstall) {
        LOG_INFO("provisioning dxvk...");
        bool ok = dxvk.installVersion(
            cfg.dxvkSource.repo, cfg.dxvkSource.version,
            [&](float p, std::string s) { setStatus(p, "dxvk: " + s); });
        if (ok) {
          auto installs = dxvk.getInstalledVersions();
          if (!installs.empty()) {
            cfg.dxvkSource.installedRoot = installs.back().path;
            Config::instance().save();
          }
        }
      } else {
        LOG_DEBUG("dxvk already installed at %s",
                  cfg.dxvkSource.installedRoot.c_str());
        setStatus(1.0f, "dxvk: already installed");
      }
    }

    if (stop_) {
      setState(LauncherState::FINISHED);
      return;
    }

    setState(LauncherState::PREPARING_WINE);

    if (cfg.runnerType == "Wine") {
      bool needInstall = cfg.wineSource.installedRoot.empty() ||
                         !fs::exists(cfg.wineSource.installedRoot);
      if (needInstall && !cfg.wineSource.useCustomRoot) {
        LOG_INFO("provisioning wine...");
        bool ok = wine.installVersion(
            cfg.wineSource.repo, cfg.wineSource.version, cfg.wineSource.asset,
            [&](float p, std::string s) { setStatus(p, "wine: " + s); });
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
    } else if (cfg.runnerType == "Proton" || cfg.runnerType == "UMU") {
      bool isUmuManaged =
          (cfg.runnerType == "UMU" && !cfg.protonSource.useCustomRoot &&
           cfg.protonSource.customRootPath == "GE-Proton");
      if (!isUmuManaged) {
        bool needInstall = cfg.protonSource.installedRoot.empty() ||
                           !fs::exists(cfg.protonSource.installedRoot);
        if (needInstall && !cfg.protonSource.useCustomRoot) {
          LOG_INFO("provisioning proton...");
          bool ok = wine.installVersion(
              cfg.protonSource.repo, cfg.protonSource.version,
              cfg.protonSource.asset,
              [&](float p, std::string s) { setStatus(p, "proton: " + s); });
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
      }
    }

    RunnerManager::instance().refresh();
    auto runner = RunnerManager::instance().get();
    if (!runner) {
      setError("failed to initialize runner");
      return;
    }

    if (isFile) {
      std::string winPath = runner->resolveWindowsPath(targetPath);
      if (!winPath.empty()) {
        LOG_INFO("Opening local file: %s -> %s", targetPath.c_str(),
                 winPath.c_str());
        launchArgs.clear();
        launchArgs.push_back(winPath);
      } else {
        LOG_WARN("failed to resolve windows path for: %s", targetPath.c_str());
      }
    }

    setStatus(0.85f, "preparing environment...");
    if (!runner->configure([&](float p, std::string s) { setStatus(p, s); })) {
      setError("runner configuration failed");
      return;
    }

    if (stop_) {
      setState(LauncherState::FINISHED);
      return;
    }

    if (runner->getPrefix()) {
      setStatus(0.85f, "syncing credentials...");
      CredentialManager::instance().syncAllRunners(runner->getPrefix());
    }

    setState(LauncherState::APPLYING_CONFIG);
    setStatus(0.95f, "applying studio configuration...");

    if (runner->getPrefix()) {
      setStatus(0.95f, "applying studio configuration...");
      LOG_DEBUG("forcing studio theme preference: %s", cfg.studioTheme.c_str());
      runner->getPrefix()->registryAdd(
          "HKCU\\Software\\Roblox\\RobloxStudio\\Themes", "CurrentTheme",
          cfg.studioTheme, "REG_SZ");
      
      setStatus(0.96f, "committing registry changes...");
      runner->getPrefix()->registryCommit();
    }

    nlohmann::json clientSettings = Config::instance().getClientAppSettings();
    fs::path versionDir = fs::path(PathManager::instance().versions()) / guid;
    fs::path settingsDir = versionDir / "ClientSettings";
    fs::create_directories(settingsDir);
    std::ofstream(settingsDir / "ClientAppSettings.json")
        << clientSettings.dump(4);

    if (cfg.dxvk) {
      auto installs = dxvk.getInstalledVersions();
      if (!installs.empty()) {
        fs::path dxvkRoot = installs[0].path;
        fs::path src64 = dxvkRoot / "x64";
        if (!fs::exists(src64))
          src64 = dxvkRoot / "x86_64-windows";
        if (fs::exists(src64)) {
          for (const auto &entry : fs::directory_iterator(src64)) {
            if (entry.path().extension() == ".dll") {
              fs::copy_file(entry.path(), versionDir / entry.path().filename(),
                            fs::copy_options::overwrite_existing);
            }
          }
        }
      }
    }

    if (installOnly) {
      setState(LauncherState::FINISHED);
      setStatus(1.0f, "installation complete");
      return;
    }

    setState(LauncherState::LAUNCHING_STUDIO);
    setStatus(1.0f, "launching studio...");

    if (stop_) {
      setState(LauncherState::FINISHED);
      return;
    }

    try {
      auto users =
          CredentialManager::instance().getLoggedInUsers(runner->getPrefix());
      for (const auto &user : users) {
        LOG_INFO("Active Roblox session detected for user: %s (ID: %s)",
                 user.username.c_str(), user.userId.c_str());
      }
    } catch (...) {
    }

    setState(LauncherState::RUNNING);

    if (stop_) {
      setState(LauncherState::FINISHED);
      return;
    }

    stream_buffer_t outBuf;
    outBuf.connect([](std::string_view s) {
      LOG_INFO("[Studio] %s", std::string(s).data());
    });

    if (stop_) {
      setState(LauncherState::FINISHED);
      return;
    }

    auto result = runner->runStudio(guid, launchArgs, outBuf);

    // Force kill studio process if it is still running and we are shutting down
    if (stop_ && result.pid > 0) {
      LOG_WARN("Shutdown requested during runStudio, killing PID %d",
               result.pid);
      rsjfw::cmd::Command::kill(result.pid, true);
    }

    if (result.exitCode != 0) {
      LOG_DEBUG("Setting error state due to exit code %d", result.exitCode);
      setError("studio exited with code: " + std::to_string(result.exitCode));
    } else {
      if (runner->getPrefix()) {
        LOG_INFO("Propagating session changes to other runners...");
        setStatus(1.0f, "backing up credentials...");
        CredentialManager::instance().syncAllRunners(runner->getPrefix());
      }
      LOG_DEBUG("Setting finished state");
      setState(LauncherState::FINISHED);
      setStatus(1.0f, "session ended");
    }
    LOG_DEBUG("Orchestrator worker thread finished");

  } catch (const std::exception &e) {
    LOG_ERROR("Orchestrator exception: %s", e.what());
    setError(e.what());
  } catch (...) {
    LOG_ERROR("Orchestrator unknown exception");
    setError("unknown error");
  }
}

} // namespace rsjfw
