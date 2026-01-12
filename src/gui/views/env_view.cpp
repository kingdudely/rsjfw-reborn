#include "gui/views/env_view.h"
#include "config.h"
#include <imgui.h>
#include <cstring>

namespace rsjfw {

void EnvView::render() {
    auto& env = Config::instance().getGeneral().customEnv;
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
                env[newKey] = newVal;
                newKey[0] = '\0';
                newVal[0] = '\0';
            }
        }

        auto it = env.begin();
        while (it != env.end()) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%s", it->first.c_str());
            
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%s", it->second.c_str());
            
            ImGui::TableSetColumnIndex(2);
            std::string label = "unset##" + it->first;
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.1f, 0.1f, 1.0f));
            if (ImGui::Button(label.c_str(), ImVec2(-1, 0))) {
                it = env.erase(it);
            } else {
                it++;
            }
            ImGui::PopStyleColor();
        }
        ImGui::EndTable();
    }
    
    ImGui::Dummy(ImVec2(0, 50));
}

}
