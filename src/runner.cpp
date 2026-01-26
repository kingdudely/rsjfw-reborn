#include "runner.h"
#include "config.h"
#include "gpu_manager.h"
#include "logger.h"
#include "orchestrator.h"
#include "path_manager.h"
#include "proton.h"
#include "umu.h"
#include "wine.h"
#include <filesystem>
#include <fstream>
#include <shared_mutex>

#include "http.h"
#include "zip_util.h"
#include <future>
#include <sstream>

namespace rsjfw {

namespace fs = std::filesystem;

std::unique_ptr<Runner>
Runner::createWineRunner(const std::string &wineRootPath) {
  auto pm = PathManager::instance();
  auto pfx = std::make_shared<Prefix>(pm.prefix().string(), wineRootPath);
  return std::make_unique<WineRunner>(pfx, wineRootPath);
}

std::unique_ptr<Runner>
Runner::createProtonRunner(const std::string &protonRootPath) {
  return std::make_unique<ProtonRunner>(protonRootPath);
}

std::unique_ptr<Runner>
Runner::createUmuRunner(const std::string &protonRootPath) {
  auto &pm = PathManager::instance();
  std::filesystem::path umuData = pm.umu();
  std::filesystem::path pfxPath = umuData / "pfx";
  auto pfx = std::make_shared<Prefix>(pfxPath.string(), protonRootPath);
  return std::make_unique<UmuRunner>(pfx, protonRootPath);
}

void Runner::addBaseEnv() { env_ = getBaseEnv(); }

bool Runner::provisionCommonDependencies(ProgressCb cb) {
  auto &pm = PathManager::instance();
  auto &cfg = Config::instance().getGeneral();

  fs::path pfxPath = fs::path(prefix_->getPath());
  fs::path driveC = pfxPath / "drive_c";
  fs::path fontsDir = driveC / "windows" / "Fonts";
  fs::path appDir = driveC / "Program Files (x86)" / "Microsoft" /
                    "EdgeWebView" / "Application";

  // Marker should be inside the data dir (parent of pfx)
  fs::path marker = pfxPath.parent_path() / ".rsjfw_provisioned";
  if (fs::exists(marker)) {
    LOG_DEBUG("Dependencies already provisioned (marker found)");
    if (cb)
      cb(1.0f, "environment ready.");
    return true;
  }

  LOG_INFO("Provisioning system dependencies...");
  fs::create_directories(fontsDir);

  std::vector<std::pair<std::string, std::string>> fonts = {
      {"https://sourceforge.net/projects/corefonts/files/the%20fonts/final/"
       "arial32.exe/download",
       "arial32.exe"},
      {"https://sourceforge.net/projects/corefonts/files/the%20fonts/final/"
       "times32.exe/download",
       "times32.exe"},
      {"https://sourceforge.net/projects/corefonts/files/the%20fonts/final/"
       "verdan32.exe/download",
       "verdan32.exe"}};

  LOG_INFO("Downloading fonts in parallel...");
  std::vector<std::future<bool>> dlFutures;
  std::vector<double> currentSpeeds(fonts.size(), 0.0);
  std::vector<bool> done(fonts.size(), false);
  std::mutex speedMtx;

  for (size_t i = 0; i < fonts.size(); ++i) {
    const auto &font = fonts[i];
    dlFutures.push_back(std::async(
        std::launch::async, [&pm, font, i, &currentSpeeds, &done, &speedMtx]() {
          fs::path fdest = pm.cache() / font.second;
          if (fs::exists(fdest) &&
              fs::file_size(fdest) > 100000) { // Basic sanity check
            std::lock_guard l(speedMtx);
            done[i] = true;
            return true;
          }
          auto subCb = [&](float, std::string speedStr) {
            double speed = 0;
            std::stringstream ss(speedStr);
            double val;
            std::string unit;
            if (ss >> val >> unit) {
              if (unit == "KB/s")
                speed = val * 1024;
              else if (unit == "MB/s")
                speed = val * 1024 * 1024;
              else
                speed = val;
            }
            std::lock_guard l(speedMtx);
            currentSpeeds[i] = speed;
          };
          bool ok = HTTP::download(font.first, fdest.string(), subCb);
          std::lock_guard l(speedMtx);
          done[i] = ok;
          currentSpeeds[i] = 0;
          return ok;
        }));
  }

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
            if (!downloadingList.empty())
              downloadingList += ", ";
            downloadingList += fonts[i].second;
          }
        }
        totalSpeed += currentSpeeds[i];
      }
    }
    if (allDone)
      break;
    if (cb) {
      std::string speedStr;
      if (totalSpeed < 1024)
        speedStr = std::to_string((int)totalSpeed) + " B/s";
      else if (totalSpeed < 1024 * 1024)
        speedStr = std::to_string((int)(totalSpeed / 1024)) + " KB/s";
      else
        speedStr = std::to_string((int)(totalSpeed / (1024 * 1024))) + " MB/s";
      cb(0.3f, "fonts: " + speedStr + " (dl: " + downloadingList + ")");
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }

  for (size_t i = 0; i < fonts.size(); i++) {
    if (dlFutures[i].get()) {
      fs::path fdest = pm.cache() / fonts[i].second;
      fs::path fexpand = pm.cache() / (fonts[i].second + "_ext");
      fs::remove_all(fexpand);

      auto extCb = [&](float p, std::string msg) {
          if (cb) {
              float base = 0.4f + (0.1f * i);
              float effective = base + (p * 0.1f);
              cb(effective, "fonts: " + msg);
          }
      };
      
      ZipUtil::extract(fdest.string(), fexpand.string(), extCb);
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
    std::string wv2Url =
        "https://msedge.sf.dl.delivery.mp.microsoft.com/filestreamingservice/"
        "files/ea30811e-a216-4d55-89f3-c1099862c8fc/"
        "Microsoft.WebView2.FixedVersionRuntime.143.0.3650.139.x64.cab";
    fs::path wv2Dest = pm.cache() / "webview2.cab";
    fs::path wv2Expand = pm.cache() / "webview2_expanded";

    if (!fs::exists(wv2Dest) || fs::file_size(wv2Dest) < 10000000) {
      auto dlCb = [&](float p, std::string msg) {
          if (cb) cb(0.7f + (p * 0.15f), "webview2: " + msg);
      };
      HTTP::download(wv2Url, wv2Dest.string(), dlCb);
    }

    auto extCb = [&](float p, std::string msg) {
        if (cb) cb(0.85f + (p * 0.05f), "webview2: " + msg);
    };
    fs::remove_all(wv2Expand);
    ZipUtil::extract(wv2Dest.string(), wv2Expand.string(), extCb);

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
      if (cb)
        cb(0.9f, "deploying webview2 runtime...");
      fs::remove_all(targetDir);
      fs::rename(sourceDir, targetDir);
    }

    prefix_->registryAdd(
        "HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\EdgeUpdate\\Clients\\{"
        "F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}",
        "pv", "143.0.3650.139", "REG_SZ");
    prefix_->registryAdd(
        "HKEY_LOCAL_"
        "MACHINE\\SOFTWARE\\WOW6432Node\\Microsoft\\EdgeUpdate\\Clients\\{"
        "F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}",
        "location",
        "C:\\Program Files (x86)\\Microsoft\\EdgeWebView\\Application",
        "REG_SZ");
    prefix_->registryAdd(
        "HKEY_CURRENT_USER\\Software\\Wine\\AppDefaults\\msedgewebview2.exe",
        "Version", "win7", "REG_SZ");
    prefix_->registryCommit();
  }

  std::ofstream(marker) << "done";
  if (cb)
    cb(1.0f, "environment ready.");
  return true;
}

