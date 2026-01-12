#include "gui/views/dxvk_view.h"
#include "config.h"
#include "downloader/github_client.h"
#include <imgui.h>
#include <cstring>
#include <thread>

namespace rsjfw {

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

DxvkView::DxvkView() {}

void DxvkView::render() {
    auto& src = Config::instance().getGeneral().dxvkSource;
    ImGui::Text("dxvk configuration");
    ImGui::TextDisabled("vulkan-based d3d11 implementation for linux.");
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 10));
    
    if (src.repo != lastRepo_) {
        refreshVersions(src.repo);
        lastRepo_ = src.repo;
    }

    const char* presets[] = { "doitsujin/dxvk", "pythonlover02/DXVK-Sarek" };
    static int currentPreset = -1;
    ImGui::Text("preset repo");
    ImGui::SameLine(180);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 10);
    if (ImGui::Combo("##DxvkPreset", &currentPreset, presets, 2)) {
        src.repo = presets[currentPreset];
    }

    ImGui::Text("repository");
    ImGui::SameLine(180);
    InputTextString("##DxvkRepo", src.repo, ImGui::GetContentRegionAvail().x - 10);
    
    {
        std::lock_guard<std::mutex> lock(mtx_);
        if (fetching_) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1, 1, 0, 1), "checking...");
        } else if (!repoValid_) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1, 0, 0, 1), "error");
        }
    }

    ImGui::Text("version");
    ImGui::SameLine(180);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 10);
    if (ImGui::BeginCombo("##DxvkVer", src.version.c_str())) {
        if (ImGui::Selectable("latest", src.version == "latest")) {
            src.version = "latest";
            src.asset = "";
        }
        
        std::lock_guard<std::mutex> lock(mtx_);
        for (const auto& r : releases_) {
            if (ImGui::Selectable(r.tag.c_str(), src.version == r.tag)) {
                src.version = r.tag;
                src.asset = "";
            }
        }
        ImGui::EndCombo();
    }

    if (src.version != "latest") {
        const downloader::GithubRelease* selectedRel = nullptr;
        {
            std::lock_guard<std::mutex> lock(mtx_);
            for(const auto& r : releases_) {
                if (r.tag == src.version) {
                    selectedRel = &r;
                    break;
                }
            }
        }

        if (selectedRel) {
            ImGui::Text("release file");
            ImGui::SameLine(180);
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 10);
            if (src.asset.empty() && !selectedRel->assets.empty()) {
                for(const auto& a : selectedRel->assets) {
                    if (a.name.find(".tar") != std::string::npos) {
                        src.asset = a.name;
                        break;
                    }
                }
            }

            if (ImGui::BeginCombo("##DxvkAsset", src.asset.c_str())) {
                for (const auto& a : selectedRel->assets) {
                    if (a.name.find(".tar") == std::string::npos) continue; 
                    bool isSelected = (src.asset == a.name);
                    if (ImGui::Selectable(a.name.c_str(), isSelected)) {
                        src.asset = a.name;
                    }
                    if (isSelected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
        }
    }
    
    ImGui::Dummy(ImVec2(0, 10));
    ImGui::TextDisabled("installed: %s", src.installedRoot.empty() ? "none" : src.installedRoot.c_str());
    
    if (ImGui::Button("force reinstall")) {
        src.installedRoot = "";
    }
    
    ImGui::Dummy(ImVec2(0, 50));
}

void DxvkView::refreshVersions(const std::string& repo) {
    fetching_ = true;
    releases_.clear();

    std::thread([this, repo]() {
        bool valid = downloader::GithubClient::isValidRepo(repo);
        std::vector<downloader::GithubRelease> rels;
        if (valid) {
            auto rawRels = downloader::GithubClient::fetchReleases(repo);
            for (const auto& r : rawRels) {
                bool hasArchive = false;
                for (const auto& asset : r.assets) {
                    if (asset.name.find(".tar.gz") != std::string::npos || 
                        asset.name.find(".tar.xz") != std::string::npos) {
                        hasArchive = true;
                        break;
                    }
                }
                if (hasArchive) rels.push_back(r);
            }
        }

        std::lock_guard<std::mutex> lock(mtx_);
        releases_ = std::move(rels);
        repoValid_ = valid;
        fetching_ = false;
    }).detach();
}

}
