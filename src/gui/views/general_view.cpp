#include "gui/views/general_view.h"
#include "config.h"
#include "preset_manager.h"
#include "gpu_manager.h"
#include "runner_manager.h"
#include "gui/icons.h"
#include <imgui.h>
#include <ImGuiFileDialog.h>
#include <cstring>
#include <vector>
#include <filesystem>
#include <fstream>

namespace rsjfw {

namespace fs = std::filesystem;

static bool InputTextString(const char* label, std::string& str, float width = -1.0f) {
    char buf[512];
    strncpy(buf, str.c_str(), sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    if (width > 0) ImGui::SetNextItemWidth(width);
    if (ImGui::InputText(label, buf, sizeof(buf))) {
        str = buf;
        return true;
    }
    return false;
}

struct GpuUiOption {
    std::string name;
    std::string id;
};

static std::string getDeviceTypeString(VkPhysicalDeviceType type) {
    switch (type) {
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return "integrated";
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: return "discrete";
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: return "virtual";
        case VK_PHYSICAL_DEVICE_TYPE_CPU: return "cpu";
        default: return "other";
    }
}

static std::vector<GpuUiOption> getGpuOptions() {
    std::vector<GpuUiOption> options;
    options.push_back({"automatic / default", ""});
    try {
        auto devices = GpuManager::instance().discoverDevices();
        for (const auto& dev : devices) {
            std::string typeStr = getDeviceTypeString(dev.deviceType);
            std::string displayName = dev.name + " (" + typeStr + ")";
            std::string id = std::to_string(dev.pciBus) + ":" + 
                           std::to_string(dev.pciSlot) + ":" + 
                           std::to_string(dev.pciFunction);
            options.push_back({displayName, id});
        }
    } catch (...) {}
    return options;
}

void GeneralView::render() {
    auto& gen = Config::instance().getGeneral();
    auto& pm = PresetManager::instance();
    
    ImGui::Text("configuration presets");
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 10));
    
    static int selectedPreset = -1;
    const auto& presets = pm.getPresets();
    
    std::string preview = "select preset...";
    if (selectedPreset >= 0 && selectedPreset < (int)presets.size()) {
        preview = presets[selectedPreset].name;
    }
    
