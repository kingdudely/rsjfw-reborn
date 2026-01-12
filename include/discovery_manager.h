#ifndef DISCOVERY_MANAGER_H
#define DISCOVERY_MANAGER_H

#include <string>
#include <vector>
#include <filesystem>

namespace rsjfw {

enum class RunnerType {
    Wine,
    Proton,
    System
};

struct RunnerInfo {
    std::string name;
    std::filesystem::path path;
    RunnerType type;
    std::string version;
};

class DiscoveryManager {
public:
    static DiscoveryManager& instance();

    void scan();
    std::vector<RunnerInfo> getRunners(RunnerType type) const;
    std::vector<RunnerInfo> getAllRunners() const;

private:
    DiscoveryManager() = default;
    
    void scanSteamProton();
    void scanSystemWine();
    
    std::vector<RunnerInfo> runners_;
};

}

#endif
