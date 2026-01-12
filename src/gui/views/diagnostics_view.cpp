#include "gui/views/diagnostics_view.h"
#include "diagnostics.h"
#include <imgui.h>

namespace rsjfw {

void DiagnosticsView::render() {
    ImGui::Text("system diagnostics");
    ImGui::TextDisabled("detailed health check of the environment.");
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 10));
    
    if (ImGui::Button("run full scan", ImVec2(150, 35))) {
        Diagnostics::instance().runChecks();
    }
    
    ImGui::Dummy(ImVec2(0, 20));
    
    auto& diag = Diagnostics::instance();
    auto& results = const_cast<std::vector<std::pair<std::string, HealthStatus>>&>(diag.getResults());
    
    if (results.empty()) {
        ImGui::TextDisabled("no results available. please run a scan.");
    }
    
    if (ImGui::BeginTable("DiagTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupColumn("status", ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableSetupColumn("check name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("message / action", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();
        
        for (auto& r : results) {
            ImGui::TableNextRow();
            
            ImGui::TableSetColumnIndex(0);
            if (r.second.ok) {
                ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "pass");
            } else if (r.second.ignored) {
                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "ignored");
            } else {
                ImGui::TextColored(ImVec4(0.9f, 0.2f, 0.2f, 1.0f), "fail");
            }
            
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%s", r.first.c_str());
            
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%s", r.second.message.c_str());
            if (!r.second.ok && r.second.fixable) {
                ImGui::SameLine();
                std::string label = "fix##" + r.first;
                if (ImGui::Button(label.c_str())) {
                    diag.fixIssue(r.first, [](float, std::string){});
                }
                ImGui::SameLine();
                std::string ignoreLabel = (r.second.ignored ? "unignore##" : "ignore##") + r.first;
                if (ImGui::Button(ignoreLabel.c_str())) {
                    r.second.ignored = !r.second.ignored;
                }
            }
        }
        ImGui::EndTable();
    }
    
    ImGui::Dummy(ImVec2(0, 50));
}

}