    ImGui::Text("active preset");
    ImGui::SameLine(180);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 10);
    if (ImGui::BeginCombo("##PresetsCombo", preview.c_str())) {
        for (int i = 0; i < (int)presets.size(); ++i) {
            bool isSelected = (selectedPreset == i);
            if (ImGui::Selectable(presets[i].name.c_str(), isSelected)) {
                selectedPreset = i;
            }
            if (isSelected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    
    ImGui::Dummy(ImVec2(0, 5));
    float availW = ImGui::GetContentRegionAvail().x - 180 - 10;
    float btnW = (availW - 10) / 3.0f; 
    
    ImGui::SetCursorPosX(180);
    if (ImGui::Button("load", ImVec2(btnW, 0))) {
        if (selectedPreset >= 0 && selectedPreset < (int)presets.size()) {
            pm.loadPreset(presets[selectedPreset].name);
            RunnerManager::instance().refresh();
        }
    }
    ImGui::SameLine(0, 5);
    if (ImGui::Button("save as...", ImVec2(btnW, 0))) ImGui::OpenPopup("SavePresetPopup");
    ImGui::SameLine(0, 5);
    if (ImGui::Button("delete", ImVec2(btnW, 0))) {
        if (selectedPreset >= 0 && selectedPreset < (int)presets.size()) { pm.deletePreset(presets[selectedPreset].name); selectedPreset = -1; }
    }
    
    ImGui::Dummy(ImVec2(0, 20));

    ImGui::Text("core configuration");
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 10));

    ImGui::Text("environment type");
    ImGui::SameLine(180);
    const char* runnerTypes[] = { "wine", "proton", "umu" };
    int currentType = 0;
    if (gen.runnerType == "Proton") currentType = 1;
    else if (gen.runnerType == "UMU") currentType = 2;

    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 10);
    if (ImGui::Combo("##RunnerType", &currentType, runnerTypes, 3)) {
        gen.runnerType = (currentType == 0) ? "Wine" : (currentType == 1) ? "Proton" : "UMU";
        RunnerManager::instance().refresh();
    }
    
    ImGui::Dummy(ImVec2(0, 10));
    ImGui::Text("graphics api");
    ImGui::SameLine(180);
    const char* renderers[] = { "automatic", "d3d11", "vulkan", "opengl" };
    int currentRenderer = 0;
    if (gen.renderer == "D3D11") currentRenderer = 1;
    else if (gen.renderer == "Vulkan") currentRenderer = 2;
    else if (gen.renderer == "OpenGL") currentRenderer = 3;

    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 10);
    if (ImGui::Combo("##Renderer", &currentRenderer, renderers, 4)) {
        gen.renderer = (currentRenderer == 0) ? "Automatic" : (currentRenderer == 1) ? "D3D11" : (currentRenderer == 2) ? "Vulkan" : "OpenGL";
    }
    
    ImGui::Dummy(ImVec2(0, 10));
    ImGui::Text("studio theme");
    ImGui::SameLine(180);
    const char* themes[] = { "default", "light", "dark" };
    int currentTheme = (gen.studioTheme == "Light") ? 1 : (gen.studioTheme == "Dark") ? 2 : 0;
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 10);
    if (ImGui::Combo("##Theme", &currentTheme, themes, 3)) {
        gen.studioTheme = (currentTheme == 1) ? "Light" : (currentTheme == 2) ? "Dark" : "Default";
    }

    ImGui::Dummy(ImVec2(0, 10));
    ImGui::Text("primary gpu");
    ImGui::SameLine(180);
    static std::vector<GpuUiOption> discoveredGpus = getGpuOptions();
    std::string currentGpuName = "automatic / default";
    for (const auto& g : discoveredGpus) if (g.id == gen.selectedGpu) { currentGpuName = g.name; break; }
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 10);
    if (ImGui::BeginCombo("##GpuSelector", currentGpuName.c_str())) {
        for (const auto& g : discoveredGpus) {
            bool isSelected = (gen.selectedGpu == g.id);
            if (ImGui::Selectable(g.name.c_str(), isSelected)) gen.selectedGpu = g.id;
            if (isSelected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::Dummy(ImVec2(0, 10));
    ImGui::Text("target fps");
    ImGui::SameLine(180);
    int fps = gen.targetFps;
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 10);
    if (ImGui::SliderInt("##FPS", &fps, 0, 560, fps == 0 ? "unlimited" : "%d fps")) {
        gen.targetFps = fps;
        Config::instance().getFFlags()["DFIntTaskSchedulerTargetFps"] = (fps == 0) ? 999 : fps;
    }
    
    ImGui::Dummy(ImVec2(0, 20));
    ImGui::Checkbox("enable dxvk translation", &gen.dxvk);
    ImGui::Checkbox("hide launcher ui", &gen.hideLauncher);
    ImGui::Checkbox("auto-apply fixes", &gen.autoApplyFixes);

    ImGui::Dummy(ImVec2(0, 20));
    ImGui::Text("performance & compatibility (wrappers)");
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 10));

    auto WrapperRow = [](const char* label, bool* enabled, std::string* args, const char* tooltip) {
        ImGui::Checkbox(label, enabled);
        if (tooltip && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tooltip);
        ImGui::SameLine(180);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 10);
        std::string tag = std::string("##") + label + "Args";
        InputTextString(tag.c_str(), *args);
    };

    if (gen.runnerType == "UMU") {
        static bool alwaysTrue = true;
        WrapperRow("umu id", &alwaysTrue, &gen.umuId, "umu-run --app-id <id>");
    }
    WrapperRow("gamemode", &gen.enableGamemode, &gen.gamemodeArgs, "gamemoderun <args>");
    WrapperRow("gamescope", &gen.enableGamescope, &gen.gamescopeArgs, "gamescope <args> --");

    for (size_t i = 0; i < gen.customLaunchers.size(); ++i) {
        auto& cl = gen.customLaunchers[i];
        std::string label = "custom #" + std::to_string(i+1);
        ImGui::Checkbox(label.c_str(), &cl.enabled);
        ImGui::SameLine(180);
        ImGui::SetNextItemWidth(120);
        InputTextString(("##CLCmd" + std::to_string(i)).c_str(), cl.command);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 40);
        InputTextString(("##CLArgs" + std::to_string(i)).c_str(), cl.args);
        ImGui::SameLine();
        if (ImGui::Button(("X##DelCL" + std::to_string(i)).c_str())) {
            gen.customLaunchers.erase(gen.customLaunchers.begin() + i);
        }
    }
    if (ImGui::Button("add custom wrapper")) gen.customLaunchers.push_back({false, "", ""});

    ImGui::Dummy(ImVec2(0, 20));
    ImGui::Text("vulkan layer fixes");
    ImGui::Separator();
    ImGui::Checkbox("enable rsjfw fixes layer", &gen.enableVulkanLayer);
    ImGui::SameLine(250);
    const char* presentModes[] = { "FIFO (V-Sync)", "MAILBOX (Fast)" };
    int currentPresentMode = (gen.vulkanPresentMode == "FIFO") ? 0 : 1;
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 10);
    if (ImGui::Combo("##PresentMode", &currentPresentMode, presentModes, 2)) gen.vulkanPresentMode = (currentPresentMode == 0) ? "FIFO" : "MAILBOX";

    ImGui::Dummy(ImVec2(0, 20));
    ImGui::Text("window management");
    ImGui::Separator();
    ImGui::Checkbox("virtual desktop", &gen.desktopMode);
    ImGui::SameLine(250);
    InputTextString("resolution##Res", gen.desktopResolution, ImGui::GetContentRegionAvail().x - 10);
    
    ImGui::Dummy(ImVec2(0, 50));
}

}
