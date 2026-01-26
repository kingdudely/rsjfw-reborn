#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <signal.h>
#include <string>
#include <thread>
#include <vector>

#include "config.h"
#include "diagnostics.h"
#include "downloader/roblox_manager.h"
#include "gui.h"
#include "logger.h"
#include "orchestrator.h"
#include "path_manager.h"

#include "rsjfw.h"

namespace fs = std::filesystem;

void showHelp() {
  std::cout
      << "RSJFW v" << RSJFW_VERSION << " - Roblox Studio Just Fucking Works\n\n"
      << "Usage:\n"
      << "  rsjfw launch [protocol]   Launch Roblox Studio\n"
      << "  rsjfw config              Open configuration UI\n"
      << "  rsjfw register            Register desktop entry and protocol "
         "handlers\n"
      << "  rsjfw install             Install latest version without "
         "launching\n"
      << "  rsjfw kill                Kill all running Studio instances\n"
      << "  rsjfw help                Show this help message\n\n"
      << "Options:\n"
      << "  -v, --verbose             Enable debug logging\n"
      << "  --enable-wine-debug       Enable critical Wine/Proton error logs\n"
      << "  --runner={Proton|Wine|Umu} Set runner type\n"
      << "  --[no-]dxvk               Enable/disable DXVK\n"
      << "  --[no-]webview2           Enable/disable WebView2\n"
      << "  --[no-]mangohud           Enable/disable MangoHud\n"
      << "  --[no-]vulkan-layer       Enable/disable RSJFW Vulkan layer\n"
      << "  --[no-]desktop            Enable/disable virtual desktop\n\n"
      << "RSJFW SUPREMACY. VINEGAR K!!!\n";
}

void killStudio() {
  LOG_INFO("Killing Roblox Studio instances...");
  system("pkill -f RobloxStudioBeta.exe");
  system("pkill -f msedgewebview2.exe");
}

static std::atomic<bool> g_shutdown{false};

