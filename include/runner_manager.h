#ifndef RUNNER_MANAGER_H
#define RUNNER_MANAGER_H

#include "runner.h"
#include <memory>
#include <mutex>

namespace rsjfw {

class RunnerManager {
public:
    static RunnerManager& instance();

    // Returns the current active runner. Builds it if it doesn't exist.
    std::shared_ptr<Runner> get();

    // Rebuilds the runner based on current config
    void refresh();

private:
    RunnerManager() = default;
    std::shared_ptr<Runner> currentRunner_;
    std::mutex mutex_;
};

}

#endif
