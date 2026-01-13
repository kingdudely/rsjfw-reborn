#include "gui/views/env_view.h"
#include "config.h"
#include "runner.h"
#include <imgui.h>
#include <cstring>
#include <vector>

namespace rsjfw {

void EnvView::render() {
    auto& cfg = Config::instance().getGeneral();
    auto& customEnv = cfg.customEnv;
    
    std::unique_ptr<Runner> runner;
    if (cfg.runnerType == "Proton") {
        runner = Runner::createProtonRunner(cfg.protonSource.installedRoot);
    } else {
        runner = Runner::createWineRunner(cfg.wineSource.installedRoot);
    }
    auto baseEnv = runner->getBaseEnv();

    ImGui::Text("environment variables");
    ImGui::TextDisabled("inject custom variables into the runner process.");
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 10));
    
    if (ImGui::BeginTable("EnvTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupColumn("variable", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("action", ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableHeadersRow();
        
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        static char newKey[256] = "";
        ImGui::PushItemWidth(-1);
        ImGui::InputTextWithHint("##NewEnvKey", "variable name...", newKey, 256);
        ImGui::PopItemWidth();
        
        ImGui::TableSetColumnIndex(1);
        static char newVal[256] = "";
        ImGui::PushItemWidth(-1);
        ImGui::InputTextWithHint("##NewEnvVal", "value...", newVal, 256);
        ImGui::PopItemWidth();
        
        ImGui::TableSetColumnIndex(2);
        if (ImGui::Button("set", ImVec2(-1, 0))) {
            if (newKey[0] != '\0') {
                customEnv[newKey] = newVal;
                newKey[0] = '\0';
                newVal[0] = '\0';
            }
        }

        // Show all envs
        for (auto const& [key, val] : baseEnv) {
            bool isCustom = customEnv.count(key);
            
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            if (!isCustom) {
                ImGui::TextDisabled("(locked) %s", key.c_str());
            } else {
                ImGui::Text("%s", key.c_str());
            }
            
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%s", val.c_str());
            
            ImGui::TableSetColumnIndex(2);
            if (isCustom) {
                std::string label = "unset##" + key;
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.1f, 0.1f, 1.0f));
                if (ImGui::Button(label.c_str(), ImVec2(-1, 0))) {
                    customEnv.erase(key);
                }
                ImGui::PopStyleColor();
            } else {
                ImGui::TextDisabled(" [lock]");
            }
        }
        ImGui::EndTable();
    }
    
    ImGui::Dummy(ImVec2(0, 50));
}

}
