#include "gui/views/home_view.h"
#include "gui.h"
#include "config.h"
#include "path_manager.h"
#include "runner_manager.h"
#include "http.h"
#include "async_image_loader.h"
#include <imgui.h>
#include <filesystem>
#include <thread>

namespace rsjfw {

HomeView::HomeView() {
    refreshUsers();
}

void HomeView::refreshUsers() {
    if (refreshing_) return;
    refreshing_ = true;

    std::thread([this]() {
        auto runner = RunnerManager::instance().get();
        if (runner) {
            auto users = CredentialManager::instance().getLoggedInUsers(runner->getPrefix());
            std::lock_guard<std::mutex> lock(usersMtx_);
            cachedUsers_ = std::move(users);
        }
        refreshing_ = false;
    }).detach();
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
            runnerVal = gen.wineSource.useCustomRoot ? "custom" : gen.wineSource.repo;
        } else if (gen.runnerType == "Proton") {
            runnerVer = gen.protonSource.version;
            runnerVal = gen.protonSource.useCustomRoot ? "custom" : gen.protonSource.repo;
        } else {
            runnerVer = "umu-launcher";
            runnerVal = gen.umuId;
        }

        StatusCard("CH", "channel", gen.channel.c_str(), gen.robloxVersion.c_str(), ImVec4(0.2f, 0.7f, 1.0f, 1.0f));
        StatusCard("RN", "runner", runnerVal.c_str(), runnerVer.c_str(), ImVec4(1.0f, 0.5f, 0.1f, 1.0f));
        StatusCard("DX", "dxvk", gen.dxvk ? "enabled" : "disabled", gen.dxvkSource.version.c_str(), gen.dxvk ? ImVec4(0.2f, 0.8f, 0.2f, 1.0f) : ImVec4(0.8f, 0.2f, 0.2f, 1.0f));

        ImGui::EndTable();
    }
    
    ImGui::Dummy(ImVec2(0, 20));
    ImGui::Text("logged in accounts");
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 10));

    std::vector<RobloxUser> users;
    {
        std::lock_guard<std::mutex> lock(usersMtx_);
        users = cachedUsers_;
    }

    if (users.empty()) {
        if (refreshing_) {
            ImGui::TextColored(ImVec4(1, 1, 0, 1), "refreshing accounts...");
        } else {
            ImGui::TextDisabled("no accounts found in this prefix.");
            if (ImGui::Button("refresh accounts")) refreshUsers();
        }
    } else {
        for (const auto& user : users) {
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.12f, 0.12f, 1.0f));
            ImGui::BeginChild(user.userId.c_str(), ImVec2(0, 80), true, ImGuiWindowFlags_NoScrollbar);
            
            unsigned int tex = AsyncImageLoader::instance().getTexture(user.profilePicUrl);
            
            ImGui::SetCursorPos(ImVec2(15, 15));
            if (tex) {
                ImGui::Image((void*)(intptr_t)tex, ImVec2(50, 50));
            } else {
                ImDrawList* drawList = ImGui::GetWindowDrawList();
                ImVec2 pos = ImGui::GetCursorScreenPos();
                drawList->AddRectFilled(pos, ImVec2(pos.x + 50, pos.y + 50), IM_COL32(40, 40, 40, 255));
                ImGui::Dummy(ImVec2(50, 50));
            }

            ImGui::SameLine(80);
            ImGui::BeginGroup();
            ImGui::Dummy(ImVec2(0, 5));
            ImGui::Text("%s", user.username.c_str());
            ImGui::TextDisabled("ID: %s", user.userId.c_str());
            ImGui::EndGroup();
            
            ImGui::EndChild();
            ImGui::PopStyleColor();
            ImGui::Dummy(ImVec2(0, 5));
        }
        
        if (refreshing_) {
            ImGui::TextColored(ImVec4(1, 1, 0, 1), "refreshing...");
        } else {
            if (ImGui::Button("refresh accounts")) refreshUsers();
        }
    }

    ImGui::Dummy(ImVec2(0, 50));
}

}
