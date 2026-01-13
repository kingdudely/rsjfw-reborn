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
#include "diagnostics.h"

#include "rsjfw.h"

namespace fs = std::filesystem;

void showHelp() {
  std::cout << "RSJFW v" << RSJFW_VERSION << " - Roblox Studio Just Fucking Works\n\n"
            << "Usage:\n"
            << "  rsjfw launch [protocol]   Launch Roblox Studio\n"
            << "  rsjfw config              Open configuration UI\n"
            << "  rsjfw register            Register desktop entry and protocol handlers\n"
            << "  rsjfw install             Install latest version without "
               "launching\n"
            << "  rsjfw kill                Kill all running Studio instances\n"
            << "  rsjfw help                Show this help message\n\n"
            << "Options:\n"
            << "  -v, --verbose             Enable debug logging\n\n"
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
  bool verbose = false;

  std::vector<std::string> args;
  for (int i = 1; i < argc; ++i) {
      std::string arg = argv[i];
      if (arg == "-v" || arg == "--verbose") {
          verbose = true;
      } else {
          args.push_back(arg);
      }
  }

  logger.setVerbose(verbose);
  logger.setLogFile(pm.root() / "rsjfw.log");

  if (!args.empty()) {
    std::string cmd = args[0];
    if (cmd == "launch") {
      launcherMode = true;
      if (args.size() > 1)
        launchArg = args[1];
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
      auto& diag = rsjfw::Diagnostics::instance();
      diag.runChecks();
      for (const auto& r : diag.getResults()) {
          if (r.second.category == "Integration" && !r.second.ok && r.second.fixable) {
              LOG_INFO("Applying fix for: %s...", r.first.c_str());
              diag.fixIssue(r.first, [](float, std::string){});
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
    auto state = orch.getState();
    if (state != rsjfw::LauncherState::FINISHED && state != rsjfw::LauncherState::ERROR) {
        LOG_INFO("waiting for studio session to end...");
        while (true) {
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

  rsjfw::Orchestrator::instance().cancel();
  return 0;
}
