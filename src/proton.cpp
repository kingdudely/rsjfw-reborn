#include "proton.h"
#include "common.h"
#include "config.h"
#include "http.h"
#include "logger.h"
#include "path_manager.h"
#include "zip_util.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sys/stat.h>
#include <unistd.h>
#include <future>

namespace rsjfw {

namespace fs = std::filesystem;

ProtonRunner::ProtonRunner(std::string protonRoot)
    : Runner(nullptr, ""), protonRoot_(protonRoot) {
    LOG_DEBUG("ProtonRunner created with root: %s", protonRoot.c_str());

  fs::path root(protonRoot);
  protonScript_ = (root / "proton").string();

  auto &pm = PathManager::instance();
  compatDataPath_ = (pm.root() / "proton_data").string();
  fs::path pfxPath = fs::path(compatDataPath_) / "pfx";

  prefix_ = std::make_shared<Prefix>(pfxPath.string(), protonRoot);
  prefix_->setExecutor(protonScript_, {"run"});

  std::map<std::string, std::string> env;
  env["STEAM_COMPAT_DATA_PATH"] = compatDataPath_;
  env["STEAM_COMPAT_CLIENT_INSTALL_PATH"] = (fs::path(getenv("HOME")) / ".local/share/Steam").string();
  prefix_->setEnvironment(env);
  LOG_DEBUG("Proton compat data path: %s", compatDataPath_.c_str());
}

bool ProtonRunner::configure(ProgressCb cb) {
  auto &pm = PathManager::instance();
  auto &cfg = Config::instance().getGeneral();
  LOG_INFO("Configuring Proton environment...");

  fs::path appDir = fs::path(compatDataPath_) / "pfx" / "drive_c" /
                    "Program Files (x86)" / "Microsoft" / "EdgeWebView" /
                    "Application";

  if (!cfg.enableWebView2) {
    if (fs::exists(appDir)) {
      if (cb) cb(0.1f, "cleaning up webview2...");
      LOG_DEBUG("WebView2 disabled, removing existing runtime...");
      fs::remove_all(fs::path(compatDataPath_) / "pfx" / "drive_c" /
                     "Program Files (x86)" / "Microsoft" / "EdgeWebView");
    }
  }

  fs::path marker = fs::path(compatDataPath_) / ".rsjfw_proton_provisioned";
  if (!fs::exists(compatDataPath_)) fs::create_directories(compatDataPath_);

  if (fs::exists(marker)) {
    LOG_DEBUG("Proton prefix already provisioned (marker found)");
    if (cb) cb(1.0f, "environment ready.");
    return true;
  }

  if (!prefix_->init(makeSubProgress(0.0f, 0.1f, "verifying prefix...", cb)))
    return false;

  LOG_INFO("Provisioning system dependencies...");
  std::vector<std::pair<std::string, std::string>> fonts = {
      {"https://sourceforge.net/projects/corefonts/files/the%20fonts/final/arial32.exe/download", "arial32.exe"},
      {"https://sourceforge.net/projects/corefonts/files/the%20fonts/final/times32.exe/download", "times32.exe"},
      {"https://sourceforge.net/projects/corefonts/files/the%20fonts/final/verdan32.exe/download", "verdan32.exe"}};

  fs::path fontsDir = fs::path(compatDataPath_) / "pfx" / "drive_c" / "windows" / "Fonts";
  fs::create_directories(fontsDir);

  LOG_INFO("Downloading fonts in parallel...");
  std::vector<std::future<bool>> dlFutures;
  std::vector<double> currentSpeeds(fonts.size(), 0.0);
  std::vector<bool> done(fonts.size(), false);
  std::mutex speedMtx;

  for (size_t i = 0; i < fonts.size(); i++) {
    const auto& font = fonts[i];
    dlFutures.push_back(std::async(std::launch::async, [&pm, font, i, &currentSpeeds, &done, &speedMtx]() {
      fs::path fdest = pm.cache() / font.second;
      if (fs::exists(fdest)) {
          { std::lock_guard l(speedMtx); done[i] = true; }
          return true;
      }

      auto subCb = [&](float, std::string speedStr) {
          double speed = 0;
          std::stringstream ss(speedStr);
          double val; std::string unit;
          if (ss >> val >> unit) {
              if (unit == "KB/s") speed = val * 1024;
              else if (unit == "MB/s") speed = val * 1024 * 1024;
              else speed = val;
          }
          { std::lock_guard l(speedMtx); currentSpeeds[i] = speed; }
      };

      bool ok = HTTP::download(font.first, fdest.string(), subCb);
      { std::lock_guard l(speedMtx); done[i] = ok; currentSpeeds[i] = 0; }
      return ok;
    }));
  }

  float base = 0.2f;
  float slice = 0.4f;

  while (true) {
    bool allDone = true;
    double totalSpeed = 0;
    std::string downloadingList = "";
    
    {
        std::lock_guard l(speedMtx);
        for (size_t i = 0; i < fonts.size(); i++) {
            if (!done[i]) {
                allDone = false;
                if (currentSpeeds[i] > 0) {
                    if (!downloadingList.empty()) downloadingList += ", ";
                    downloadingList += fonts[i].second;
                }
            }
            totalSpeed += currentSpeeds[i];
        }
    }

    if (allDone) break;

    if (cb) {
        std::string speedStr;
        if (totalSpeed < 1024) speedStr = std::to_string((int)totalSpeed) + " B/s";
        else if (totalSpeed < 1024*1024) speedStr = std::to_string((int)(totalSpeed/1024)) + " KB/s";
        else speedStr = std::to_string((int)(totalSpeed/(1024*1024))) + " MB/s";

        cb(base + (slice * 0.5f), "fonts: " + speedStr + " (dl: " + downloadingList + ")");
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }

  for (size_t i = 0; i < fonts.size(); i++) {
    float start = base + (float)i * (slice / (float)fonts.size());
    if (dlFutures[i].get()) {
        LOG_DEBUG("Extracting font: %s", fonts[i].second.c_str());
        if (cb) cb(start + 0.1f, "extracting " + fonts[i].second + "...");
        
        fs::path fdest = pm.cache() / fonts[i].second;
        fs::path fexpand = pm.cache() / (fonts[i].second + "_ext_p");
        fs::remove_all(fexpand);
        
        float baseProg = start + 0.1f;
        float fontSlice = slice / (float)fonts.size() * 0.5f; // Allocation for extract
        ZipUtil::extract(fdest.string(), fexpand.string(), [&](float p, std::string s){
            if (cb) cb(baseProg + (p * fontSlice), s);
        });

        if (fs::exists(fexpand)) {
          for (const auto &entry : fs::directory_iterator(fexpand)) {
            if (entry.path().extension() == ".TTF" ||
                entry.path().extension() == ".ttf") {
              fs::copy_file(entry.path(), fontsDir / entry.path().filename(),
                            fs::copy_options::overwrite_existing);
            }
          }
        }
        fs::remove_all(fexpand);
    }
  }

  if (cfg.enableWebView2) {
    LOG_INFO("Provisioning WebView2 runtime...");
    std::string wv2Url = "https://msedge.sf.dl.delivery.mp.microsoft.com/filestreamingservice/files/ea30811e-a216-4d55-89f3-c1099862c8fc/Microsoft.WebView2.FixedVersionRuntime.143.0.3650.139.x64.cab";
    fs::path wv2Dest = pm.cache() / "webview2.cab";
    fs::path wv2Expand = pm.cache() / "webview2_expanded_proton";

    if (!fs::exists(wv2Dest)) {
      LOG_DEBUG("Downloading WebView2...");
      HTTP::download(wv2Url, wv2Dest.string(), makeSubProgress(0.7f, 0.8f, "downloading webview2...", cb));
    }

    LOG_DEBUG("Extracting WebView2...");
    fs::remove_all(wv2Expand);
    ZipUtil::extract(wv2Dest.string(), wv2Expand.string(), makeSubProgress(0.82f, 0.88f, "extracting webview2...", cb));

    fs::path versionDir = "143.0.3650.139";
    fs::create_directories(appDir);

    fs::path sourceDir;
    if (fs::exists(wv2Expand)) {
      for (const auto &entry : fs::directory_iterator(wv2Expand)) {
        if (entry.is_directory()) {
          sourceDir = entry.path();
          break;
        }
      }
    }

    if (!sourceDir.empty()) {
      fs::path targetDir = appDir / versionDir;
      LOG_DEBUG("Deploying WebView2 to %s", targetDir.c_str());
      if (cb) cb(0.9f, "deploying webview2 runtime...");
      fs::remove_all(targetDir);
      fs::rename(sourceDir, targetDir);
    }

    LOG_DEBUG("Updating WebView2 registry entries...");
    if (cb) cb(0.95f, "configuring registry...");
    prefix_->registryAdd("HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Microsoft\\EdgeUpdate\\Clients\\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}", "pv", "143.0.3650.139", "REG_SZ");
    prefix_->registryAdd("HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Microsoft\\EdgeUpdate\\Clients\\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}", "location", "C:\\Program Files (x86)\\Microsoft\\EdgeWebView\\Application", "REG_SZ");
    prefix_->registryAdd("HKEY_CURRENT_USER\\Software\\Wine\\AppDefaults\\msedgewebview2.exe", "Version", "win7", "REG_SZ");
    prefix_->registryAdd("HKEY_LOCAL_MACHINE\\Software\\Microsoft\\EdgeUpdate\\Clients\\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}", "pv", "130.0.2849.46", "REG_SZ");
    prefix_->registryAdd("HKEY_LOCAL_MACHINE\\Software\\Khronos\\OpenXR\\1", "ActiveRuntime", "C:\\Windows\\system32\\steam_openxr.json", "REG_SZ");
    prefix_->registryCommit();
  }

  std::ofstream(marker) << "done";
  if (cb) cb(1.0f, "environment ready.");
  return true;
}

bool ProtonRunner::runWine(const std::string &exe,
                           const std::vector<std::string> &args,
                           const std::string &taskName) {
  if (prefix_) {
    addBaseEnv();
    std::map<std::string, std::string> taskEnv = env_;
    taskEnv["WINEDEBUG"] = "-all";
    LOG_DEBUG("Running Proton task '%s': %s", taskName.c_str(), exe.c_str());
    return prefix_->wine(
        exe, args,
        [taskName](const std::string &s) {
          if (!s.empty())
            LOG_DEBUG("[%s] %s", taskName.c_str(), s.c_str());
        },
        "", true, taskEnv);
  }
  return false;
}

cmd::CmdResult ProtonRunner::runStudio(const std::string &versionGuid,
                                       const std::vector<std::string> &args,
                                       stream_buffer_t &outBuffer) {
  auto &pm = PathManager::instance();
  fs::path versionDir = pm.versions() / versionGuid;
  fs::path exe = versionDir / "RobloxStudioBeta.exe";

  if (!fs::exists(exe)) {
    LOG_ERROR("Studio executable not found at %s", exe.c_str());
    return {-1, 1};
  }

  LOG_INFO("Preparing Studio execution environment...");
  std::vector<std::string> runArgs;
  auto onOut = [&](const std::string &s) { outBuffer.append(s); };

  addBaseEnv();

  auto &cfg = Config::instance().getGeneral();
  if (cfg.enableWebView2) {
    LOG_DEBUG("Setting up WebView2 environment variables...");
    env_["WEBVIEW2_BROWSER_EXECUTABLE_FOLDER"] = "C:\\Program Files (x86)\\Microsoft\\EdgeWebView\\Application\\143.0.3650.139";
    env_["WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS"] = "--no-sandbox --disable-gpu --disable-dev-shm-usage --disable-features=RendererCodeIntegrity";
  }

  std::string target;
  if (cfg.desktopMode || cfg.multipleDesktops) {
    target = "explorer.exe";
    std::string res = cfg.desktopResolution;
    res.erase(std::remove(res.begin(), res.end(), ' '), res.end());
    if (res.empty()) res = "1920x1080";

    std::string desktopName = cfg.multipleDesktops ? std::to_string(getpid()) : "Desktop";
    LOG_INFO("Starting Studio in virtual desktop: RSJFW_%s (%s)", desktopName.c_str(), res.c_str());
    runArgs.push_back("/desktop=RSJFW_" + desktopName + "," + res);
    runArgs.push_back(exe.string());
  } else {
    target = exe.string();
  }

  for (const auto &a : args) runArgs.push_back(a);

  LOG_INFO("Launching Studio process...");
  bool ok = prefix_->wine(target, runArgs, onOut, versionDir.string(), true, env_);
  return ok ? cmd::CmdResult{0, 0} : cmd::CmdResult{-1, 1};
}

std::string ProtonRunner::resolveWindowsPath(const std::string &unixPath) {
    fs::path p(unixPath);
    if (!fs::exists(p)) return "";
    
    std::string absPath = fs::absolute(p).string();
    std::string pfx = prefix_->getPath();
    fs::path driveC = fs::path(pfx) / "drive_c";
    std::string driveCStr = fs::absolute(driveC).string();

    std::string result = "";
    if (absPath.find(driveCStr) == 0) {
        result = "C:" + absPath.substr(driveCStr.length());
    } else {
        result = "Z:" + absPath;
    }

    std::replace(result.begin(), result.end(), '/', '\\');
    return result;
}

std::map<std::string, std::string> ProtonRunner::getBaseEnv() {
    auto res = Runner::getBaseEnv();
    auto &cfg = Config::instance().getGeneral();
    auto &pm = PathManager::instance();

    res["STEAM_COMPAT_DATA_PATH"] = (pm.root() / "proton_data").string();
    res["STEAM_COMPAT_CLIENT_INSTALL_PATH"] = (fs::path(getenv("HOME")) / ".local/share/Steam").string();
    res["WINEPREFIX"] = (fs::path(res["STEAM_COMPAT_DATA_PATH"]) / "pfx").string();
    res["WINEARCH"] = "win64";

    if (cfg.enableWebView2) {
        res["WEBVIEW2_BROWSER_EXECUTABLE_FOLDER"] = "C:\\Program Files (x86)\\Microsoft\\EdgeWebView\\Application\\143.0.3650.139";
        res["WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS"] = "--no-sandbox --disable-gpu --disable-dev-shm-usage --disable-features=RendererCodeIntegrity";
    }

    std::string ovr = "winebrowser.exe=b;d3dcompiler_47=n,b;atmlib=b";
    if (cfg.dxvk) {
        ovr += ";d3d11,dxgi,d3d9,d3d10core=n,b";
    }

    if (res.count("WINEDLLOVERRIDES")) res["WINEDLLOVERRIDES"] = res["WINEDLLOVERRIDES"] + ";" + ovr;
    else res["WINEDLLOVERRIDES"] = ovr;

    return res;
}

}
