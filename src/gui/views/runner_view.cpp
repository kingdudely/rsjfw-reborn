#include "gui/views/runner_view.h"
#include "config.h"
#include "downloader/github_client.h"
#include "discovery_manager.h"
#include <imgui.h>
#include <ImGuiFileDialog.h>
#include <cstring>
#include <thread>
#include <vector>
#include <string>

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

RunnerView::RunnerView() {}

void RunnerView::render() {
    auto& gen = Config::instance().getGeneral();
    
    ImGui::Text("runner architecture");
    ImGui::TextDisabled("select and configure your execution environment.");
    ImGui::Dummy(ImVec2(0, 10));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 20));

    const char* runners[] = { "wine", "proton" };
    int currentRunner = 0;
    if (gen.runnerType == "Proton") currentRunner = 1;
    
    ImGui::Text("environment type");
    ImGui::SameLine(180);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 10);
    if (ImGui::BeginCombo("##RunnerType", runners[currentRunner])) {
        for (int i = 0; i < 2; i++) {
            bool isSelected = (currentRunner == i);
            if (ImGui::Selectable(runners[i], isSelected)) {
                gen.runnerType = (i == 1) ? "Proton" : "Wine";
            }
            if (isSelected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    
    ImGui::Dummy(ImVec2(0, 10));
    ImGui::Text("execution wrapper");
    ImGui::SameLine(180);
    InputTextString("##Wrapper", gen.launchWrapper, ImGui::GetContentRegionAvail().x - 10);
    
    ImGui::SetCursorPosX(180);
    ImGui::BeginGroup();
    if (ImGui::Button("gamemode", ImVec2(100, 25))) gen.launchWrapper = "gamemoderun";
    ImGui::SameLine();
    if (ImGui::Button("gamescope", ImVec2(100, 25))) gen.launchWrapper = "gamescope -W 1920 -H 1080 -r 144 --";
    ImGui::EndGroup();
    
    ImGui::Dummy(ImVec2(0, 10));
    ImGui::Checkbox("enable mangohud", &gen.enableMangoHud);
    ImGui::SameLine();
    ImGui::Checkbox("enable webview2", &gen.enableWebView2);
    
    ImGui::Dummy(ImVec2(0, 20));
    
    if (gen.runnerType == "Proton") renderProtonConfig();
    else renderWineConfig();
}

void RunnerView::renderWineConfig() {
    auto& src = Config::instance().getGeneral().wineSource;
    ImGui::Text("wine settings");
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 10));

    if (ImGui::RadioButton("fetch from github", !src.useCustomRoot)) src.useCustomRoot = false;
    ImGui::SameLine();
    if (ImGui::RadioButton("system / static path", src.useCustomRoot)) src.useCustomRoot = true;

    ImGui::Dummy(ImVec2(0, 10));

    if (!src.useCustomRoot) {
        if (src.repo != lastWineRepo_) {
            refreshVersions(src.repo, false);
            lastWineRepo_ = src.repo;
        }

        const char* presets[] = { "vinegarhq/wine-builds", "GloriousEggroll/wine-ge-custom" };
        static int currentPreset = -1;
        ImGui::Text("preset repo");
        ImGui::SameLine(180);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 10);
        if (ImGui::Combo("##WinePreset", &currentPreset, presets, 2)) {
            src.repo = presets[currentPreset];
        }

        ImGui::Text("repository");
        ImGui::SameLine(180);
        InputTextString("##WineRepo", src.repo, ImGui::GetContentRegionAvail().x - 10);
        
        {
            std::lock_guard<std::mutex> lock(mtx_);
            if (fetchingWine_) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(1, 1, 0, 1), "checking...");
            } else if (!wineRepoValid_) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(1, 0, 0, 1), "error");
            }
        }

        ImGui::Text("version");
        ImGui::SameLine(180);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 10);
        if (ImGui::BeginCombo("##WineVer", src.version.c_str())) {
            if (ImGui::Selectable("latest", src.version == "latest")) {
                src.version = "latest";
                src.asset = "";
            }
            std::lock_guard<std::mutex> lock(mtx_);
            for (const auto& r : wineReleases_) {
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
                for(const auto& r : wineReleases_) {
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

                if (ImGui::BeginCombo("##WineAsset", src.asset.c_str())) {
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

    } else {
        ImGui::Text("local wine path");
        ImGui::SameLine(180);
        
        static bool scanDone = false;
        if (!scanDone) {
            DiscoveryManager::instance().scan();
            scanDone = true;
        }
        
        auto runners = DiscoveryManager::instance().getRunners(RunnerType::System);
        std::string currentSelection = src.customRootPath;
        if (currentSelection.empty()) currentSelection = "select a runner...";
        std::string previewValue = currentSelection;
        bool known = false;
        for (const auto& r : runners) {
            if (r.path.string() == src.customRootPath) {
                previewValue = r.name + " (" + r.version + ")";
                known = true;
                break;
            }
        }
        if (!known && !src.customRootPath.empty()) previewValue = src.customRootPath;

        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 60);
        if (ImGui::BeginCombo("##WineLocal", previewValue.c_str())) {
            for (const auto& r : runners) {
                std::string label = r.name + " (" + r.version + ")";
                bool isSelected = (src.customRootPath == r.path.string());
                if (ImGui::Selectable(label.c_str(), isSelected)) {
                    src.customRootPath = r.path.string();
                }
                if (isSelected) ImGui::SetItemDefaultFocus();
            }
            ImGui::Separator();
            if (ImGui::Selectable("custom path...", false)) {
                ImGuiFileDialog::Instance()->OpenDialog("ChooseWineRoot", "select wine root", nullptr, ".", 1, nullptr, ImGuiFileDialogFlags_Modal);
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        if (ImGui::Button("...", ImVec2(40, 0))) {
             ImGuiFileDialog::Instance()->OpenDialog("ChooseWineRoot", "select wine root", nullptr, ".", 1, nullptr, ImGuiFileDialogFlags_Modal);
        }

        if (ImGuiFileDialog::Instance()->Display("ChooseWineRoot", ImGuiWindowFlags_NoCollapse, ImVec2(400, 600))) {
            if (ImGuiFileDialog::Instance()->IsOk()) {
                src.customRootPath = ImGuiFileDialog::Instance()->GetFilePathName();
            }
            ImGuiFileDialog::Instance()->Close();
        }
    }
    
    ImGui::Dummy(ImVec2(0, 50));
}

void RunnerView::renderProtonConfig() {
    auto& src = Config::instance().getGeneral().protonSource;
    ImGui::Text("proton settings");
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 10));

    if (ImGui::RadioButton("fetch from github##P", !src.useCustomRoot)) src.useCustomRoot = false;
    ImGui::SameLine();
    if (ImGui::RadioButton("steam / static path##P", src.useCustomRoot)) src.useCustomRoot = true;

    ImGui::Dummy(ImVec2(0, 10));

    if (!src.useCustomRoot) {
        if (src.repo != lastProtonRepo_) {
            refreshVersions(src.repo, true);
            lastProtonRepo_ = src.repo;
        }

        const char* presets[] = { "GloriousEggroll/proton-ge-custom" };
        static int currentPreset = -1;
        ImGui::Text("preset repo");
        ImGui::SameLine(180);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 10);
        if (ImGui::Combo("##ProtonPreset", &currentPreset, presets, 1)) {
            src.repo = presets[currentPreset];
        }

        ImGui::Text("repository");
        ImGui::SameLine(180);
        InputTextString("##ProtonRepo", src.repo, ImGui::GetContentRegionAvail().x - 10);

        {
            std::lock_guard<std::mutex> lock(mtx_);
            if (fetchingProton_) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(1, 1, 0, 1), "checking...");
            } else if (!protonRepoValid_) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(1, 0, 0, 1), "error");
            }
        }

        ImGui::Text("version");
        ImGui::SameLine(180);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 10);
        if (ImGui::BeginCombo("##ProtonVer", src.version.c_str())) {
            if (ImGui::Selectable("latest", src.version == "latest")) {
                src.version = "latest";
                src.asset = "";
            }
            std::lock_guard<std::mutex> lock(mtx_);
            for (const auto& r : protonReleases_) {
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
                for(const auto& r : protonReleases_) {
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

                if (ImGui::BeginCombo("##ProtonAsset", src.asset.c_str())) {
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

    } else {
        ImGui::Text("local proton path");
        ImGui::SameLine(180);
        
        static bool scanDoneP = false;
        if (!scanDoneP) {
            DiscoveryManager::instance().scan(); 
            scanDoneP = true;
        }
        
        auto runners = DiscoveryManager::instance().getRunners(RunnerType::Proton);
        std::string currentSelection = src.customRootPath;
        std::string previewValue = currentSelection;
        bool known = false;
        for (const auto& r : runners) {
            if (r.path.string() == src.customRootPath) {
                previewValue = r.name;
                known = true;
                break;
            }
        }
        if (!known && !src.customRootPath.empty()) previewValue = src.customRootPath;
        if (previewValue.empty()) previewValue = "select proton version...";

        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 60);
        if (ImGui::BeginCombo("##ProtonLocal", previewValue.c_str())) {
            for (const auto& r : runners) {
                bool isSelected = (src.customRootPath == r.path.string());
                if (ImGui::Selectable(r.name.c_str(), isSelected)) {
                    src.customRootPath = r.path.string();
                }
                if (isSelected) ImGui::SetItemDefaultFocus();
            }
            ImGui::Separator();
            if (ImGui::Selectable("custom path...", false)) {
                ImGuiFileDialog::Instance()->OpenDialog("ChooseProtonRoot", "select proton root", nullptr, ".", 1, nullptr, ImGuiFileDialogFlags_Modal);
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        if (ImGui::Button("...##PBtn", ImVec2(40, 0))) {
             ImGuiFileDialog::Instance()->OpenDialog("ChooseProtonRoot", "select proton root", nullptr, ".", 1, nullptr, ImGuiFileDialogFlags_Modal);
        }

        if (ImGuiFileDialog::Instance()->Display("ChooseProtonRoot", ImGuiWindowFlags_NoCollapse, ImVec2(400, 600))) {
            if (ImGuiFileDialog::Instance()->IsOk()) {
                src.customRootPath = ImGuiFileDialog::Instance()->GetFilePathName();
            }
            ImGuiFileDialog::Instance()->Close();
        }
    }

    ImGui::Dummy(ImVec2(0, 10));
    auto& gen = Config::instance().getGeneral();
    ImGui::Checkbox("enable fsync", &gen.enableFsync);
    ImGui::SameLine();
    ImGui::Checkbox("enable esync", &gen.enableEsync);
    
    ImGui::Dummy(ImVec2(0, 50));
}

void RunnerView::refreshVersions(const std::string& repo, bool isProton) {
    {
        std::lock_guard<std::mutex> lock(mtx_);
        if (isProton) {
            fetchingProton_ = true;
            protonReleases_.clear();
        } else {
            fetchingWine_ = true;
            wineReleases_.clear();
        }
    }

    std::thread([this, repo, isProton]() {
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
        if (isProton) {
            protonReleases_ = std::move(rels);
            protonRepoValid_ = valid;
            fetchingProton_ = false;
        } else {
            wineReleases_ = std::move(rels);
            wineRepoValid_ = valid;
            fetchingWine_ = false;
        }
    }).detach();
}

}
