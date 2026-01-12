#include "gui/views/fflags_view.h"
#include "config.h"
#include <imgui.h>
#include <nlohmann/json.hpp>
#include <cstring>

namespace rsjfw {

using json = nlohmann::json;

void FFlagsView::render() {
    ImGui::Text("fast flags editor");
    ImGui::TextDisabled("modify internal roblox engine parameters.");
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 10));

    static int currentPreset = -1;
    const char* presets[] = { "performance optimized", "high quality (raytracing)", "vanilla / default" };
    
    ImGui::Text("apply preset");
    if (currentPreset == -1) {
        auto& fflags = Config::instance().getFFlags();
        auto& gen = Config::instance().getGeneral();
        if (fflags.empty() && gen.lightingTechnology == "Default") currentPreset = 2;
        else if (fflags.contains("FFlagDebugForceFutureIsBrightPhase3") && fflags["FFlagDebugForceFutureIsBrightPhase3"] == true) currentPreset = 1;
        else if (fflags.contains("FFlagDisablePostFx") && fflags["FFlagDisablePostFx"] == true) currentPreset = 0;
    }

    if (ImGui::Combo("##Presets", &currentPreset, presets, 3)) {
        auto& fflags = Config::instance().getFFlags();
        auto& gen = Config::instance().getGeneral();
        
        if (currentPreset == 0) { 
            fflags["FFlagDebugForceFutureIsBrightPhase3"] = false;
            fflags["FFlagDisablePostFx"] = true;
            gen.renderer = "Vulkan";
            gen.lightingTechnology = "ShadowMap";
        } else if (currentPreset == 1) { 
            fflags["FFlagDebugForceFutureIsBrightPhase3"] = true;
            fflags["FFlagEnableGlobalShadows"] = true;
            gen.renderer = "Vulkan";
            gen.lightingTechnology = "Future";
        } else { 
            fflags.clear();
            gen.lightingTechnology = "Default";
        }
    }

    ImGui::Dummy(ImVec2(0, 20));
    
    auto& fflags = Config::instance().getFFlags();
    
    if (ImGui::BeginTable("FFlagsTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupColumn("flag name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("value (json)", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("action", ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableHeadersRow();
        
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        static char newKey[256] = "";
        ImGui::PushItemWidth(-1);
        ImGui::InputTextWithHint("##NewKey", "flag name...", newKey, 256);
        ImGui::PopItemWidth();
        
        ImGui::TableSetColumnIndex(1);
        static char newVal[256] = "";
        ImGui::PushItemWidth(-1);
        ImGui::InputTextWithHint("##NewVal", "true, 123, \"str\"...", newVal, 256);
        ImGui::PopItemWidth();
        
        ImGui::TableSetColumnIndex(2);
        if (ImGui::Button("add", ImVec2(-1, 0))) {
            try {
                if (newKey[0] != '\0') {
                    json v = json::parse(newVal[0] == '\0' ? "null" : newVal);
                    fflags[newKey] = v;
                    newKey[0] = '\0';
                    newVal[0] = '\0';
                }
            } catch(...) {}
        }

        auto it = fflags.begin();
        while (it != fflags.end()) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%s", it->first.c_str());
            
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%s", it->second.dump().c_str());
            
            ImGui::TableSetColumnIndex(2);
            std::string label = "delete##" + it->first;
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.1f, 0.1f, 1.0f));
            if (ImGui::Button(label.c_str(), ImVec2(-1, 0))) {
                it = fflags.erase(it);
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
