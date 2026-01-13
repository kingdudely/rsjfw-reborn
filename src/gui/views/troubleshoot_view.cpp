#include "gui/views/troubleshoot_view.h"
#include "config.h"
#include "path_manager.h"
#include "downloader/roblox_manager.h"
#include <imgui.h>
#include <filesystem>

namespace rsjfw {

namespace fs = std::filesystem;

void TroubleshootView::render() {
    auto& gen = Config::instance().getGeneral();
    ImGui::Text("maintenance & repair");
    ImGui::TextColored(ImVec4(0.9f, 0.4f, 0.0f, 1.0f), "warning: destructive actions ahead.");
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 10));
    
    float w = ImGui::GetContentRegionAvail().x;
    float h = 45.0f;

    if (ImGui::Button("wipe wine prefix", ImVec2(w, h))) {
        ImGui::OpenPopup("Confirm Prefix Wipe");
    }
    
    if (ImGui::BeginPopupModal("Confirm Prefix Wipe", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("this will delete all registry and system files in the prefix.");
        ImGui::Text("installed versions will remain, but dependencies will re-install.");
        
        if (ImGui::Button("proceed", ImVec2(120, 0))) {
            auto& pm = PathManager::instance();
            try {
                if (gen.runnerType == "Proton") fs::remove_all(pm.root() / "proton_data");
                else fs::remove_all(pm.prefix());
            } catch(...) {}
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("cancel", ImVec2(120, 0))) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    
    ImGui::Dummy(ImVec2(0, 5));
    
    if (ImGui::Button("clear shader cache", ImVec2(w, h))) {
        auto& pm = PathManager::instance();
        try {
            for (auto& entry : fs::recursive_directory_iterator(pm.root())) {
                if (entry.path().extension() == ".dxvk-cache") fs::remove(entry.path());
            }
        } catch(...) {}
    }
    
    ImGui::Dummy(ImVec2(0, 5));

    if (ImGui::Button("purge downloads", ImVec2(w, h))) {
        try {
            fs::remove_all(PathManager::instance().cache());
            fs::create_directories(PathManager::instance().cache());
        } catch(...) {}
    }

    ImGui::Dummy(ImVec2(0, 5));

    if (ImGui::Button("factory reset fflags", ImVec2(w, h))) {
        Config::instance().getFFlags().clear();
    }
    
    ImGui::Dummy(ImVec2(0, 50));
}

}
