#ifndef CONFIG_H
#define CONFIG_H

#include <filesystem>
#include <map>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

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

struct LauncherConfig {
  bool enabled = false;
  std::string command = "";
  std::string args = "";
};

struct GeneralConfig {
  std::string runnerType = "Proton"; // Wine, Proton, UMU
  std::string renderer = "Vulkan";
  bool dxvk = true;

  WineSourceConfig wineSource;
  ProtonSourceConfig protonSource;
  DxvkSourceConfig dxvkSource;

  std::string channel = "LIVE";
  std::string robloxVersion = "";
  int targetFps = 60;
  std::string lightingTechnology = "Default";
  std::string studioTheme = "Dark"; // Default, Light, Dark

  bool desktopMode = false;
  bool multipleDesktops = false;
  std::string desktopResolution = "1920x1080";

  std::string selectedGpu = "";
  bool hideLauncher = false;
  bool autoApplyFixes = true;
  bool enableMangoHud = false;
  bool enableFsync = true;
  bool enableEsync = true;
  bool enableWebView2 = true;

  // UMU Specific
  std::string umuId = "roblox-studio";

  // Global Wrappers
  bool enableGamemode = false;
  std::string gamemodeArgs = "";
  bool enableGamescope = false;
  std::string gamescopeArgs = "-W 1920 -H 1080 -f";

  std::vector<LauncherConfig> customLaunchers;

  bool enableVulkanLayer = true;
  std::string vulkanPresentMode = "FIFO";
  bool enableLayerLogging = false;
  bool enableRenderdoc = false;

  std::map<std::string, std::string> customEnv;
};

class Config {
public:
  static Config &instance();

  void load(const std::filesystem::path &path);
  void save();

  nlohmann::json serialize() const;
  void deserialize(const nlohmann::json &j);

  GeneralConfig &getGeneral() { return general_; }
  std::map<std::string, nlohmann::json> &getFFlags() { return fflags_; }

  nlohmann::json getClientAppSettings() const;

private:
  Config() = default;
  std::filesystem::path configPath_;
  GeneralConfig general_;
  std::map<std::string, nlohmann::json> fflags_;
  std::mutex mutex_;
};

} // namespace rsjfw

#endif
