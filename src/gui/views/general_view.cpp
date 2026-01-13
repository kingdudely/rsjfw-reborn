#include "gui/views/general_view.h"
#include "config.h"
#include "preset_manager.h"
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

struct GpuInfo {
    std::string name;
    std::string id;
};

static std::vector<GpuInfo> discoverGpus() {
    std::vector<GpuInfo> gpus;
    gpus.push_back({"automatic / default", ""});
    try {
        std::vector<std::string> cards;
        for (const auto& entry : fs::directory_iterator("/sys/class/drm")) {
            std::string name = entry.path().filename().string();
            if (name.find("card") == 0 && name.find("-") == std::string::npos) {
                cards.push_back(name);
            }
        }
        // Sort cards (card0, card1...) to match DRI_PRIME index expectations
        std::sort(cards.begin(), cards.end());

        for (size_t i = 0; i < cards.size(); ++i) {
            fs::path cardPath = fs::path("/sys/class/drm") / cards[i];
            fs::path devicePath = cardPath / "device";
            if (fs::exists(devicePath / "vendor") && fs::exists(devicePath / "device")) {
                std::ifstream v(devicePath / "vendor");
                std::ifstream d(devicePath / "device");
                std::string vendor, device;
                v >> vendor;
                d >> device;
                
                std::string vendorName = vendor;
                if (vendor == "0x10de") vendorName = "NVIDIA";
                else if (vendor == "0x1002") vendorName = "AMD";
                else if (vendor == "0x8086") vendorName = "Intel";
                
                GpuInfo info;
                // Use the card index for DRI_PRIME
                info.id = std::to_string(i);
                
                std::string pciAddr = "";
                if (fs::is_symlink(devicePath)) {
                    pciAddr = " [" + fs::read_symlink(devicePath).filename().string() + "]";
                }

                info.name = vendorName + " " + device + " (" + cards[i] + ")" + pciAddr;
                gpus.push_back(info);
            }
        }
    } catch (...) {}
    return gpus;
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
        }
    }
    ImGui::SameLine(0, 5);
    if (ImGui::Button("save as...", ImVec2(btnW, 0))) {
        ImGui::OpenPopup("SavePresetPopup");
    }
    ImGui::SameLine(0, 5);
    if (ImGui::Button("delete", ImVec2(btnW, 0))) {
        if (selectedPreset >= 0 && selectedPreset < (int)presets.size()) {
            pm.deletePreset(presets[selectedPreset].name);
            selectedPreset = -1;
        }
    }
    
    ImGui::Dummy(ImVec2(0, 5));
    ImGui::SetCursorPosX(180);
    if (ImGui::Button("import...", ImVec2(btnW, 0))) {
        ImGuiFileDialog::Instance()->OpenDialog("ImportPreset", "import .rsjfwpreset", ".rsjfwpreset", ".", 1, nullptr, ImGuiFileDialogFlags_Modal);
    }
    ImGui::SameLine(0, 5);
    if (ImGui::Button("export...", ImVec2(btnW, 0))) {
        if (selectedPreset >= 0 && selectedPreset < (int)presets.size()) {
            ImGuiFileDialog::Instance()->OpenDialog("ExportPreset", "export preset", ".rsjfwpreset", ".", 1, nullptr, ImGuiFileDialogFlags_Modal);
        }
    }

    if (ImGui::BeginPopupModal("SavePresetPopup", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        static char nameBuf[64] = "";
        ImGui::InputText("name", nameBuf, 64);
        if (ImGui::Button("save", ImVec2(120, 0))) {
            pm.savePreset(nameBuf);
            nameBuf[0] = '\0';
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("cancel", ImVec2(120, 0))) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    if (ImGuiFileDialog::Instance()->Display("ImportPreset", ImGuiWindowFlags_NoCollapse, ImVec2(400, 600))) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            pm.importPreset(ImGuiFileDialog::Instance()->GetFilePathName());
        }
        ImGuiFileDialog::Instance()->Close();
    }
    if (ImGuiFileDialog::Instance()->Display("ExportPreset", ImGuiWindowFlags_NoCollapse, ImVec2(400, 600))) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            std::string path = ImGuiFileDialog::Instance()->GetFilePathName();
            if (path.find(".rsjfwpreset") == std::string::npos) path += ".rsjfwpreset";
            if (selectedPreset >= 0) {
                pm.exportPreset(presets[selectedPreset].name, path);
            }
        }
        ImGuiFileDialog::Instance()->Close();
    }

    ImGui::Dummy(ImVec2(0, 20));

    ImGui::Text("core configuration");
    ImGui::Separator();
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
    
    ImGui::Text("primary gpu");
    ImGui::SameLine(180);
    static std::vector<GpuInfo> discoveredGpus = discoverGpus();
    std::string currentGpuName = "automatic / default";
    for (const auto& g : discoveredGpus) {
        if (g.id == gen.selectedGpu) {
            currentGpuName = g.name;
            break;
        }
    }
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 10);
    if (ImGui::BeginCombo("##GpuSelector", currentGpuName.c_str())) {
        for (const auto& g : discoveredGpus) {
            bool isSelected = (gen.selectedGpu == g.id);
            if (ImGui::Selectable(g.name.c_str(), isSelected)) {
                gen.selectedGpu = g.id;
            }
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
        if (fps == 0) Config::instance().getFFlags()["DFIntTaskSchedulerTargetFps"] = 999;
        else Config::instance().getFFlags()["DFIntTaskSchedulerTargetFps"] = fps;
    }
    
    ImGui::Dummy(ImVec2(0, 10));
    ImGui::Text("lighting engine");
    ImGui::SameLine(180);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 10);
    if (ImGui::BeginCombo("##Lighting", gen.lightingTechnology.c_str())) {
        const char* techs[] = {"default", "future", "shadowmap"};
        for (const auto& t : techs) {
            bool selected = (gen.lightingTechnology == t);
            if (ImGui::Selectable(t, selected)) gen.lightingTechnology = t;
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::Dummy(ImVec2(0, 20));
    ImGui::Checkbox("enable dxvk translation", &gen.dxvk);
    ImGui::Checkbox("force dark mode", &gen.darkMode);
    ImGui::Checkbox("hide launcher ui", &gen.hideLauncher);
    ImGui::Checkbox("auto-apply fixes", &gen.autoApplyFixes);

    ImGui::Dummy(ImVec2(0, 20));
    ImGui::Text("window management");
    ImGui::Separator();
    ImGui::Checkbox("virtual desktop container", &gen.desktopMode);
    ImGui::Checkbox("isolated session", &gen.multipleDesktops);
    
    ImGui::Text("resolution (wxh)");
    ImGui::SameLine(180);
    InputTextString("##Res", gen.desktopResolution, ImGui::GetContentRegionAvail().x - 10);
    
    ImGui::Dummy(ImVec2(0, 50));
}

}
