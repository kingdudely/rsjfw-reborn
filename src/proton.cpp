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

namespace rsjfw {

namespace fs = std::filesystem;

ProtonRunner::ProtonRunner(std::string protonRoot)
    : Runner(nullptr, ""), protonRoot_(protonRoot) {

  fs::path root(protonRoot);
  protonScript_ = (root / "proton").string();

  auto &pm = PathManager::instance();
  compatDataPath_ = (pm.root() / "proton_data").string();
  fs::path pfxPath = fs::path(compatDataPath_) / "pfx";

  prefix_ = std::make_shared<Prefix>(pfxPath.string(), protonRoot);
  prefix_->setExecutor(protonScript_, {"run"});

  fs::path wrapperPath = pm.root() / "xdg-open-wrapper";
  {
    std::ofstream ofs(wrapperPath);
    ofs << "#!/bin/sh\n"
        << "env -u LD_LIBRARY_PATH -u LD_PRELOAD -u PYTHONPATH -u PYTHONHOME "
           "/usr/bin/xdg-open \"$@\"\n";
  }
  chmod(wrapperPath.c_str(), 0755);

  std::map<std::string, std::string> env;
  env["STEAM_COMPAT_DATA_PATH"] = compatDataPath_;
  env["STEAM_COMPAT_CLIENT_INSTALL_PATH"] =
      (fs::path(getenv("HOME")) / ".local/share/Steam").string();
  env["WINEBROWSER_HOST_EXECUTABLE"] = wrapperPath.string();
  env["WINE_XDG_OPEN_PROGRAM"] = wrapperPath.string();
  prefix_->setEnvironment(env);
}

bool ProtonRunner::configure(ProgressCb cb) {
  auto &pm = PathManager::instance();
  auto &cfg = Config::instance().getGeneral();
  fs::path appDir = fs::path(compatDataPath_) / "pfx" / "drive_c" /
                    "Program Files (x86)" / "Microsoft" / "EdgeWebView" /
                    "Application";

  if (!cfg.enableWebView2) {
    if (fs::exists(appDir)) {
      if (cb)
        cb(0.8f, "Removing WebView2...");
      fs::remove_all(fs::path(compatDataPath_) / "pfx" / "drive_c" /
                     "Program Files (x86)" / "Microsoft" / "EdgeWebView");
    }
  }

  fs::path marker = fs::path(compatDataPath_) / ".rsjfw_proton_provisioned";
  if (!fs::exists(compatDataPath_)) {
    fs::create_directories(compatDataPath_);
  }

  if (fs::exists(marker)) {
    if (cb)
      cb(1.0f, "Proton prefix ready");
    return true;
  }

  if (!prefix_->init(makeSubProgress(0.0f, 0.1f, "Initializing Prefix", cb)))
    return false;

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

  fs::path fontsDir =
      fs::path(compatDataPath_) / "pfx" / "drive_c" / "windows" / "Fonts";
  fs::create_directories(fontsDir);

  float base = 0.2f;
  float slice = 0.4f / fonts.size();

  for (size_t i = 0; i < fonts.size(); i++) {
    float start = base + i * slice;
    fs::path fdest = pm.cache() / fonts[i].second;
    if (!fs::exists(fdest)) {
      HTTP::download(fonts[i].first, fdest.string());
    }

    fs::path fexpand = pm.cache() / (fonts[i].second + "_ext_p");
    fs::remove_all(fexpand);
    ZipUtil::extract(fdest.string(), fexpand.string(), nullptr);

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

  if (cfg.enableWebView2) {
    std::string wv2Url =
        "https://msedge.sf.dl.delivery.mp.microsoft.com/filestreamingservice/"
        "files/ea30811e-a216-4d55-89f3-c1099862c8fc/"
        "Microsoft.WebView2.FixedVersionRuntime.143.0.3650.139.x64.cab";
    fs::path wv2Dest = pm.cache() / "webview2.cab";
    fs::path wv2Expand = pm.cache() / "webview2_expanded_proton";

    if (!fs::exists(wv2Dest)) {
      if (cb)
        HTTP::download(wv2Url, wv2Dest.string(),
                       makeSubProgress(0.7f, 0.8f, "DL WebView2", cb));
      else
        HTTP::download(wv2Url, wv2Dest.string());
    }

    if (cb)
      cb(0.85f, "Extracting WebView2");
    fs::remove_all(wv2Expand);
    ZipUtil::extract(wv2Dest.string(), wv2Expand.string(), nullptr);

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
      fs::remove_all(targetDir);
      fs::rename(sourceDir, targetDir);
    }

    prefix_->registryAdd("HKLM\\SOFTWARE\\WOW6432Node\\Microsoft\\EdgeUpdate\\C"
                         "lients\\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}",
                         "pv", "143.0.3650.139", "REG_SZ");
    prefix_->registryAdd(
        "HKLM\\SOFTWARE\\WOW6432Node\\Microsoft\\EdgeUpdate\\Clients\\{"
        "F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}",
        "location",
        "C:\\Program Files (x86)\\Microsoft\\EdgeWebView\\Application",
        "REG_SZ");
    prefix_->registryAdd(
        "HKCU\\Software\\Wine\\AppDefaults\\msedgewebview2.exe", "Version",
        "win7", "REG_SZ");
    prefix_->registryAdd("HKLM\\Software\\Microsoft\\EdgeUpdate\\Clients\\{"
                         "F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}",
                         "pv", "130.0.2849.46", "REG_SZ");
    prefix_->registryAdd("HKLM\\SOFTWARE\\WOW6432Node\\Microsoft\\EdgeUpdate\\C"
                         "lients\\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}",
                         "pv", "130.0.2849.46", "REG_SZ");
    prefix_->registryAdd("HKLM\\Software\\Khronos\\OpenXR\\1", "ActiveRuntime",
                         "C:\\Windows\\system32\\steam_openxr.json", "REG_SZ");
    prefix_->registryCommit();
  }

  std::ofstream(marker) << "done";
  if (cb)
    cb(1.0f, "Ready");
  return true;
}

bool ProtonRunner::runWine(const std::string &exe,
                           const std::vector<std::string> &args,
                           const std::string &taskName) {
  if (prefix_) {
    addBaseEnv();
    std::map<std::string, std::string> taskEnv = env_;
    taskEnv["WINEDEBUG"] = "-all";
    return prefix_->wine(
        exe, args,
        [taskName](const std::string &s) {
          if (!s.empty())
            std::cout << "[" << taskName << "] " << s << std::flush;
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

  std::vector<std::string> runArgs;
  auto onOut = [&](const std::string &s) { outBuffer.append(s); };

  addBaseEnv();

  auto &cfg = Config::instance().getGeneral();
  if (cfg.enableWebView2) {
    env_["WEBVIEW2_BROWSER_EXECUTABLE_FOLDER"] =
        "C:\\Program Files "
        "(x86)\\Microsoft\\EdgeWebView\\Application\\143.0.3650.139";
    env_["WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS"] =
        "--no-sandbox --disable-gpu --disable-dev-shm-usage "
        "--disable-features=RendererCodeIntegrity";
  }

  std::string ovr = "winebrowser.exe=b;d3dcompiler_47=n,b;atmlib=b";
  if (cfg.dxvk)
    ovr += ";d3d11,dxgi,d3d9,d3d10core=n,b";

  if (env_.count("WINEDLLOVERRIDES")) {
    env_["WINEDLLOVERRIDES"] = env_["WINEDLLOVERRIDES"] + ";" + ovr;
  } else {
    env_["WINEDLLOVERRIDES"] = ovr;
  }

  std::string target;
  if (cfg.desktopMode || cfg.multipleDesktops) {
    target = "explorer.exe";
    std::string res = cfg.desktopResolution;
    res.erase(std::remove(res.begin(), res.end(), ' '), res.end());
    if (res.empty())
      res = "1920x1080";

    std::string desktopName = "Desktop";
    if (cfg.multipleDesktops)
      desktopName = std::to_string(getpid());

    runArgs.push_back("/desktop=RSJFW_" + desktopName + "," + res);
    runArgs.push_back(exe.string());
  } else {
    target = exe.string();
  }

  for (const auto &a : args)
    runArgs.push_back(a);

  bool ok =
      prefix_->wine(target, runArgs, onOut, versionDir.string(), true, env_);
  return ok ? cmd::CmdResult{0, 0} : cmd::CmdResult{-1, 1};
}

}
