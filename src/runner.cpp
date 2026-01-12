#include "runner.h"
#include "wine.h"
#include "proton.h"
#include "path_manager.h"
#include "config.h"
#include <filesystem>

namespace rsjfw {

std::unique_ptr<Runner> Runner::createWineRunner(const std::string& wineRootPath) {
    auto pm = PathManager::instance();
    auto pfx = std::make_shared<Prefix>(pm.prefix().string(), wineRootPath);
    return std::make_unique<WineRunner>(pfx, wineRootPath);
}

std::unique_ptr<Runner> Runner::createProtonRunner(const std::string& protonRootPath) {
    return std::make_unique<ProtonRunner>(protonRootPath);
}

void Runner::addBaseEnv() {
    auto& cfg = Config::instance().getGeneral();
    env_ = cfg.customEnv;
    
    if (cfg.enableMangoHud) env_["MANGOHUD"] = "1";
    
    if (!cfg.selectedGpu.empty()) {
        env_["MESA_VK_DEVICE_SELECT"] = cfg.selectedGpu;
        
        if (cfg.selectedGpu.find("0x10de") != std::string::npos) {
            env_["__NV_PRIME_RENDER_OFFLOAD"] = "1";
            env_["__GLX_VENDOR_LIBRARY_NAME"] = "nvidia";
            env_["__VK_LAYER_NV_optimus"] = "NVIDIA_only";
        } else {
            env_["DRI_PRIME"] = "1";
        }
    }
}

}
