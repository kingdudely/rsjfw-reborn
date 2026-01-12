#ifndef PRESET_MANAGER_H
#define PRESET_MANAGER_H

#include <string>
#include <vector>
#include <filesystem>
#include <nlohmann/json.hpp>

namespace rsjfw {

class PresetManager {
public:
    static PresetManager& instance();

    struct PresetInfo {
        std::string name;
        std::filesystem::path path;
    };

    void refreshPresets();
    const std::vector<PresetInfo>& getPresets() const { return presets_; }

    bool savePreset(const std::string& name);
    bool loadPreset(const std::string& name);
    bool deletePreset(const std::string& name);
    bool importPreset(const std::filesystem::path& path);
    bool exportPreset(const std::string& name, const std::filesystem::path& dest);

private:
    PresetManager();
    
    std::filesystem::path presetsDir_;
    std::vector<PresetInfo> presets_;
    

    nlohmann::json sanitizeForExport(const nlohmann::json& j);
};

}

#endif
