#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <vector>
#include <map>
#include <filesystem>
#include <mutex>
#include <nlohmann/json.hpp>

namespace rsjfw {

struct WineSourceConfig {
    std::string repo = "vinegarhq/wine-builds";
    std::string version = "latest";
    std::string asset = "";
    std::string installedRoot = "";
    bool useCustomRoot = false;
    std::string customRootPath = "";
};

struct ProtonSourceConfig {
    std::string repo = "GloriousEggroll/proton-ge-custom";
    std::string version = "latest";
    std::string asset = "";
    std::string installedRoot = "";
    bool useCustomRoot = false;
    std::string customRootPath = "";
};

struct DxvkSourceConfig {
    std::string repo = "doitsujin/dxvk";
    std::string version = "latest";
    std::string asset = "";
    std::string installedRoot = "";
};

struct GeneralConfig {
    std::string runnerType = "Wine";
    std::string renderer = "D3D11";
    bool dxvk = true;
    
    WineSourceConfig wineSource;
    ProtonSourceConfig protonSource;
    DxvkSourceConfig dxvkSource;
    
    std::string channel = "LIVE";
    std::string robloxVersion = ""; 
    int targetFps = 60;
    std::string lightingTechnology = "Default";
    bool darkMode = true;
    
    bool desktopMode = false;
    bool multipleDesktops = false;
    std::string desktopResolution = "1920x1080";

    std::string launchWrapper = "";
    bool enableMangoHud = false;
    bool enableFsync = true;
    bool enableEsync = true;
    bool enableWebView2 = true;
    
    std::map<std::string, std::string> customEnv;
};

class Config {
public:
    static Config& instance();

    void load(const std::filesystem::path& path);
    void save();

    GeneralConfig& getGeneral() { return general_; }
    std::map<std::string, nlohmann::json>& getFFlags() { return fflags_; }
    
    nlohmann::json getClientAppSettings() const;

private:
    Config() = default;
    std::filesystem::path configPath_;
    GeneralConfig general_;
    std::map<std::string, nlohmann::json> fflags_;
    std::mutex mutex_;
};

}

#endif
