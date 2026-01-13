#include "gui/views/home_view.h"
#include "gui.h"
#include "diagnostics.h"
#include "config.h"
#include <imgui.h>
#include <thread>
#include <mutex>
#include <cmath>
#include <filesystem>

namespace rsjfw {

HomeView::HomeView() {
    Diagnostics::instance().runChecks();
}

void HomeView::render() {
    float winWidth = ImGui::GetContentRegionAvail().x;
    
    unsigned int heroTex = GUI::instance().getWideLogoTexture();
    if (heroTex != 0) {
        float aspect = (float)GUI::instance().getWideLogoWidth() / (float)GUI::instance().getWideLogoHeight();
        float displayWidth = winWidth; 
        float displayHeight = displayWidth / aspect;
        
        ImGui::SetCursorPosX(-40); 
        ImGui::Image((void*)(intptr_t)heroTex, ImVec2(displayWidth + 80, displayHeight), ImVec2(0,0), ImVec2(1,1), ImVec4(1,1,1,1));
    }

    ImGui::Dummy(ImVec2(0, 10));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 10));

    auto& gen = Config::instance().getGeneral();
    
    if (ImGui::BeginTable("Badges", 3, ImGuiTableFlags_NoBordersInBody)) {
        ImGui::TableNextRow();
        
        auto StatusCard = [](const char* id, const char* title, const char* val, const char* detail, ImVec4 color) {
            ImGui::TableSetColumnIndex(0);
            if (strcmp(id, "RN") == 0) ImGui::TableSetColumnIndex(1);
            if (strcmp(id, "DX") == 0) ImGui::TableSetColumnIndex(2);

            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.14f, 0.14f, 0.14f, 1.0f)); 
            ImGui::BeginChild(id, ImVec2(0, 110), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
            
            ImGui::SetCursorPos(ImVec2(15, 15));
            ImGui::TextDisabled("%s", title);
            
            ImGui::SetCursorPos(ImVec2(15, 38));
            ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
            ImGui::TextColored(color, "%s", val);
            ImGui::PopFont();
            
            ImGui::SetCursorPos(ImVec2(15, 62));
            ImGui::TextDisabled("%s", detail);
            
            ImGui::EndChild();
            ImGui::PopStyleColor();
        };

        std::string runnerVal = gen.runnerType;
        std::string runnerVer = "unknown";

        if (gen.runnerType == "Wine") {
            runnerVer = gen.wineSource.version;
            if (gen.wineSource.useCustomRoot) {
                std::string p = gen.wineSource.customRootPath;
                if (p.find("/usr/bin/wine") != std::string::npos) runnerVal = "system wine";
                else if (p.empty()) runnerVal = "none selected";
                else runnerVal = std::filesystem::path(p).filename().string();
            } else {
                runnerVal = gen.wineSource.repo;
            }
        } else if (gen.runnerType == "Proton") {
            runnerVer = gen.protonSource.version;
            if (gen.protonSource.useCustomRoot) {
                std::string p = gen.protonSource.customRootPath;
                if (p.find(".steam") != std::string::npos || p.find("Steam") != std::string::npos) runnerVal = "steam proton";
                else if (p.empty()) runnerVal = "none selected";
                else runnerVal = std::filesystem::path(p).filename().string();
            } else {
                runnerVal = gen.protonSource.repo;
            }
        }

        StatusCard("CH", "channel", gen.channel.c_str(), gen.robloxVersion.c_str(), ImVec4(0.2f, 0.7f, 1.0f, 1.0f));
        StatusCard("RN", "runner", runnerVal.c_str(), runnerVer.c_str(), ImVec4(1.0f, 0.5f, 0.1f, 1.0f));
        
        std::string dxVal = gen.dxvk ? gen.dxvkSource.repo : "disabled";
        std::string dxVer = gen.dxvk ? gen.dxvkSource.version : "n/a";
        StatusCard("DX", "dxvk", dxVal.c_str(), dxVer.c_str(), gen.dxvk ? ImVec4(0.2f, 0.8f, 0.2f, 1.0f) : ImVec4(0.8f, 0.2f, 0.2f, 1.0f));

        ImGui::EndTable();
    }
    
    ImGui::Dummy(ImVec2(0, 20));

    float prog = fixProgress_.load();
    if (prog >= 0.0f) {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.14f, 0.14f, 0.14f, 1.0f));
        ImGui::BeginChild("FixProgress", ImVec2(0, 85), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        {
            std::lock_guard<std::mutex> lock(statusMtx_);
            ImGui::SetCursorPos(ImVec2(15, 15));
            ImGui::Text("optimizing: %s", fixStatus_.c_str());
        }
        
        ImGui::SetCursorPos(ImVec2(15, 45));
        ImGui::PushItemWidth(ImGui::GetWindowWidth() - 140);
        ImGui::ProgressBar(prog, ImVec2(0, 25));
        ImGui::PopItemWidth();

        if (prog >= 1.0f) {
            ImGui::SetCursorPos(ImVec2(ImGui::GetWindowWidth() - 110, 45));
            if (ImGui::Button("dismiss", ImVec2(95, 25))) {
                fixProgress_ = -1.0f;
                Diagnostics::instance().runChecks();
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::Dummy(ImVec2(0, 15));
    }

    ImGui::Dummy(ImVec2(0, 50));
}

}
