#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "config.h"
#include "downloader/roblox_manager.h"
#include "gui.h"
#include "logger.h"
#include "orchestrator.h"
#include "path_manager.h"

namespace fs = std::filesystem;

void showHelp() {
  std::cout << "RSJFW - Roblox Studio Just Fucking Works\n\n"
            << "Usage:\n"
            << "  rsjfw launch [protocol]   Launch Roblox Studio\n"
            << "  rsjfw config              Open configuration UI\n"
            << "  rsjfw install             Install latest version without "
               "launching\n"
            << "  rsjfw kill                Kill all running Studio instances\n"
            << "  rsjfw help                Show this help message\n\n"
            << "RSJFW SUPREMACY. VINEGAR K!!!\n";
}

void killStudio() {
  LOG_INFO("Killing Roblox Studio instances...");
  system("pkill -f RobloxStudioBeta.exe");
  system("pkill -f msedgewebview2.exe");
}

int main(int argc, char *argv[]) {
  auto &pm = rsjfw::PathManager::instance();
  pm.init();

  auto &logger = rsjfw::Logger::instance();
  auto &config = rsjfw::Config::instance();
  config.load(pm.root() / "config.json");

  std::string launchArg = "";
  bool launcherMode = false;
  bool installOnly = false;

  if (argc > 1) {
    std::string cmd = argv[1];
    if (cmd == "launch") {
      launcherMode = true;
      if (argc > 2)
        launchArg = argv[2];
    } else if (cmd == "config") {
      launcherMode = false;
    } else if (cmd == "install") {
      installOnly = true;
      launcherMode = true;
    } else if (cmd == "kill") {
      killStudio();
      return 0;
    } else if (cmd == "help" || cmd == "--help" || cmd == "-h") {
      showHelp();
      return 0;
    } else if (cmd.find("roblox-studio:") == 0 ||
               cmd.find("roblox-studio-auth:") == 0) {
      launcherMode = true;
      launchArg = cmd;
    } else {
      showHelp();
      return 1;
    }
  }

  auto &gui = rsjfw::GUI::instance();
  if (!gui.init()) {
    LOG_ERROR("Failed to initialize GUI");
    return 1;
  }

  gui.setMode(launcherMode ? rsjfw::GUI::MODE_LAUNCHER
                           : rsjfw::GUI::MODE_CONFIG);

  if (launcherMode) {
    if (installOnly) {
      rsjfw::Orchestrator::instance().startLaunch("INSTALL_ONLY");
    } else {
      rsjfw::Orchestrator::instance().startLaunch(launchArg);
    }
  }

  gui.run();

  auto &orch = rsjfw::Orchestrator::instance();
  if (launcherMode) {
    while (true) {
      auto state = orch.getState();
      if (state == rsjfw::LauncherState::FINISHED ||
          state == rsjfw::LauncherState::ERROR) {
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  }

  config.save();
  gui.shutdown();

  rsjfw::Orchestrator::instance().cancel();

  return 0;
}
