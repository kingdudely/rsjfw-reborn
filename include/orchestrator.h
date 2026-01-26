#ifndef ORCHESTRATOR_H
#define ORCHESTRATOR_H

#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <functional>
#include <thread>

namespace rsjfw {

enum class LauncherState {
    IDLE,
    BOOTSTRAPPING,
    DOWNLOADING_ROBLOX,
    PREPARING_WINE,
    INSTALLING_DXVK,
    APPLYING_CONFIG,
    LAUNCHING_STUDIO,
    RUNNING,
    FINISHED,
    ERROR
};

class Orchestrator {
public:
    static Orchestrator& instance();

    void startLaunch(const std::string& arg);
    void cancel();
    void shutdown();
    void setWineDebug(bool enable) { wineDebug_ = enable; }
    bool isWineDebugEnabled() const { return wineDebug_; }

    LauncherState getState() const { return state_; }
    float getProgress() const { return progress_; }
    std::string getStatus() const { std::lock_guard<std::mutex> l(mutex_); return status_; }
    std::string getError() const { std::lock_guard<std::mutex> l(mutex_); return error_; }

private:
    Orchestrator() = default;
    ~Orchestrator();

    void worker(std::string arg);
    void setStatus(float p, const std::string& s);
    void setState(LauncherState s);
    void setError(const std::string& e);

    std::atomic<LauncherState> state_{LauncherState::IDLE};
    std::atomic<float> progress_{0.0f};
    mutable std::mutex mutex_;
    std::string status_;
    std::string error_;
    
    std::thread workerThread_;
    std::atomic<bool> stop_{false};
    std::atomic<bool> wineDebug_{false};
};

}

#endif
