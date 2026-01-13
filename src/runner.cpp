#include "runner.h"
#include "wine.h"
#include "proton.h"
#include "path_manager.h"
#include "config.h"
#include <filesystem>
#include <fstream>

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
    env_ = getBaseEnv();
}

std::map<std::string, std::string> Runner::getBaseEnv() {
    auto& cfg = Config::instance().getGeneral();
    std::map<std::string, std::string> res = cfg.customEnv;
    
    res["WINEDEBUG"] = "-all";
    if (cfg.enableMangoHud) res["MANGOHUD"] = "1";
    
    if (!cfg.selectedGpu.empty()) {
        // Find GPU info to determine vendor
        std::string vendor = "";
        try {
            int idx = std::stoi(cfg.selectedGpu);
            std::vector<std::string> cards;
            for (const auto& entry : std::filesystem::directory_iterator("/sys/class/drm")) {
                std::string name = entry.path().filename().string();
                if (name.find("card") == 0 && name.find("-") == std::string::npos) cards.push_back(name);
            }
            std::sort(cards.begin(), cards.end());
            if (idx >= 0 && idx < (int)cards.size()) {
                std::ifstream v(std::filesystem::path("/sys/class/drm") / cards[idx] / "device" / "vendor");
                v >> vendor;
            }
        } catch(...) {}

        res["MESA_VK_DEVICE_SELECT"] = cfg.selectedGpu;
        res["MESA_D3D12_DEFAULT_ADAPTER_NAME"] = cfg.selectedGpu;
        res["DRI_PRIME"] = cfg.selectedGpu;

        if (vendor == "0x10de") {
            res["__NV_PRIME_RENDER_OFFLOAD"] = "1";
            res["__GLX_VENDOR_LIBRARY_NAME"] = "nvidia";
            res["__VK_LAYER_NV_optimus"] = "NVIDIA_only";
            res["VK_ICD_FILENAMES"] = "/usr/share/vulkan/icd.d/nvidia_icd.json";
        } else if (vendor == "0x1002") {
            res["AMD_VULKAN_ICD"] = "RADV";
            res["VK_ICD_FILENAMES"] = "/usr/share/vulkan/icd.d/radeon_icd.x86_64.json";
        } else if (vendor == "0x8086") {
            res["VK_ICD_FILENAMES"] = "/usr/share/vulkan/icd.d/intel_icd.x86_64.json";
        }
    }
    return res;
}

}