std::map<std::string, std::string> Runner::getBaseEnv() {
  auto &cfg = Config::instance().getGeneral();
  auto &pm = PathManager::instance();
  std::map<std::string, std::string> res;

  res["WINEARCH"] = "win64";
  res["STEAM_COMPAT_CLIENT_INSTALL_PATH"] =
      (fs::path(getenv("HOME")) / ".local/share/Steam").string();
  res["STEAM_COMPAT_DATA_PATH"] = (pm.root() / "proton_data").string();

  for (const auto &[k, v] : cfg.customEnv) {
    res[k] = v;
  }

  if (Orchestrator::instance().isWineDebugEnabled()) {
    if (res.find("WINEDEBUG") == res.end()) {
      const char *wd = std::getenv("WINEDEBUG");
      res["WINEDEBUG"] = wd ? wd : "err+all";
    }
  } else {
    res["WINEDEBUG"] = "-all";
  }

  if (cfg.enableMangoHud)
    res["MANGOHUD"] = "1";

  res["PROTON_NO_ESYNC"] = cfg.enableEsync ? "0" : "1";
  res["PROTON_NO_FSYNC"] = cfg.enableFsync ? "0" : "1";

  if (cfg.enableWebView2) {
    res["WEBVIEW2_BROWSER_EXECUTABLE_FOLDER"] =
        "C:\\Program Files "
        "(x86)\\Microsoft\\EdgeWebView\\Application\\143.0.3650.139";
    res["WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS"] =
        "--no-sandbox --disable-gpu --disable-dev-shm-usage "
        "--disable-features=RendererCodeIntegrity";
  }

  std::string ovr = "winebrowser.exe=b;d3dcompiler_47=n,b;atmlib=b";
  if (cfg.dxvk) {
    ovr += ";d3d11,dxgi,d3d9,d3d10core=n,b";
  }

  if (res.count("WINEDLLOVERRIDES") && !res["WINEDLLOVERRIDES"].empty())
    res["WINEDLLOVERRIDES"] = res["WINEDLLOVERRIDES"] + ";" + ovr;
  else
    res["WINEDLLOVERRIDES"] = ovr;

  auto &gpuMgr = GpuManager::instance();
  GpuInfo selectedDevice;
  bool deviceFound = false;

  if (!cfg.selectedGpu.empty()) {
    auto devices = gpuMgr.discoverDevices();
    for (const auto &dev : devices) {
      std::string id = std::to_string(dev.pciBus) + ":" +
                       std::to_string(dev.pciSlot) + ":" +
                       std::to_string(dev.pciFunction);
      if (id == cfg.selectedGpu) {
        selectedDevice = dev;
        deviceFound = true;
        break;
      }
    }
  }

  if (!deviceFound)
    selectedDevice = gpuMgr.getBestDevice();

  auto gpuEnv = gpuMgr.getEnvVars(selectedDevice);
  for (const auto &[k, v] : gpuEnv)
    res[k] = v;

  if (cfg.enableVulkanLayer) {
    std::filesystem::path exeDir = pm.executablePath().parent_path();
    std::vector<std::filesystem::path> searchPaths = {
        exeDir / "src" / "layer", exeDir / "layer", "/usr/lib/rsjfw",
        "/usr/local/lib/rsjfw", pm.root() / "layer"};

    std::filesystem::path layerPath = "/usr/lib/rsjfw";
    for (const auto &p : searchPaths) {
      if (std::filesystem::exists(p / "libVkLayer_rsjfw_fixes.so")) {
        layerPath = p;
        break;
      }
    }

    res["VK_ADD_LAYER_PATH"] = layerPath.string();
    std::string currentLayers = res["VK_INSTANCE_LAYERS"];
    if (!currentLayers.empty())
      currentLayers += ":VK_LAYER_RSJFW_fixes";
    else
      currentLayers = "VK_LAYER_RSJFW_fixes";
    res["VK_INSTANCE_LAYERS"] = currentLayers;
    res["RSJFW_LAYER_PRESENT_MODE"] = cfg.vulkanPresentMode;
    if (cfg.enableLayerLogging)
      res["RSJFW_LAYER_LOGGING"] = "1";
  }

  if (cfg.enableRenderdoc)
    res["ENABLE_VULKAN_RENDERDOC_CAPTURE"] = "1";

  return res;
}

} // namespace rsjfw
