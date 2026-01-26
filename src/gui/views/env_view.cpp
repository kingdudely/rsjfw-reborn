#include "gui/views/env_view.h"
#include "config.h"
#include "runner_manager.h"
#include <algorithm>
#include <cstring>
#include <imgui.h>
#include <vector>

namespace rsjfw {

void EnvView::refreshEnv() {
  auto runner = RunnerManager::instance().get();
  if (runner) {
    cachedBaseEnv_ = runner->getBaseEnv();
    envLoaded_ = true;
  }
}

void EnvView::render() {
  if (!envLoaded_)
    refreshEnv();

  auto &cfg = Config::instance().getGeneral();
  auto &customEnv = cfg.customEnv;

  ImGui::Text("environment variables");
  ImGui::TextDisabled("inject custom variables into the runner process.");
  ImGui::SameLine(ImGui::GetWindowWidth() - 350);
  if (ImGui::Button("refresh", ImVec2(100, 0))) {
    envLoaded_ = false;
  }

  ImGui::Separator();
  ImGui::Dummy(ImVec2(0, 10));

  if (ImGui::BeginTable("EnvTable", 3,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_Resizable |
                            ImGuiTableFlags_ScrollY)) {
    ImGui::TableSetupColumn("variable", ImGuiTableColumnFlags_WidthStretch,
                            0.4f);
    ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch, 0.6f);
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
        envLoaded_ = false;
      }
    }

    std::vector<std::pair<std::string, std::string>> sortedEnv(
        cachedBaseEnv_.begin(), cachedBaseEnv_.end());
    std::sort(sortedEnv.begin(), sortedEnv.end(),
              [&](const auto &a, const auto &b) {
                bool aCustom = customEnv.count(a.first);
                bool bCustom = customEnv.count(b.first);
                if (aCustom != bCustom)
                  return aCustom;
                return a.first < b.first;
              });

    for (auto const &[key, val] : sortedEnv) {
      bool isCustom = customEnv.count(key);

      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      if (!isCustom) {
        ImGui::TextDisabled("(locked) %s", key.c_str());
      } else {
        ImGui::Text("%s", key.c_str());
      }

      ImGui::TableSetColumnIndex(1);
      ImGui::TextWrapped("%s", val.c_str());

      ImGui::TableSetColumnIndex(2);
      if (isCustom) {
        std::string label = "unset##" + key;
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.1f, 0.1f, 1.0f));
        if (ImGui::Button(label.c_str(), ImVec2(-1, 0))) {
          customEnv.erase(key);
          envLoaded_ = false;
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

} // namespace rsjfw
