#include "runner_manager.h"
#include "config.h"
#include "logger.h"

namespace rsjfw {

RunnerManager &RunnerManager::instance() {
  static RunnerManager inst;
  return inst;
}

std::shared_ptr<Runner> RunnerManager::get() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!currentRunner_) {
    auto &gen = Config::instance().getGeneral();

    std::string runnerRoot;
    if (gen.runnerType == "Proton") {
      runnerRoot = gen.protonSource.useCustomRoot
                       ? gen.protonSource.customRootPath
                       : gen.protonSource.installedRoot;
    } else if (gen.runnerType == "UMU") {
      if (!gen.protonSource.useCustomRoot &&
          gen.protonSource.customRootPath == "GE-Proton") {
        runnerRoot = "GE-Proton";
      } else {
        runnerRoot = gen.protonSource.useCustomRoot
                         ? gen.protonSource.customRootPath
                         : gen.protonSource.installedRoot;
      }

      if (runnerRoot.empty()) {
        runnerRoot = "GE-Proton";
      }
    } else {
      runnerRoot = gen.wineSource.useCustomRoot ? gen.wineSource.customRootPath
                                                : gen.wineSource.installedRoot;
    }

    if (runnerRoot.empty() && gen.runnerType != "UMU") {
      return nullptr;
    }

    if (gen.runnerType == "Proton") {
      currentRunner_ = Runner::createProtonRunner(runnerRoot);
    } else if (gen.runnerType == "UMU") {
      currentRunner_ = Runner::createUmuRunner(runnerRoot);
    } else {
      currentRunner_ = Runner::createWineRunner(runnerRoot);
    }
  }
  return currentRunner_;
}

void RunnerManager::refresh() {
  std::lock_guard<std::mutex> lock(mutex_);
  currentRunner_.reset();
}

} // namespace rsjfw