int main(int argc, char *argv[]) {
  auto &logger = rsjfw::Logger::instance();
  auto &config = rsjfw::Config::instance();
  auto &orch = rsjfw::Orchestrator::instance();
  auto &gui = rsjfw::GUI::instance();

  auto &pm = rsjfw::PathManager::instance();
  pm.init();

  sigset_t sigset;
  sigemptyset(&sigset);
  sigaddset(&sigset, SIGINT);
  sigaddset(&sigset, SIGTERM);
  pthread_sigmask(SIG_BLOCK, &sigset, nullptr);

  std::thread signalThread([]() {
    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGINT);
    sigaddset(&sigset, SIGTERM);

    int signum;
    while (true) {
      if (sigwait(&sigset, &signum) == 0) {
        LOG_INFO("Caught signal %d, initiating shutdown...", signum);
        g_shutdown = true;
        rsjfw::Orchestrator::instance().cancel();
        rsjfw::GUI::instance().shutdown();
      }
    }
  });
  signalThread.detach();

  config.load(pm.root() / "config.json");

  std::string launchArg = "";
  bool launcherMode = false;
  bool installOnly = false;
  bool verbose = false;
  bool wineDebug = false;

  std::vector<std::string> args;
  auto &general = config.getGeneral();
  bool configChanged = false;

  std::string wineDebugChannels = "err+all";

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "-v" || arg == "--verbose") {
      verbose = true;
    } else if (arg == "--enable-wine-debug") {
      wineDebug = true;
    } else if (arg.find("--wine-debug-channels=") == 0) {
      wineDebugChannels = arg.substr(22);
      wineDebug = true;
    } else if (arg.find("--runner=") == 0) {
      std::string r = arg.substr(9);
      if (strcasecmp(r.c_str(), "umu") == 0) {
        general.runnerType = "UMU";
      } else if (strcasecmp(r.c_str(), "wine") == 0) {
        general.runnerType = "Wine";
      } else if (strcasecmp(r.c_str(), "proton") == 0) {
        general.runnerType = "Proton";
      } else {
        general.runnerType = r;
      }
      configChanged = true;

    } else if (arg == "--dxvk") {
      general.dxvk = true;
      configChanged = true;
    } else if (arg == "--no-dxvk") {
      general.dxvk = false;
      configChanged = true;
    } else if (arg == "--webview2") {
      general.enableWebView2 = true;
      configChanged = true;
    } else if (arg == "--no-webview2") {
      general.enableWebView2 = false;
      configChanged = true;
    } else if (arg == "--mangohud") {
      general.enableMangoHud = true;
      configChanged = true;
    } else if (arg == "--no-mangohud") {
      general.enableMangoHud = false;
      configChanged = true;
    } else if (arg == "--vulkan-layer") {
      general.enableVulkanLayer = true;
      configChanged = true;
    } else if (arg == "--no-vulkan-layer") {
      general.enableVulkanLayer = false;
      configChanged = true;
    } else if (arg == "--desktop") {
      general.desktopMode = true;
      configChanged = true;
    } else if (arg == "--no-desktop") {
      general.desktopMode = false;
      configChanged = true;
    } else {
      args.push_back(arg);
    }
  }

  if (configChanged) {
    config.save();
  }

  logger.setVerbose(verbose);
  logger.setLogFile(pm.root() / "rsjfw.log");

  if (wineDebug) {
    rsjfw::Orchestrator::instance().setWineDebug(true);
    setenv("WINEDEBUG", wineDebugChannels.c_str(), 1);
  } else {
    setenv("WINEDEBUG", "err+all", 1);
  }

  bool fastPath = false;
  if (!args.empty()) {
    std::string cmd = args[0];
    if (cmd == "launch") {
      launcherMode = true;
      if (args.size() > 1) {
        launchArg = args[1];
        if (launchArg.find("roblox-studio-auth:") == 0) fastPath = true;
      }
    } else if (cmd == "config") {
      launcherMode = false;
    } else if (cmd == "install") {
      installOnly = true;
      launcherMode = true;
    } else if (cmd == "kill") {
      killStudio();
      return 0;
    } else if (cmd == "register") {
      LOG_INFO("Registering RSJFW desktop integration...");
      auto &diag = rsjfw::Diagnostics::instance();
      diag.runChecks();
      for (const auto &r : diag.getResults()) {
        if (r.second.category == "Integration" && !r.second.ok &&
            r.second.fixable) {
          LOG_INFO("Applying fix for: %s...", r.first.c_str());
          diag.fixIssue(r.first, [](float, std::string) {});
        }
      }
      LOG_INFO("Registration complete.");
      return 0;
    } else if (cmd == "help" || cmd == "--help" || cmd == "-h") {
      showHelp();
      return 0;
    } else if (cmd.find("roblox-studio:") == 0 ||
               cmd.find("roblox-studio-auth:") == 0) {
      launcherMode = true;
      launchArg = cmd;
      if (cmd.find("roblox-studio-auth:") == 0) fastPath = true;
    } else {
      showHelp();
      return 1;
    }
  }

  if (fastPath) {
      LOG_INFO("Fast-path auth launch detected, skipping GUI and cleaning up processes...");
      system("killall CrGpuMain >/dev/null 2>&1");
      system("killall CrBrowserMain >/dev/null 2>&1");
  } else {
      if (!gui.init()) {
        LOG_ERROR("Failed to initialize GUI");
        return 1;
      }

      gui.setMode(launcherMode ? rsjfw::GUI::MODE_LAUNCHER
                               : rsjfw::GUI::MODE_CONFIG);
  }

  if (launcherMode) {
    if (installOnly) {
      rsjfw::Orchestrator::instance().startLaunch("INSTALL_ONLY");
    } else {
      rsjfw::Orchestrator::instance().startLaunch(launchArg);
    }
  }

  if (!fastPath) gui.run();

  if (launcherMode) {
    auto state = orch.getState();
    if (state != rsjfw::LauncherState::FINISHED &&
        state != rsjfw::LauncherState::ERROR) {
      LOG_INFO("waiting for studio session to end...");
      while (true) {
        if (g_shutdown)
          break;
        state = orch.getState();
        if (state == rsjfw::LauncherState::FINISHED ||
            state == rsjfw::LauncherState::ERROR) {
          break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }
    }
  }

  LOG_INFO("shutting down...");
  config.save();
  gui.shutdown();

  rsjfw::Orchestrator::instance().shutdown();
  LOG_INFO("Exiting main...");
  return 0;
}
