#include "gui/view.h"
#include "diagnostics.h"
#include <imgui.h>
#include <vector>

namespace rsjfw {

class HomeView : public View {
public:
    void render() override {
        ImGui::Text("Environment Status");
        ImGui::Separator();
        
        auto& diag = Diagnostics::instance();
        auto results = diag.getResults();
        
        if (ImGui::Button("Run Diagnostics")) {
            diag.runChecks();
        }
        
        ImGui::BeginChild("StatusCards", ImVec2(0, 0), true);
        for (const auto& [name, status] : results) {
            ImVec4 color = status.ok ? ImVec4(0, 1, 0, 1) : ImVec4(1, 0, 0, 1);
            ImGui::TextColored(color, "[%s]", status.ok ? "OK" : "FAIL");
            ImGui::SameLine();
            ImGui::Text("%s: %s", name.c_str(), status.message.c_str());
            
            if (!status.ok && status.fixable) {
                ImGui::SameLine();
                std::string btnLabel = "Fix##" + name;
                if (ImGui::Button(btnLabel.c_str())) {
                    diag.fixIssue(name, [](float, std::string){});
                }
            }
            
            if (!status.detail.empty()) {
                ImGui::TextDisabled("  %s", status.detail.c_str());
            }
        }
        ImGui::EndChild();
    }
    
    const char* getName() const override { return "Home"; }
};

}
